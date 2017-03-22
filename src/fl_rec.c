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
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_jitc.h"
#include "fl_logger.h"
#include "fl_rec.h"

#define tracerec(L) (L->fl.trace)

/* Some macros stolen from lvm */
#define RA(i)	(base+GETARG_A(i))
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#define RC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
#define RKB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))
#define RKC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))

/* The register was read by the instruction. */
static void readregister(TraceRecording *tr, int regpos, int tag,
                         int checktag) {
  struct TraceRegister *treg = tr->regs + regpos;
  /* check if the register must be loaded from the stack */
  if (!treg->set) {
    treg->loadedtag = treg->tag = tag;
    treg->checktag = checktag;
    treg->loaded = 1;
  }
}

/* Call readregister if the argument is a register. */
static void readrk(TraceRecording *tr, int arg, int tag, int checktag) {
  if (!ISK(arg)) readregister(tr, arg, tag, checktag);
}

/* The value was set by the instruction. */
static void setregister(TraceRecording *tr, int regpos, int tag) {
  struct TraceRegister *treg = tr->regs + regpos;
  treg->tag = tag;
  treg->set = 1;
}

/* Compute the resulting tag of a binary operation. */
static int computebintoptag(int lhs, int rhs) {
  if (lhs == rhs)
    return lhs;
  else
    return LUA_TNUMFLT;
}

/* Verify if the forloop step is less then 0. */
static int isforloopsteplt0(TValue *ra) {
  if (ttisinteger(ra))
    return ivalue(ra + 2) < 0;
  else
    return fltvalue(ra + 2) < 0;
}

/* Produce the runtime information about the instruction.
 * Return 1 if the instruction can be compiled, else return 0. */
static int recordinstruction(TraceRecording *tr, CallInfo *ci, Instruction i) {
  struct TraceInstr ti;
  TValue *base = ci->u.l.base;
  TValue *k = getproto(ci->func)->k;
  int failed = 0;
  ti.instr = i;
  switch (GET_OPCODE(i)) {
    case OP_MOVE: {
      int tag = rttype(RB(i));
      readregister(tr, GETARG_B(i), tag, 1);
      setregister(tr, GETARG_A(i), tag);
      break;
    }
    case OP_LOADK: {
      int tag = rttype(k + GETARG_Bx(i));
      setregister(tr, GETARG_A(i), tag);
      break;
    }
    case OP_ADD:
    case OP_SUB:
    case OP_MUL: {
      TValue *rkb = RKB(i), *rkc = RKC(i);
      int resulttag = computebintoptag(rttype(rkb), rttype(rkc));
      readrk(tr, GETARG_B(i), rttype(rkb), 1);
      readrk(tr, GETARG_C(i), rttype(rkc), 1);
      setregister(tr, GETARG_A(i), resulttag);
      /* TODO: now only compiles int/float opcodes :'( */
      failed = !(ttisnumber(rkb) && ttisnumber(rkc));
      break;
    }
    case OP_FORLOOP: {
      int tag = rttype(RA(i));
      ti.u.forloop.steplt0 = isforloopsteplt0(RA(i));
      readregister(tr, GETARG_A(i), tag, 1);
      readregister(tr, GETARG_A(i) + 1, tag, 0);
      readregister(tr, GETARG_A(i) + 2, tag, 0);
      setregister(tr, GETARG_A(i), tag);
      setregister(tr, GETARG_A(i) + 3, tag);
      break;
    }
    default:
      fll_error("unhandled opcode");
      break;
  }
  if (!failed) flt_rtvec_push(&tr->instrs, ti);
  return failed;
}

void flrec_start(struct lua_State *L) {
  fll_assert(!flrec_isrecording(L), "flrec_start: already recording");
  fll_assert(!tracerec(L), "flrec_start: already have an trace record");
  fllogln("flrec_start: start recording (%p)", getproto(L->ci->func));
  tracerec(L) = flt_createtrace(L);
}

/* Stop the recording. */
static void stoprecording(struct lua_State *L, int failed) {
  fll_assert(flrec_isrecording(L), "stoprecording: not recording");
  fll_assert(tracerec(L), "stoprecording: trace record not found");
  fllogln("stoprecording: stop recording");
  if (!failed) fljit_compile(tracerec(L));
  flt_destroytrace(tracerec(L));
  tracerec(L) = NULL;
}

/* Verify if the phi values have consistent types. */
static int checkphivalues(TraceRecording *tr) {
  int i;
  for (i = 0; i < tr->p->maxstacksize; ++i) {
    struct TraceRegister *treg = tr->regs + i;
    if ((treg->loaded && treg->set) &&
        (treg->loadedtag != treg->tag))
      return 1;
  }
  return 0;
}

void flrec_record_(struct lua_State *L, struct CallInfo* ci) {
  TraceRecording *tr = tracerec(L);
  const Instruction *i = ci->u.l.savedpc;
  if (tr->start != i) {
    fllogln("flrec_record_: %s", luaP_opnames[GET_OPCODE(*i)]);
    if (tr->start == NULL) {
      /* start the recording */
      tr->p = getproto(ci->func);
      tr->regs = luaM_newvector(L, tr->p->maxstacksize, struct TraceRegister);
      memset(tr->regs, 0, tr->p->maxstacksize * sizeof(struct TraceRegister));
      tr->start = i;
    }
    if (recordinstruction(tr, ci, *i)) {
      fllogln("recording failed");
      stoprecording(L, 1);
    }
  }
  else {
    /* back to the loop start */
    tr->completeloop = 1;
    stoprecording(L, checkphivalues(tr));
  }
}

