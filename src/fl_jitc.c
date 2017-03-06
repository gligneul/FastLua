/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2016 Gabriel de Quadros Ligneul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>

#include "lprefix.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_asm.h"
#include "fl_instr.h"
#include "fl_ir.h"
#include "fl_jitc.h"
#include "fl_logger.h"

/* Internal types */
struct JitRegister;
typedef struct JitExit JitExit;
typedef struct JitState JitState;

/* Containers */
TSCC_IMPL_VECTOR_WA(JitRTInfoVector, fljit_rtvec_, struct JitRTInfo,
    struct lua_State *, luaM_realloc_)

TSCC_DECL_VECTOR_WA(JitExitVector, exvec_, JitExit *, struct lua_State *)
TSCC_IMPL_VECTOR_WA(JitExitVector, exvec_, JitExit *, struct lua_State *,
    luaM_realloc_)
#define exvec_foreach(vec, val, cmd) \
    TSCC_VECTOR_FOREACH(exvec_, vec, JitExit *, val, cmd)

/* IRFunction implict parameter. */
#define _irfunc (J->irfunc)

/* Lua stack register. */
struct JitRegister {
  IRValue *value;
  IRValue *tag;
};

/* Information necessary to build the a jit exit */
struct JitExit {
  IRBBlock *bb;                 /* side exit basic block */
  struct JitRegister *tostore;  /* registers to store */
  int status;                   /* return status */
};

/* Jit compilation state. */
struct JitState {
  lua_State *L;                 /* Lua state */
  JitTrace *tr;                 /* recorded trace */
  IRFunction *irfunc;           /* IR output function */
  IRBBlock *preloop;            /* last basic block before the loop start */
  IRBBlock *loopstart;          /* first block in the loop */
  IRBBlock *loopend;            /* last block in the loop */
  IRBBlock *earlyexit;          /* side exit before the loop started */
  JitExitVector *exits;         /* exits that must restore the lua stack */
  IRValue *lstate;              /* Lua state in the jitted code */
  IRValue *base;                /* Lua stack base */
  int nregisters;               /* number of registers in Lua stack */
  struct JitRegister *current;  /* register's current value */
  struct JitRegister *phi;      /* register's phi value */
  struct JitRegister *loaded;   /* register's loaded from the Lua stack */
  l_mem pc;                     /* current instruction position */
};

/* Create/destroy a jit exit */
static JitExit *createexit(struct lua_State *L, IRBBlock *bb, int nregisters,
                           int status) {
  JitExit *e = luaM_new(L, JitExit);
  e->bb = bb;
  e->tostore = luaM_newvector(L, nregisters, struct JitRegister);
  e->status = status;
  memset(e->tostore, 0, nregisters * sizeof(struct JitRegister));
  return e;
}

static void destroyexit(struct lua_State *L, JitExit *e, int nregisters) {
  luaM_freearray(L, e->tostore, nregisters);
  luaM_free(L, e);
}

/* Create/destroy the jit state. */
static JitState *createjitstate(lua_State *L, JitTrace *tr) {
  JitState *J = luaM_new(L, JitState);
  J->L = L;
  J->tr = tr;
  J->irfunc = ir_create(L);
  J->preloop = J->loopstart = J->loopend = J->earlyexit = NULL;
  J->exits = exvec_createwa(L);
  J->lstate = J->base = NULL;
  J->nregisters = tr->p->maxstacksize;
  J->current = luaM_newvector(L, J->nregisters, struct JitRegister);
  J->phi = luaM_newvector(L, J->nregisters, struct JitRegister);
  J->loaded = luaM_newvector(L, J->nregisters, struct JitRegister);
  memset(J->current, 0, J->nregisters * sizeof(struct JitRegister));
  memset(J->phi, 0, J->nregisters * sizeof(struct JitRegister));
  memset(J->loaded, 0, J->nregisters * sizeof(struct JitRegister));
  return J;
}

static void destroyjitstate(JitState *J) {
  ir_destroy(J->irfunc);
  exvec_foreach(J->exits, e, destroyexit(J->L, e, J->nregisters));
  exvec_destroy(J->exits);
  luaM_freearray(J->L, J->current, J->nregisters);
  luaM_freearray(J->L, J->phi, J->nregisters);
  luaM_freearray(J->L, J->loaded, J->nregisters);
  luaM_free(J->L, J);
}

/* Create the entry basic block.
 * This block should contain the loop invariants. */
static void initentryblock(JitState *J) {
  ir_currbblock() = J->preloop = ir_addbblock();
  J->lstate = ir_getarg(IR_PTR, 0);
  J->base = ir_getarg(IR_PTR, 1);
}

/* Convert a lua tag to an ir type. */
static enum IRType converttag(int tag) {
  switch (tag) {
    case LUA_TNUMFLT: return IR_FLOAT;
    case LUA_TNUMINT: return IR_LUAINT;
    default: fll_error("converttag: unhandled tag"); break;
  }
  return 0;
}

/* Convert the lua binary operation to the ir binop. */
static enum IRBinOp convertbinop(int op) {
  switch (op) {
    case OP_ADD: return IR_ADD;
    default: fll_error("convertbinop: unhandled binop"); break;
  }
  return 0;
}

/* Verify if the tag matches what is expected in the preloop block. */
static void guardtaginpreloop(JitState *J, IRValue *tag, int expectedtag) {
  IRBBlock *continuation = ir_insertbblock(J->preloop);
  if (J->earlyexit == NULL) {
    ir_currbblock() = J->earlyexit = ir_addbblock();
    ir_return(ir_consti(1)); 
  }
  ir_currbblock() = J->preloop;
  ir_cmp(IR_NE, tag, ir_consti(expectedtag), J->earlyexit, continuation);
  ir_currbblock() = J->preloop = continuation;
}

/* Verify if a register has any saved information. */
static int registerhasinfo(JitState *J, int regpos) {
  return J->current[regpos].value != NULL;
}

/* Verify if a register field (value/tag) was loaded from the stack. */
#define loadedfromstack(J, regpos, field) \
    ((J->loaded[regpos].field != NULL) && (J->phi[regpos].field == NULL))

/* Obtain the register tag. */
static int getregistertag(JitState *J, int regpos) {
  return J->current[regpos].tag->args.konst.i;
}

/* Load a register from the Lua stack and verify if it matches the expected
 * tag. Registers are loaded in the entry block. */
static IRValue *getregister(JitState *J, int regpos, int expectedtag,
                            int checktag) {
  struct JitRegister *curr = J->current + regpos;
  enum IRType type = converttag(expectedtag);
  if (!registerhasinfo(J, regpos)) {
    /* No information about the register was found, so load it from the Lua
     * stack in the entry block. */
    int addr = sizeof(TValue) * regpos;
    IRBBlock *oldcurrbblock = ir_currbblock();
    ir_currbblock() = J->preloop;
    if (checktag) {
      IRValue *tag = ir_load(IR_INT, J->base, addr + offsetof(TValue, tt_));
      guardtaginpreloop(J, tag, expectedtag);
    }
    curr->tag = ir_consti(expectedtag);
    curr->value = ir_load(type, J->base, addr + offsetof(TValue, value_));
    J->loaded[regpos] = *curr;
    ir_currbblock() = oldcurrbblock;
  }
  /* TODO: verify if the current tag is correct */
  return curr->value;
}

/* Load a constant from the constant table. */
static IRValue *getconst(JitState *J, int kpos) {
  TValue *k = J->tr->p->k + kpos;
  switch (ttype(k)) {
    case LUA_TNUMFLT: return ir_constf(fltvalue(k));
    case LUA_TNUMINT: return ir_consti(ivalue(k));
    default: fll_error("getconst: unhandled const type"); break;
  }
  return NULL;
}

/* Obtain the constant tag. */
static int getconsttag(JitState *J, int kpos) {
  return ttype((J->tr->p->k + kpos));
}

/* Load a constant or register given the position. */
static IRValue *gettvalue(JitState *J, int pos, int expectedtag,
                          int checktag) {
  if (ISK(pos))
    return getconst(J, INDEXK(pos));
  else
    return getregister(J, pos, expectedtag, checktag);
}

/* Store a Lua stack register */
static void storeregister(JitState *J, int regpos, struct JitRegister r) {
  int addr = sizeof(TValue) * regpos;
  ir_store(IR_LUAINT, J->base, r.value, addr + offsetof(TValue, value_));
  if (r.tag)
    ir_store(IR_INT, J->base, r.tag, addr + offsetof(TValue, tt_));
}

/* Add a phi at the begining of the loop basic block and replace the old value.
 */
static IRValue *insertphivalue(JitState *J, IRValue *entryval) {
  IRBBlock *oldcurrbblock = ir_currbblock();
  size_t to = 0, from = ir_valvec_size(J->loopstart->values);
  IRValue *phi;
  ir_currbblock() = J->loopstart;
  phi = ir_phi(entryval->type);
  /* find the last phi in the loop block */
  ir_valvec_foreach(J->loopstart->values, v, {
    if (v->instr != IR_PHI)
      break;
    else
      to++;
  });
  ir_move(J->loopstart, from, to);
  ir_replacevalue(J->loopstart, entryval, phi);
  ir_currbblock() = oldcurrbblock;
  return phi;
}

/* Create the phi nodes for the registers that have phi values. */
static void linkphivalues(JitState *J) {
  int i;
  for (i = 0; i < J->nregisters; ++i) {
    if (J->phi[i].value) {
      ir_addphinode(J->phi[i].value, J->loaded[i].value, J->preloop);
      ir_addphinode(J->phi[i].value, J->current[i].value, J->loopend);
    }
    if (J->phi[i].tag) {
      ir_addphinode(J->phi[i].tag, J->loaded[i].tag, J->preloop);
      ir_addphinode(J->phi[i].tag, J->current[i].tag, J->loopend);
    }
  }
}

/* Define the Lua register value. */
static void setregister(JitState *J, int regpos, int tag, IRValue *value) {
  int updatetag = 1;
  if (loadedfromstack(J, regpos, value)) {
    /* The saved value is from the entry block, so create a phi and replace the
     * old value. */
    J->phi[regpos].value = insertphivalue(J, J->loaded[regpos].value);
  }
  if (loadedfromstack(J, regpos, tag)) {
    /* The same thing for tags, but also checks if the phi is necessary. */
    if (tag != getregistertag(J, regpos))
      J->phi[regpos].tag = insertphivalue(J, J->loaded[regpos].tag);
    else
      updatetag = 0;
  }
  /* Update register current information */
  J->current[regpos].value = value;
  if (updatetag) J->current[regpos].tag = ir_consti(tag);
}

/* Add the missing jumps in the basic blocks. */
static void addjmps(JitState *J) {
  /* add a jmp from entry to loop block */
  ir_currbblock() = J->preloop;
  ir_jmp(J->loopstart);
  /* and a jmp from the loop end to the beginning */
  ir_currbblock() = J->loopend;
  ir_jmp(J->loopstart);
}

/* Create an exit block and add it to the jit state. */
static IRBBlock *addexit(JitState *J, int status) {
  int i;
  IRBBlock *bb = ir_addbblock();
  JitExit *e = createexit(J->L, bb, J->nregisters, status);
  exvec_push(J->exits, e);
  for (i = 0; i < J->nregisters; ++i)
    if (!loadedfromstack(J, i, value))
      e->tostore[i] = J->current[i];
  return bb;
}

/* Store the registers back in the Lua stack. */
static void closeexit(JitState *J, JitExit *e) {
  int i;
  ir_currbblock() = e->bb;
  for (i = 0; i < J->nregisters; ++i) {
    if (e->tostore[i].value)
      storeregister(J, i, e->tostore[i]);
    else if (J->phi[i].value)
      storeregister(J, i, J->phi[i]);
    else if (!loadedfromstack(J, i, value))
      storeregister(J, i, J->current[i]);
  }
  ir_return(ir_consti(e->status));
}

/* Auxiliary macros for obtaining the Lua's tvalues. */
#define getrkb(J, i, rt) gettvalue(J, GETARG_B(i), rt.u.binop.rb, 1)
#define getrkc(J, i, rt) gettvalue(J, GETARG_C(i), rt.u.binop.rc, 1)

/*
 * Compiles a single bytecode in the trace.
 */
static void compilebytecode(JitState *J, int idx) {
  struct JitRTInfo rt = fljit_rtvec_get(J->tr->rtinfo, idx);
  Instruction i = rt.instr;
  int op = GET_OPCODE(i);
  switch (op) {
    case OP_LOADK: {
      int bx = GETARG_Bx(i);
      IRValue *k = getconst(J, bx);
      int tag = getconsttag(J, bx);
      setregister(J, GETARG_A(i), tag, k);
      break;
    }
    case OP_ADD: {
      IRValue *rb = getrkb(J, i, rt);
      IRValue *rc = getrkc(J, i, rt);
      /* TODO: type conversions (always performing int operations) */
      int resulttag = LUA_TNUMINT;
      IRValue *resultvalue = ir_binop(convertbinop(op), rb, rc);
      setregister(J, GETARG_A(i), resulttag, resultvalue);
      break;
    }
    case OP_FORLOOP: {
      int ra = GETARG_A(i);
      int expectedtag = rt.u.forloop.type;
      IRValue *idx, *limit, *step, *newidx;
      IRBBlock *loop = J->loopend;
      IRBBlock *keeplooping = ir_insertbblock(loop);
      IRBBlock *posstep = ir_insertbblock(loop);
      IRBBlock *negstep = ir_insertbblock(loop);
      IRBBlock *loopexit = addexit(J, 0);
      /*----- currloop */
      idx = gettvalue(J, ra, expectedtag, 1);
      limit = gettvalue(J, ra + 1, expectedtag, 0);
      step = gettvalue(J, ra + 2, expectedtag, 0);
      newidx = ir_binop(IR_ADD, idx, step);
      ir_cmp(IR_LT, step, ir_consti(0), negstep, posstep);
      /*----- negstep */
      ir_currbblock() = negstep;
      ir_cmp(IR_LE, limit, newidx, keeplooping, loopexit);
      /*----- posstep */
      ir_currbblock() = posstep;
      ir_cmp(IR_LE, newidx, limit, keeplooping, loopexit);
      /*----- keeplooping */
      ir_currbblock() = J->loopend = keeplooping;
      setregister(J, ra, expectedtag, newidx); /* internal index */
      setregister(J, ra + 3, expectedtag, newidx); /* external index */
      break;
    }
    default:
      fll_error("compilebytecode: unhandled opcode");
      break;
  }
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
  JitState *J;
  if (!tr->completeloop) return;
  fllogln("fljit_compile: start compilation (%p)", tr->p);
  J = createjitstate(tr->L, tr);
  initentryblock(J);
  size_t i;
  ir_currbblock() = J->loopstart = J->loopend = ir_addbblock();
  for (i = 0; i < tr->n; ++i)
    compilebytecode(J, i);
  addjmps(J);
  linkphivalues(J);
  exvec_foreach(J->exits, e, closeexit(J, e));
  ir_print();
  fllogln("fljit_compile: ended compilation");
  flasm_compile(tr->L, tr->p, fli_instrindex(tr->p, tr->start), J->irfunc);
  destroyjitstate(J);
}

