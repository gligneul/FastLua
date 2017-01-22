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

/* Internal types */
typedef struct JitState JitState;
typedef struct JitRegister JitRegister;

/* Containers */
TSCC_IMPL_VECTOR_WA(JitRTInfoVector, fljit_rtvec_, union JitRTInfo,
    struct lua_State *, luaM_realloc_)

TSCC_DECL_HASHTABLE_WA(JitRegTable, regtab_, int, JitRegister *,
    struct lua_State *)
TSCC_IMPL_HASHTABLE_WA(JitRegTable, regtab_, int, JitRegister *,
    tscc_int_hashfunc, tscc_general_compare, struct lua_State *, luaM_realloc_)
#define regtab_foreach(h, k, v, cmd) \
    TSCC_HASH_FOREACH(JitRegTable, regtab_, h, int, k, JitRegister *, v, cmd)

/* The first basic block is the loop entry and the second is the loop body. */
#define BBLOCK_ENTRY 0
#define BBLOCK_LOOP 1

/* IRFunction implict parameter. */
#define _irfunc (J->irfunc)

/* Store the jit compilation state. */
struct JitState {
  lua_State *L;
  JitTrace *tr;
  IRFunction *irfunc;
  IRValue lstate;
  IRValue ci;
  IRValue base;
  JitRegTable *regtable;
  l_mem pc;
};

/* Information about a Lua stack register. */
struct JitRegister {
  IRValue value;
  IRValue type;
  IRValue phivalue;
  IRValue phitype;
};

/* Create the jit state. */
static JitState *createjitstate(lua_State *L, JitTrace *tr) {
  JitState *J = luaM_new(L, JitState);
  J->L = L;
  J->tr = tr;
  J->irfunc = ir_create(L);
  J->lstate = NULL;
  J->ci = NULL;
  J->base = NULL;
  J->regtable = regtab_createwa(16, L);
  J->pc = tr->start - tr->p->code;
  return J;
}

/* Destroy the jit state. */
static void destroyjitstate(JitState *J) {
  ir_destroy(J->irfunc);
  regtab_foreach(J->regtable, _, reg, luaM_free(J->L, reg));
  regtab_destroy(J->regtable);
  luaM_free(J->L, J);
}

/* Create a jit register and add it to the register's table. */
static JitRegister *createregister(JitState *J, int reg, IRValue value,
                                   IRValue type) {
  JitRegister *r = luaM_new(J->L, JitRegister);
  r->value = value;
  r->type = type;
  r->phivalue = r->phitype = NULL;
  regtab_insert(J->regtable, reg, r);
  return r;
}

/* Create the entry basic block.
 * This block should contain the loop invariants. */
static void initentryblock(JitState *J) {
  ir_addbblock();
  J->lstate = ir_getarg(IR_INTPTR, 0);
  J->ci = ir_getarg(IR_INTPTR, 1);
  J->base = ir_getarg(IR_INTPTR, 2);
}

/* Obtain the next instruction position. */
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

/* Convert the lua type to ir type. */
static enum IRType converttype(int type) {
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

/* Convert the lua binary operation to ir command type. */
static enum IRCommandType convertbinop(int op) {
  switch (op) {
    case OP_ADD:
      return IR_ADD;
    default:
      assert(0);
      break;
  }
  return 0;
}

/* Create a guard instruction that verifies if the type matches what is
 * expected. 
 * The stack should only be restored if the guard fail inside the loop. */
static void guardtype(JitState *J, IRValue type, int expectedtype,
                      int restorestack) {
  IRBBlock *originalbb = ir_currbblock();
  IRBBlock *sideexit = ir_addbblock();
  if (restorestack) {
    /* TODO */
  }
  ir_return(ir_consti(1)); 
  ir_currbblock() = originalbb;
  ir_jne(type, ir_consti(expectedtype), sideexit);
}

/* Load a register from the Lua stack and verify if it matches the expected
 * type. Registers are loaded in the entry block. */
static IRValue gettvaluer(JitState *J, int regpos, int expectedtype) {
  IRValue value;
  enum IRType irtype = converttype(expectedtype);
  if (!regtab_contains(J->regtable, regpos)) {
    /* No information about the register was found, so load it from the stack
     * at the entry block. */
    size_t offset = sizeof(TValue) * regpos;
    IRValue addr, type;
    ir_currbblock() = ir_getbblock(BBLOCK_ENTRY);
    if (offset != 0)
      addr = ir_binop(IR_ADD, J->base, ir_consti(offset));
    else
      addr = J->base;
    type = ir_loadfield(IR_INT, addr, TValue, tt_);
    guardtype(J, type, expectedtype, 0);
    value = ir_loadfield(irtype, addr, TValue, value_);
    createregister(J, regpos, value, type);
    ir_currbblock() = ir_getbblock(BBLOCK_LOOP);
  }
  else {
    /* TODO: verify if the current loadedtype is correct */
    JitRegister *r = regtab_get(J->regtable, regpos, NULL);
    value = r->value;
  }
  return value;
}

/* Load a constant from the constant table. */
static IRValue gettvaluek(JitState *J, int kpos, int expectedtype) {
  TValue *k = J->tr->p->k + kpos;
  assert(expectedtype == ttype(k));
  switch (expectedtype) {
    case LUA_TNUMFLT: return ir_constf(fltvalue(k));
    case LUA_TNUMINT: return ir_consti(ivalue(k));
    default: assert(0); break;
  }
  return NULL;
}

/* Load a constant or register given the position. */
static IRValue gettvalue(JitState *J, int pos, int expectedtype) {
  if (ISK(pos))
    return gettvaluek(J, INDEXK(pos), expectedtype);
  else
    return gettvaluer(J, pos, expectedtype);
}

/* Add a phi at the begining of the loop basic block and replace the old value.
 */
static IRValue insertphivalue(JitState *J, IRValue entryval, IRValue loopval) {
  size_t to, from;
  IRBBlock *entry = ir_getbblock(BBLOCK_ENTRY);
  IRBBlock *loop = ir_getbblock(BBLOCK_LOOP);
  IRValue phi = ir_phi(entryval->type);
  from = ir_cmdvec_size(loop->cmds) - 1;
  ir_addphinode(phi, entryval, entry);
  ir_addphinode(phi, loopval, loop);
  /* find the last phi in the loop block */
  for (to = 0; to < ir_cmdvec_size(loop->cmds); ++to)
    if (ir_cmdvec_get(loop->cmds, to)->cmdtype != IR_PHI)
      break;
  ir_move(loop, from, to);
  ir_replacevalue(loop, entryval, phi);
  return phi;
}

/* Define the Lua register value. */
static void settvalue(JitState *J, int reg, int luatype, IRValue value) {
  IRBBlock *entry = ir_getbblock(BBLOCK_ENTRY);
  JitRegister *r = regtab_get(J->regtable, reg, NULL);
  IRValue type = ir_consti(luatype);
  if (!r) {
    /* No information about the register was found, so create it */
    createregister(J, reg, value, type);
  }
  else if (r->value->bblock == entry) {
    /* The saved value is from the entry block, so create a phi and replace the
     * old value. */
    r->phivalue = insertphivalue(J, r->value, value);
    r->phitype = insertphivalue(J, r->type, type);
  }
  else {
    /* The saved value is from the loop block */
    if (r->phivalue) {
      /* Update the phi if it exists */
      ir_phivec_get(r->phivalue->args.phi, BBLOCK_LOOP)->value = value;
      ir_phivec_get(r->phitype->args.phi, BBLOCK_LOOP)->value = type;
    }
    /* Update the saved value */
    r->value = value;
    r->type = type;
  }
}

/* Auxiliary macros for obtaining the Lua's tvalues. */
#define getrkb(J, i, rt) gettvalue(J, GETARG_B(i), rt.binop.rb)
#define getrkc(J, i, rt) gettvalue(J, GETARG_C(i), rt.binop.rc)

/*
 * Compiles a single opcode.
 */
static void compilebytecode(JitState *J, int n) {
  Proto *p = J->tr->p;
  Instruction i = p->code[J->pc];
  union JitRTInfo rt = fljit_rtvec_get(J->tr->rtinfo, n);
  int op = GET_OPCODE(i);
  switch (op) {
    case OP_ADD: {
      IRValue rb = getrkb(J, i, rt);
      IRValue rc = getrkc(J, i, rt);
      // TODO: conversions
      // TOPO: rvalue -> resultingvalue
      int rtype = LUA_TNUMINT;
      IRValue rvalue = ir_binop(convertbinop(op), rb, rc);
      settvalue(J, GETARG_A(i), rtype, rvalue);
      break;
    }
    case OP_FORLOOP: {
      // Check if the loop is the expected type
      // get step
      // get idx
      // get limit
      // if to go back
      // then (update indices)
      // else exit loop (update external pc)
      break;
    }
    default:
      assert(0);
      break;
  }
  J->pc = getnextpc(J->pc, i);
}

JitTrace *fljit_createtrace(struct lua_State *L) {
  JitTrace *tr = luaM_new(L, JitTrace);
  tr->L = L;
  tr->p = NULL;
  tr->start = NULL;
  tr->n = 0;
  tr->rtinfo = fljit_rtvec_createwa(L);
  tr->completeloop = 0;
  return tr;
}

void fljit_destroytrace(JitTrace *tr) {
  fljit_rtvec_destroy(tr->rtinfo);
  luaM_free(tr->L, tr);
}

void fljit_compile(JitTrace *tr) {
  JitState *J = createjitstate(tr->L, tr);
  initentryblock(J);
  if (tr->completeloop) {
    size_t i;
    ir_addbblock(); /* loop bblock */
    for (i = 0; i < tr->n; ++i)
      compilebytecode(J, i);
    /* add a jmp from entry to loop block */
    ir_currbblock() = ir_getbblock(BBLOCK_ENTRY);
    ir_jmp(ir_getbblock(BBLOCK_LOOP));
  } else {
    assert(0);
  }
  ir_print();
  destroyjitstate(J);
}

