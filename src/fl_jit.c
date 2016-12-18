/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2016 Gabriel de Quadros Ligneul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>

#include "lprefix.h"

#include "lmem.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_ir.h"
#include "fl_jit.h"

/*
 * The first basic block is the loop preamble and the second is the loop body.
 */
#define BBLOCK_ENTRY 0
#define BBLOCK_LOOP 1

/*
 * Stores the jit compilation state.
 */
typedef struct JitState {
  lua_State *L;
  TraceRec *tr;
  IRFunction *F;
  IRValue lstate;
  IRValue ci;
  IRValue base;
  IRValue *rvalues;
  IRValue *rtypes;
  l_mem pc;
} JitState;

/*
 * Creates/destroy the jit state.
 */
static JitState *createjitstate(lua_State *L, TraceRec *tr) {
  int i;
  JitState *J = luaM_new(L, JitState);
  J->L = L;
  J->tr = tr;
  J->F = flI_createfunc(L);
  J->lstate = IRNullValue;
  J->ci = IRNullValue;
  J->base = IRNullValue;
  J->rvalues = luaM_newvector(L, tr->p->maxstacksize, IRValue);
  J->rtypes = luaM_newvector(L, tr->p->maxstacksize, IRValue);
  for (i = 0; i < tr->p->maxstacksize; ++i)
    J->rvalues[i] = J->rtypes[i] = IRNullValue;
  J->pc = tr->start - tr->p->code;
  return J;
}

static void destroyjitstate(JitState *J) {
  flI_destroyfunc(J->F);
  luaM_freearray(J->L, J->rvalues, J->tr->p->maxstacksize);
  luaM_freearray(J->L, J->rtypes, J->tr->p->maxstacksize);
  luaM_free(J->L, J);
}

/*
 * Creates the entry basic block.
 * This block contains the trace (loop) invariants.
 */
static void initentryblock(JitState *J) {
  IRFunction *F = J->F;
  flI_createbb(F);
  J->lstate = flI_getarg(F, IR_INTPTR, 0);
  J->ci = flI_getarg(F, IR_INTPTR, 1);
  J->base = flI_getarg(F, IR_INTPTR, 2);
}

/*
 * Create stubs that can be replaced by phi values later.
 */
static void createstubs(JitState *J) {
  int i;
  int nstubs = J->tr->p->maxstacksize * 2;
  for (i = 0; i < nstubs; ++i)
    flI_stub(J->F);
}

/*
 * Obtains the next instruction position.
 */
static l_mem getnextpc(l_mem oldpc, Instruction i) {
  l_mem pc = oldpc;
  switch (GET_OPCODE(i)) {
    case OP_FORLOOP:
      pc += GETARG_sBx(i) + 1;
      break;
    default:
      pc += 1;
      break;
  }
  return pc;
}

/*
 * Converts from lua type to ir type.
 */
static int converttype(int type) {
  switch (type) {
    case LUA_TNUMFLT:
      return IR_LUAFLT;
    case LUA_TNUMINT:
      return IR_LUAINT;
    default:
      assert(0);
      break;
  }
  return 0;
}

/*
 * Converts the binary operation to the equivalente in the IR.
 */
static int convertbinop(int op) {
  switch (op) {
    case OP_ADD:
      return IR_ADD;
    default:
      assert(0);
      break;
  }
  return 0;
}

/*
 * Obtains a constant/register in the lua stack.
 */
static IRValue gettvaluer(JitState *J, int v, int expectedtype) {
  IRFunction *F = J->F;
  IRValue *savedvalue = J->rvalues + v;
  IRValue *savedtype = J->rtypes + v;
  if (flI_isnullvalue(*savedvalue)) {
    IRValue offset, addr;
    flI_setcurrbb(F, BBLOCK_ENTRY);
    offset = flI_consti(F, sizeof(TValue) * v);
    addr = flI_binop(F, IR_ADD, J->base, offset);
    *savedvalue = flI_loadfield(F, expectedtype, addr, TValue, value_);
    // TODO: verify if the loadedtype is correct
    *savedtype = flI_loadfield(F, IR_INT, addr, TValue, tt_);
    flI_setcurrbb(F, BBLOCK_LOOP);
  } else {
    // TODO: verify if the loadedtype is correct
  }
  return *savedvalue;
}

static IRValue gettvaluek(JitState *J, int v, int expectedtype) {
  TValue *k = J->tr->p->k + v;
  switch (ttype(k)) {
    case LUA_TNUMFLT:
      assert(expectedtype == IR_LUAFLT);
      return flI_constf(J->F, fltvalue(k));
    case LUA_TNUMINT:
      assert(expectedtype == IR_LUAINT);
      return flI_consti(J->F, ivalue(k));
    default:
      assert(0);
      break;
  }
  return IRNullValue;
}

static IRValue gettvalue(JitState *J, int v, int expectedtype) {
  if (ISK(v))
    return gettvaluek(J, INDEXK(v), expectedtype);
  else
    return gettvaluer(J, v, expectedtype);
}

/*
 * Add a phi at the begining of the loop basic block and also replace the usage
 * of the old value in this block. Returns the phi value.
 */
static IRValue insertphivalue(JitState *J, int stubpos, IRValue entryval,
    IRValue loopval) {
  IRValue phi = flI_phi(J->F, entryval, loopval);
  IRValue stub = {BBLOCK_LOOP, stubpos};
  flI_swapvalues(J->F, phi, stub);
  flI_replacevalue(J->F, BBLOCK_LOOP, entryval, stub);
  return phi;
}

/*
 * Defines the lua register value.
 */
static void settvalue(JitState *J, int reg, int type, IRValue value) {
  IRFunction *F = J->F;
  IRValue *savedvalue = J->rvalues + reg;
  IRValue *savedtype = J->rtypes + reg;
  IRValue irtype = flI_consti(F, type);
  if (flI_isnullvalue(*savedvalue)) {
    *savedvalue = value;
    *savedtype = irtype;
  } else if (flI_valuegetbb(*savedvalue) == BBLOCK_ENTRY) {
    /* create a phi value */
    *savedvalue = insertphivalue(J, reg * 2, *savedvalue, value);
    *savedtype = insertphivalue(J, reg * 2 + 1, *savedtype, irtype);
  } else {
    /* replace the value of the loop block */
    IRCommand *valuecmd = flI_getcmd(F, *savedvalue);
    IRCommand *typecmd = flI_getcmd(F, *savedtype);
    if (valuecmd->cmdtype == IR_PHI) {
      valuecmd->args.phi.loop = value;
      typecmd->args.phi.loop = irtype;
    } else {
      *savedvalue = value;
      *savedtype = irtype;
    }
  }
}

/*
 * Auxiliary macros for obtaining the Lua's tvalues.
 */
#define getrkb(J, i, rt) \
  gettvalue(J, GETARG_B(i), converttype(rt->binoptypes.rb))
#define getrkc(J, i, rt) \
  gettvalue(J, GETARG_C(i), converttype(rt->binoptypes.rc))

/*
 * Compiles a single opcode.
 */
static void compilebytecode(JitState *J, int n) {
  IRFunction *F = J->F;
  Proto *p = J->tr->p;
  Instruction i = p->code[J->pc];
  RuntimeRec *rt = J->tr->rt + n;
  int op = GET_OPCODE(i);
  switch (op) {
    case OP_ADD: {
      IRValue rb = getrkb(J, i, rt);
      IRValue rc = getrkc(J, i, rt);
      // TODO conversions
      int rtype = LUA_TNUMINT;
      IRValue rvalue = flI_binop(F, convertbinop(op), rb, rc);
      settvalue(J, GETARG_A(i), rtype, rvalue);
      break;
    }
    case OP_FORLOOP: {
      break;
    }
    default:
      assert(0);
      break;
  }
  J->pc = getnextpc(J->pc, i);
}

TraceRec *flJ_createtracerec(struct lua_State *L) {
  TraceRec *tr = luaM_new(L, TraceRec);
  tr->p = NULL;
  tr->start = NULL;
  tr->n = 0;
  tr->rt = NULL;
  tr->rtsize = 0;
  tr->completeloop = 0;
  return tr;
}

void flJ_destroytracerec(struct lua_State *L, TraceRec *tr) {
  luaM_freearray(L, tr->rt, tr->rtsize);
  luaM_free(L, tr);
}

void flJ_compile(struct lua_State *L, TraceRec *tr) {
  JitState *J = createjitstate(L, tr);
  IRFunction *F = J->F;
  initentryblock(J);
  if (tr->completeloop) {
    int i;
    flI_createbb(F);
    createstubs(J);
    for (i = 0; i < tr->n; ++i)
      compilebytecode(J, i);
  } else {
    assert(0);
  }
  flI_print(J->F);
  destroyjitstate(J);
}

