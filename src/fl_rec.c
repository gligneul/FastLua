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

#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_rec.h"
#include "fl_jit.h"

#define tracerec(L) (L->jit_tracerec)

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
static int creatert(CallInfo *ci, Instruction i, union JitRTInfo *rt) {
  TValue *base = ci->u.l.base;
  TValue *k = getproto(ci->func)->k;
  switch (GET_OPCODE(i)) {
    case OP_ADD:
    case OP_SUB:
    case OP_MUL: {
      TValue *rkb = RKB(i), *rkc = RKC(i);
      rt->binop.rb = ttype(rkb);
      rt->binop.rc = ttype(rkc);
      /* TODO: now only compiles int/float opcodes :'( */
      return ttisnumber(rkb) && ttisnumber(rkc);
    }
    case OP_FORLOOP: {
      rt->forloop.ittype = ttype(RA(i));
      return 1;
    }
    default:
      break;
  }
  return 0;
}

void flrec_start(struct lua_State *L) {
  assert(!flrec_isrecording(L));
  assert(!tracerec(L));
  flrec_isrecording(L) = 1;
  tracerec(L) = fljit_createtrace(L);
}

void flrec_stop(struct lua_State *L) {
  assert(flrec_isrecording(L));
  assert(tracerec(L));
  flrec_isrecording(L) = 0;
  fljit_compile(tracerec(L));
  fljit_destroytrace(tracerec(L));
  tracerec(L) = NULL;
}

void flrec_record_(struct lua_State *L, struct CallInfo* ci) {
  JitTrace *tr = tracerec(L);
  const Instruction *i = ci->u.l.savedpc;
  if (tr->start != i) {
    if (tr->start == NULL) {
      /* start the recording */
      tr->p = getproto(ci->func);
      tr->start = i;
    }
    union JitRTInfo rt;
    if (creatert(ci, *i, &rt)) {
      fljit_rtvec_push(tr->rtinfo, rt);
      tr->n++;
    }
    else {
      flrec_stop(L);
    }
  }
  else {
    /* back to the loop start */
    tr->completeloop = 1;
    flrec_stop(L);
  }
}

