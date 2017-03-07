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
  TraceRecording *tr;           /* recorded trace */
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
static JitState *createjitstate(lua_State *L, TraceRecording *tr) {
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

/* Load a register from the stack. */
static void loadregister(JitState *J, struct TraceRegister *treg, int regpos) {
  struct JitRegister *curr = J->current + regpos;
  enum IRType type = converttag(treg->tag);
  int addr = sizeof(TValue) * regpos;
  if (treg->checktag) {
    IRValue *tag = ir_load(IR_INT, J->base, addr + offsetof(TValue, tt_));
    IRBBlock *continuation = ir_insertbblock(J->preloop);
    if (J->earlyexit == NULL) {
      ir_currbblock() = J->earlyexit = ir_addbblock();
      ir_return(ir_consti(1)); 
    }
    ir_currbblock() = J->preloop;
    ir_cmp(IR_NE, tag, ir_consti(treg->tag), J->earlyexit, continuation);
    ir_currbblock() = J->preloop = continuation;
  }
  curr->tag = ir_consti(treg->tag);
  curr->value = ir_load(type, J->base, addr + offsetof(TValue, value_));
  J->loaded[regpos] = *curr;
}

/* Create the entry basic block and load the registers from the Lua stack.
 * This block should contain the loop invariants. */
static void initentryblock(JitState *J) {
  int i;
  ir_currbblock() = J->preloop = ir_addbblock();
  J->lstate = ir_getarg(IR_PTR, 0);
  J->base = ir_getarg(IR_PTR, 1);
  for (i = 0; i < J->tr->p->maxstacksize; ++i) {
    struct TraceRegister *treg = J->tr->regs + i;
    if (treg->loaded) loadregister(J, treg, i);
  }
}

/* Create the phi values for registers. */
static void createphivalues(JitState *J) {
  int i;
  for (i = 0; i < J->tr->p->maxstacksize; ++i) {
    struct TraceRegister *treg = J->tr->regs + i;
    if (treg->loaded && treg->set) {
      struct JitRegister *phi = J->phi + i;
      phi->value = ir_phi(converttag(treg->tag));
      phi->tag = ir_phi(IR_LONG);
      J->current[i] = *phi;
    }
  }
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
static IRValue *gettvalue(JitState *J, int pos) {
  if (ISK(pos))
    return getconst(J, INDEXK(pos));
  else
    return J->current[pos].value;
}

/* Store a Lua stack register */
static void storeregister(JitState *J, int regpos, struct JitRegister r) {
  int addr = sizeof(TValue) * regpos;
  ir_store(IR_LUAINT, J->base, r.value, addr + offsetof(TValue, value_));
  if (r.tag)
    ir_store(IR_INT, J->base, r.tag, addr + offsetof(TValue, tt_));
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
  J->current[regpos].value = value;
  J->current[regpos].tag = ir_consti(tag);
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
    if (J->tr->regs[i].set)
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
    else if (J->tr->regs[i].set)
      storeregister(J, i, J->current[i]);
  }
  ir_return(ir_consti(e->status));
}

/* Auxiliary macros for obtaining the Lua's tvalues. */
#define getrkb(J, i, ti) gettvalue(J, GETARG_B(i))
#define getrkc(J, i, ti) gettvalue(J, GETARG_C(i))

/*
 * Compiles a single bytecode in the trace.
 */
static void compilebytecode(JitState *J, struct TraceInstr ti) {
  Instruction i = ti.instr;
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
      IRValue *rb = getrkb(J, i, ti);
      IRValue *rc = getrkc(J, i, ti);
      /* TODO: type conversions (always performing int operations) */
      int resulttag = ti.u.binop.restag;
      IRValue *resultvalue = ir_binop(convertbinop(op), rb, rc);
      setregister(J, GETARG_A(i), resulttag, resultvalue);
      break;
    }
    case OP_FORLOOP: {
      int ra = GETARG_A(i);
      int expectedtag = ti.u.forloop.type;
      IRValue *idx, *limit, *step, *newidx;
      IRBBlock *loop = J->loopend;
      IRBBlock *keeplooping = ir_insertbblock(loop);
      IRBBlock *posstep = ir_insertbblock(loop);
      IRBBlock *negstep = ir_insertbblock(loop);
      IRBBlock *loopexit = addexit(J, 0);
      /*----- currloop */
      idx = gettvalue(J, ra);
      limit = gettvalue(J, ra + 1);
      step = gettvalue(J, ra + 2);
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
      fll_error("unhandled opcode");
      break;
  }
}

void fljit_compile(TraceRecording *tr) {
  JitState *J;
  if (!tr->completeloop) return;
  fllogln("fljit_compile: start compilation (%p)", tr->p);
  J = createjitstate(tr->L, tr);
  initentryblock(J);
  ir_currbblock() = J->loopstart = J->loopend = ir_addbblock();
  createphivalues(J);
  flt_rtvec_foreach(J->tr->instrs, ti, compilebytecode(J, ti));
  addjmps(J);
  linkphivalues(J);
  exvec_foreach(J->exits, e, closeexit(J, e));
  ir_print();
  fllogln("fljit_compile: ended compilation");
  flasm_compile(tr->L, tr->p, fli_instrindex(tr->p, tr->start), J->irfunc);
  destroyjitstate(J);
}

