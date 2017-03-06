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

/* Produce the runtime information of the instruction.
 * Return 1 if the instruction can be compiled, else return 0. */
static int creatert(CallInfo *ci, Instruction i, struct JitRTInfo *rt) {
  TValue *base = ci->u.l.base;
  TValue *k = getproto(ci->func)->k;
  rt->instr = i;
  switch (GET_OPCODE(i)) {
    case OP_LOADK:
      /* do nothing */
      return 1;
    case OP_ADD:
    case OP_SUB:
    case OP_MUL: {
      TValue *rkb = RKB(i), *rkc = RKC(i);
      rt->u.binop.rb = ttype(rkb);
      rt->u.binop.rc = ttype(rkc);
      /* TODO: now only compiles int/float opcodes :'( */
      return ttisnumber(rkb) && ttisnumber(rkc);
    }
    case OP_FORLOOP: {
      rt->u.forloop.type = ttype(RA(i));
      return 1;
    }
    default:
      break;
  }
  return 0;
}

void flrec_start(struct lua_State *L) {
  fll_assert(!flrec_isrecording(L), "flrec_start: already recording");
  fll_assert(!tracerec(L), "flrec_start: already have an trace record");
  fllogln("flrec_start: start recording (%p)", getproto(L->ci->func));
  tracerec(L) = fljit_createtrace(L);
}

/* Stop the recording. If status is != 0, the recording failed. */
static void stoprecording(struct lua_State *L, int status) {
  fll_assert(flrec_isrecording(L), "stoprecording: not recording");
  fll_assert(tracerec(L), "stoprecording: trace record not found");
  fllogln("stoprecording: stop recording");
  if (status != 0)
    fljit_compile(tracerec(L));
  fljit_destroytrace(tracerec(L));
  tracerec(L) = NULL;
}

void flrec_record_(struct lua_State *L, struct CallInfo* ci) {
  JitTrace *tr = tracerec(L);
  const Instruction *i = ci->u.l.savedpc;
  if (tr->start != i) {
    struct JitRTInfo rt;
    fllogln("flrec_record_: %s", luaP_opnames[GET_OPCODE(*i)]);
    if (tr->start == NULL) {
      /* start the recording */
      tr->p = getproto(ci->func);
      tr->start = i;
    }
    if (creatert(ci, *i, &rt)) {
      fljit_rtvec_push(tr->rtinfo, rt);
      tr->n++;
    }
    else {
      fllogln("recording failed");
      stoprecording(L, 1);
    }
  }
  else {
    /* back to the loop start */
    tr->completeloop = 1;
    stoprecording(L, 1);
  }
}

