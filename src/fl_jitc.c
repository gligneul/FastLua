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

/* IRFunction implict parameter. */
#define _irfunc (J->irfunc)

/* Information necessary to build the a jit exit */
struct JitExit {
  IRBBlock *bb;                 /* side exit basic block */
  int ntostore;                 /* number of registers that will be stored */
  int *indices;                 /* register's indices */
  IRValue **values;             /* register's values */
  int *tags;                    /* register's tags */
  int status;                   /* return status */
};

/* JitExit container */
TSCC_DECL_VECTOR(JitExitVector, exvec_, struct JitExit)
#define exvec_foreach(vec, val, cmd) \
    TSCC_VECTOR_FOREACH(exvec_, vec, struct JitExit, val, cmd)

/* Jit compilation state. */
typedef struct JitState {
  lua_State *L;                 /* Lua state */
  TraceRecording *tr;           /* recorded trace */
  IRFunction *irfunc;           /* IR output function */
  IRBBlock *preloop;            /* last basic block before the loop start */
  IRBBlock *loopstart;          /* first block in the loop */
  IRBBlock *loopend;            /* last block in the loop */
  IRBBlock *earlyexit;          /* side exit before the loop started */
  JitExitVector exits;          /* exits that must restore the lua stack */
  IRValue *lstate;              /* Lua state in the jitted code */
  IRValue *base;                /* Lua stack base */
  int nregisters;               /* number of registers in Lua stack */
  IRValue **current;            /* register's current value */
  IRValue **phi;                /* register's phi value */
  int *tag;                     /* register's current tag */
  l_mem pc;                     /* current instruction position */
} JitState;

/* Create/destroy the jit state. */
static JitState *createjitstate(lua_State *L, TraceRecording *tr) {
  JitState *J = luaM_new(L, JitState);
  J->L = L;
  J->tr = tr;
  J->irfunc = ir_create(L);
  J->preloop = NULL;
  J->loopstart = NULL;
  J->loopend = NULL;
  J->earlyexit = NULL;
  exvec_create(&J->exits, J->L);
  J->lstate = J->base = NULL;
  J->nregisters = tr->p->maxstacksize;
  J->current = luaM_newvector(L, J->nregisters, IRValue *);
  J->phi = luaM_newvector(L, J->nregisters, IRValue *);
  J->tag = luaM_newvector(L, J->nregisters, int);
  memset(J->current, 0, J->nregisters * sizeof(IRValue *));
  memset(J->phi, 0, J->nregisters * sizeof(IRValue *));
  return J;
}

static void destroyjitstate(JitState *J) {
  ir_destroy(J->irfunc);
  exvec_destroy(&J->exits);
  luaM_freearray(J->L, J->current, J->nregisters);
  luaM_freearray(J->L, J->phi, J->nregisters);
  luaM_freearray(J->L, J->tag, J->nregisters);
  luaM_free(J->L, J);
}

/* Convert a lua tag to an ir type. */
static enum IRType converttag(int tag) {
  switch (tag & 0x3F) {
    case LUA_TNUMFLT: return IR_FLOAT;
    case LUA_TNUMINT: return IR_LUAINT;
    case LUA_TSHRSTR:
    case LUA_TLNGSTR:
        return IR_PTR;
    default: fll_error("unhandled tag"); break;
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
  enum IRType type = converttag(treg->tag);
  int expectedtag = treg->loadedtag;
  int addr = sizeof(TValue) * regpos;
  if (treg->checktag) {
    IRValue *tag = ir_load(IR_INT, J->base, addr + offsetof(TValue, tt_));
    ir_cmp(IR_NE, tag, ir_consti(expectedtag, IR_INT), J->earlyexit);
  }
  J->current[regpos] = ir_load(type, J->base, addr + offsetof(TValue, value_));
  J->tag[regpos] = expectedtag;
}

/* Create the phi values for registers. */
static void createphivalues(JitState *J) {
  int i;
  for (i = 0; i < J->tr->p->maxstacksize; ++i) {
    struct TraceRegister *treg = J->tr->regs + i;
    if (treg->set) {
      J->phi[i] = ir_phi(converttag(treg->tag));
      ir_addphinode(J->phi[i], J->current[i], J->preloop);
      J->current[i] = J->phi[i];
    }
  }
}

/* Load a constant from the constant table. */
static IRValue *getconst(JitState *J, int kpos, int *tag) {
  TValue *k = J->tr->p->k + kpos;
  if (tag) *tag = rttype(k);
  switch (ttype(k)) {
    case LUA_TNUMFLT: return ir_constf(fltvalue(k));
    case LUA_TNUMINT: return ir_consti(ivalue(k), IR_LUAINT);
    case LUA_TSHRSTR:
    case LUA_TLNGSTR:
        return ir_constp(gcvalue(k));
    default: fll_error("unhandled const type"); break;
  }
  return NULL;
}

/* Load a constant or register given the position. */
static IRValue *gettvalue(JitState *J, int pos, int *tag) {
  if (ISK(pos))
    return getconst(J, INDEXK(pos), tag);
  else {
    if (tag) *tag = J->tag[pos];
    return J->current[pos];
  }
}

/* Store a Lua stack register */
static void storeregister(JitState *J, int regpos, IRValue *value, int tag) {
  int addr = sizeof(TValue) * regpos;
  ir_store(J->base, value, addr + offsetof(TValue, value_));
  ir_store(J->base, ir_consti(tag, IR_INT), addr + offsetof(TValue, tt_));
}

/* Create the phi nodes for the registers that have phi values. */
static void linkphivalues(JitState *J) {
  int i;
  for (i = 0; i < J->nregisters; ++i)
    if (J->phi[i])
      ir_addphinode(J->phi[i], J->current[i], J->loopend);
}

/* Define the Lua register value. */
static void setregister(JitState *J, int regpos, IRValue *value, int tag) {
  J->current[regpos] = value;
  J->tag[regpos] = tag;
}

/* Create an exit block and add it to the jit state. */
static IRBBlock *addexit(JitState *J, int status) {
  int i, currindex = 0, ntostore = 0;
  struct JitExit e;
  /* compute the number of registers that will be stored */
  for (i = 0; i < J->tr->p->maxstacksize; ++i)
    if (J->tr->regs[i].set && J->current[i])
      ntostore++;
  /* create the exit */
  e.bb = ir_addbblock();
  e.ntostore = ntostore;
  e.indices = luaM_newvector(J->L, ntostore, int);
  e.values = luaM_newvector(J->L, ntostore, IRValue *);
  e.tags = luaM_newvector(J->L, ntostore, int);
  e.status = status;
  exvec_push(&J->exits, e);
  /* save the values that will be stored for later */
  for (i = 0; i < J->tr->p->maxstacksize; ++i) {
    if (J->tr->regs[i].set && J->current[i]) {
      e.indices[currindex] = i;
      e.values[currindex] = J->current[i];
      e.tags[currindex++] = J->tag[i];
    }
  }
  return e.bb;
}

/* Store the registers back in the Lua stack. */
static void closeexit(JitState *J, struct JitExit *e) {
  int i;
  ir_currbblock() = e->bb;
  for (i = 0; i < e->ntostore; ++i)
    storeregister(J, e->indices[i], e->values[i], e->tags[i]);
  ir_return(ir_consti(e->status, IR_LONG));
  luaM_freearray(J->L, e->indices, e->ntostore);
  luaM_freearray(J->L, e->values, e->ntostore);
}

/*
 * Compiles a single bytecode in the trace.
 */
static void compilebytecode(JitState *J, struct TraceInstr ti) {
  Instruction i = ti.instr;
  int op = GET_OPCODE(i);
  switch (op) {
    case OP_MOVE: {
      int tag;
      IRValue *rb = gettvalue(J, GETARG_B(i), &tag);
      setregister(J, GETARG_A(i), rb, tag);
      break;
    }
    case OP_LOADK: {
      int tag;
      IRValue *k = getconst(J, GETARG_Bx(i), &tag);
      setregister(J, GETARG_A(i), k, tag);
      break;
    }
    case OP_ADD: {
      int btag, ctag, resulttag;
      IRValue *rb = gettvalue(J, GETARG_B(i), &btag);
      IRValue *rc = gettvalue(J, GETARG_C(i), &ctag);
      if (btag == LUA_TNUMINT && ctag == LUA_TNUMINT) {
        resulttag = LUA_TNUMINT;
      }
      else {
        resulttag = LUA_TNUMFLT;
        if (btag == LUA_TNUMINT)
          rb = ir_cast(rb, IR_FLOAT);
        else if (ctag == LUA_TNUMINT)
          rc = ir_cast(rc, IR_FLOAT);
      }
      IRValue *resultvalue = ir_binop(convertbinop(op), rb, rc);
      setregister(J, GETARG_A(i), resultvalue, resulttag);
      break;
    }
    case OP_FORLOOP: {
      int a = GETARG_A(i);
      int tag;
      IRValue *idx = gettvalue(J, a, &tag);
      IRValue *limit = gettvalue(J, a + 1, NULL);
      IRValue *step = gettvalue(J, a + 2, NULL);
      IRValue *newidx;
      IRBBlock *loopexit = addexit(J, 0);
      if (ir_currbblock() == J->preloop) {
        enum IRCmpOp cmp = ti.u.forloop.steplt0 ? IR_GE : IR_LT;
        ir_cmp(cmp, step, ir_consti(0, IR_LUAINT), J->earlyexit);
      }
      newidx = ir_binop(IR_ADD, idx, step);
      ir_cmp(ti.u.forloop.steplt0 ? IR_LT : IR_GT, newidx, limit, loopexit);
      setregister(J, a, newidx, tag); /* internal index */
      setregister(J, a + 3, newidx, tag); /* external index */
      break;
    }
    default:
      fll_error("unhandled opcode");
      break;
  }
}

/* Create the entry basic block.
 * This block should contain the loop invariants. */
static void initblocks(JitState *J) {
  J->preloop = ir_addbblock();
  J->loopstart = J->loopend = ir_addbblock();
  J->earlyexit = ir_currbblock() = ir_addbblock();
  ir_return(ir_consti(1, IR_LONG));
}

static void compilepreloop(JitState *J) {
  int i;
  ir_currbblock() = J->preloop;
  J->lstate = ir_getarg(IR_PTR, 0);
  J->base = ir_getarg(IR_PTR, 1);
  for (i = 0; i < J->tr->p->maxstacksize; ++i) {
    struct TraceRegister *treg = J->tr->regs + i;
    if (treg->loaded) loadregister(J, treg, i);
  }
  flt_rtvec_foreach(&J->tr->instrs, ti, compilebytecode(J, ti));
}

static void compileloop(JitState *J) {
  ir_currbblock() = J->loopstart;
  createphivalues(J);
  flt_rtvec_foreach(&J->tr->instrs, ti, compilebytecode(J, ti));
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

void fljit_compile(TraceRecording *tr) {
  JitState *J;
  if (!tr->completeloop) return;
  fllogln("starting jit compilation (%p)", tr->p);
  J = createjitstate(tr->L, tr);
  initblocks(J);
  compilepreloop(J);
  compileloop(J);
  addjmps(J);
  linkphivalues(J);
  exvec_foreach(&J->exits, e, closeexit(J, &e));
  ir_print();
  fllogln("ended jit compilation");
  flasm_compile(tr->L, tr->p, (Instruction*)tr->start, J->irfunc);
  destroyjitstate(J);
}

