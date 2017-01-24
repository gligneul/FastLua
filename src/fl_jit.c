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

/* IRFunction implict parameter. */
#define _irfunc (J->irfunc)

/* Store the jit compilation state. */
struct JitState {
  lua_State *L;
  JitTrace *tr;
  IRFunction *irfunc;
  IRBBlockVector *entry;
  IRBBlockVector *loop;
  IRBBlock *earlyexit;
  IRBBlock *loopexit;
  IRValue *lstate;
  IRValue *ci;
  IRValue *base;
  JitRegTable *regtable;
  l_mem pc;
};

/* Information about a Lua stack register. */
struct JitRegister {
  IRValue *value;
  IRValue *type;
  IRValue *phivalue;
  IRValue *phitype;
  IRValue *loadedvalue;
  IRValue *loadedtype;
  int loadedfromstack;
};

/* Create the jit state. */
static JitState *createjitstate(lua_State *L, JitTrace *tr) {
  JitState *J = luaM_new(L, JitState);
  J->L = L;
  J->tr = tr;
  J->irfunc = ir_create(L);
  J->entry = ir_bbvec_createwa(L);
  J->loop = ir_bbvec_createwa(L);
  J->earlyexit = NULL;
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
  ir_bbvec_destroy(J->entry);
  ir_bbvec_destroy(J->loop);
  regtab_foreach(J->regtable, _, reg, luaM_free(J->L, reg));
  regtab_destroy(J->regtable);
  luaM_free(J->L, J);
}

/* Create a jit register and add it to the register's table. */
static JitRegister *createregister(JitState *J, int reg, IRValue *value,
                                   IRValue *type, int loadedfromstack) {
  JitRegister *r = luaM_new(J->L, JitRegister);
  r->value = value;
  r->type = type;
  r->phivalue = r->phitype = NULL;
  if (loadedfromstack) {
    r->loadedvalue = r->value;
    r->loadedtype = r->type;
  } else {
    r->loadedvalue = r->loadedtype = NULL;
  }
  r->loadedfromstack = loadedfromstack;
  regtab_insert(J->regtable, reg, r);
  return r;
}

/* Create the entry basic block.
 * This block should contain the loop invariants. */
static void initentryblock(JitState *J) {
  ir_bbvec_push(J->entry, ir_currbblock() = ir_addbblock());
  J->lstate = ir_getarg(IR_IPTR, 0);
  J->ci = ir_getarg(IR_IPTR, 1);
  J->base = ir_getarg(IR_IPTR, 2);
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
    case LUA_TNUMFLT: return IR_FLOAT;
    case LUA_TNUMINT: return IR_LUAINT;
    default: assert(0); break;
  }
  return 0;
}

/* Convert the lua binary operation to the ir binop. */
static enum IRBinOp convertbinop(int op) {
  switch (op) {
    case OP_ADD: return IR_ADD;
    default: assert(0); break;
  }
  return 0;
}

/* Obtain the address of a Lua stack value */
static IRValue *gettvalueaddr(JitState *J, int regpos) {
  size_t offset = sizeof(TValue) * regpos;
  if (offset != 0)
    return ir_binop(IR_ADD, J->base, ir_consti(offset));
  else
    return J->base;
}

/* Create a guard instruction that verifies if the type matches what is
 * expected. 
 * The stack should only be restored if the guard fail inside the loop. */
static void guardtype(JitState *J, IRValue *type, int expectedtype,
                      IRBBlockVector *bbs) {
  IRBBlock *original = ir_currbblock();
  IRBBlock *sideexit = NULL;
  IRBBlock *continuation = ir_insertbblock(original);
  ir_bbvec_push(bbs, continuation);
  if (bbs == J->loop) {
    /* TODO */
    assert(0);
  }
  else {
    if (!J->earlyexit) {
      J->earlyexit = ir_currbblock() = ir_addbblock();
      ir_return(ir_consti(1)); 
    }
    sideexit = J->earlyexit;
  }
  ir_currbblock() = original;
  ir_cmp(IR_NE, type, ir_consti(expectedtype), sideexit, continuation);
  ir_currbblock() = continuation;
}

/* Load a register from the Lua stack and verify if it matches the expected
 * type. Registers are loaded in the entry block. */
static IRValue *gettvaluer(JitState *J, int regpos, int expectedtype) {
  IRValue *value;
  enum IRType irtype = converttype(expectedtype);
  if (!regtab_contains(J->regtable, regpos)) {
    /* No information about the register was found, so load it from the Lua
     * stack in the entry block. */
    IRBBlock *oldcurrbblock = ir_currbblock();
    IRValue *type, *addr;
    ir_currbblock() = ir_bbvec_back(J->entry);
    addr = gettvalueaddr(J, regpos);
    type = ir_loadfield(IR_INT, addr, TValue, tt_);
    guardtype(J, type, expectedtype, J->entry);
    value = ir_loadfield(irtype, addr, TValue, value_);
    createregister(J, regpos, value, type, 1);
    ir_currbblock() = oldcurrbblock;
  }
  else {
    /* TODO: verify if the current loadedtype is correct */
    JitRegister *r = regtab_get(J->regtable, regpos, NULL);
    value = r->value;
  }
  return value;
}

/* Load a constant from the constant table. */
static IRValue *gettvaluek(JitState *J, int kpos, int expectedtype) {
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
static IRValue *gettvalue(JitState *J, int pos, int expectedtype) {
  if (ISK(pos))
    return gettvaluek(J, INDEXK(pos), expectedtype);
  else
    return gettvaluer(J, pos, expectedtype);
}

/* Add a phi at the begining of the loop basic block and replace the old value.
 */
static IRValue *insertphivalue(JitState *J, IRValue *entryval) {
  IRBBlock *loopstart = ir_bbvec_front(J->loop);
  IRBBlock *oldcurrbblock = ir_currbblock();
  size_t to = 0, from = ir_valvec_size(loopstart->values);
  ir_currbblock() = loopstart;
  IRValue *phi = ir_phi(entryval->type);
  /* find the last phi in the loop block */
  ir_valvec_foreach(loopstart->values, v, {
    if (v->instr != IR_PHI)
      break;
    else
      to++;
  });
  ir_move(loopstart, from, to);
  ir_replacevalue(loopstart, entryval, phi);
  ir_currbblock() = oldcurrbblock;
  return phi;
}

/* Create the phi nodes for the registers that have phi values. */
static void linkphivalues(JitState *J) {
  IRBBlock *entry = ir_bbvec_back(J->entry);
  IRBBlock *loop = ir_bbvec_back(J->loop);
  regtab_foreach(J->regtable, _, r, {
    if (r->phivalue) {
      ir_addphinode(r->phivalue, r->loadedvalue, entry);
      ir_addphinode(r->phivalue, r->value, loop);
      ir_addphinode(r->phitype, r->loadedtype, entry);
      ir_addphinode(r->phitype, r->type, loop);
    }
  });
}

/* Define the Lua register value. */
static void settvalue(JitState *J, int regpos, IRValue *type, IRValue *value) {
  JitRegister *r;
  if (!regtab_find(J->regtable, regpos, &r)) {
    /* No information about the register was found, so create it */
    createregister(J, regpos, value, type, 0);
  }
  else {
    if (r->loadedfromstack) {
      /* The saved value is from the entry block, so create a phi and replace
       * the old value. */
      r->phivalue = insertphivalue(J, r->value);
      r->phitype = insertphivalue(J, r->type);
      r->loadedfromstack = 0;
    }
    /* Update the saved value */
    r->value = value;
    r->type = type;
  }
}

/* Add the missing jumps in the basic blocks. */
static void addjmps(JitState *J) {
  /* add a jmp from entry to loop block */
  ir_currbblock() = ir_bbvec_back(J->entry);
  ir_jmp(ir_bbvec_front(J->loop));
  /* and a jmp from the loop end to the beginning */
  ir_currbblock() = ir_bbvec_back(J->loop);
  ir_jmp(ir_bbvec_front(J->loop));
}

/* Store the registers back in the Lua stack. */
static void fillloopexit(JitState *J) {
  /* TODO fix: save the latest value that the exit branch can reach */
  /* TODO: fill all side exits */
  ir_currbblock() = J->loopexit;
  regtab_foreach(J->regtable, regpos, r, {
    if (r->phivalue) {
      IRValue *addr = gettvalueaddr(J, regpos);
      IRValue *valueaddr = ir_getfieldaddr(addr, TValue, value_);
      IRValue *typeaddr = ir_getfieldaddr(addr, TValue, tt_);
      ir_store(IR_LUAINT, valueaddr, r->phivalue);
      ir_store(IR_INT, typeaddr, r->phitype);
    }
  });
  ir_return(ir_consti(0));
}

/* Auxiliary macros for obtaining the Lua's tvalues. */
#define getrkb(J, i, rt) gettvalue(J, GETARG_B(i), rt.binop.rb)
#define getrkc(J, i, rt) gettvalue(J, GETARG_C(i), rt.binop.rc)

/*
 * Compiles a single bytecode, given the J->pc.
 */
static void compilebytecode(JitState *J, int n) {
  Proto *p = J->tr->p;
  Instruction i = p->code[J->pc];
  union JitRTInfo rt = fljit_rtvec_get(J->tr->rtinfo, n);
  int op = GET_OPCODE(i);
  switch (op) {
    case OP_ADD: {
      IRValue *rb = getrkb(J, i, rt);
      IRValue *rc = getrkc(J, i, rt);
      /* TODO: type conversions (always performing int operations) */
      IRValue *resulttype = ir_consti(LUA_TNUMINT);
      IRValue *resultvalue = ir_binop(convertbinop(op), rb, rc);
      settvalue(J, GETARG_A(i), resulttype, resultvalue);
      break;
    }
    case OP_FORLOOP: {
      /* TODO: remove unnecessary verifications for idx and limit */
      int ra = GETARG_A(i);
      int expectedtype = rt.forloop.type;
      IRValue *looptype, *idx, *limit, *step, *newidx;
      IRBBlock *currloop = ir_bbvec_front(J->loop);
      IRBBlock *keeplooping = ir_insertbblock(currloop);
      IRBBlock *posstep = ir_insertbblock(currloop);
      IRBBlock *negstep = ir_insertbblock(currloop);
      J->loopexit = ir_addbblock();
      ir_bbvec_push(J->loop, negstep);
      ir_bbvec_push(J->loop, posstep);
      ir_bbvec_push(J->loop, keeplooping);

      idx = gettvalue(J, ra, expectedtype);
      limit = gettvalue(J, ra + 1, expectedtype);
      step = gettvalue(J, ra + 2, expectedtype);
      newidx = ir_binop(IR_ADD, idx, step);
      ir_cmp(IR_LT, step, ir_consti(0), negstep, posstep);

      ir_currbblock() = negstep;
      ir_cmp(IR_GT, newidx, limit, J->loopexit, keeplooping);

      ir_currbblock() = posstep;
      ir_cmp(IR_GT, limit, newidx, J->loopexit, keeplooping);

      ir_currbblock() = keeplooping;
      looptype = ir_consti(expectedtype);
      settvalue(J, ra, looptype, newidx); /* internal index */
      settvalue(J, ra + 3, looptype, newidx); /* external index */
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
    ir_bbvec_push(J->loop, ir_currbblock() = ir_addbblock());
    for (i = 0; i < tr->n; ++i)
      compilebytecode(J, i);
    addjmps(J);
    linkphivalues(J);
    fillloopexit(J);
  } else {
    assert(0);
  }
  ir_print();
  destroyjitstate(J);
}

