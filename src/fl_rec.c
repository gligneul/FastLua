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

#include "lprefix.h"

#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_rec.h"
#include "fl_jit.h"

#define tracerec(L) (L->tracerec)
#define recflag(L) (flR_recflag(L))

/*
 * Some macros from lvm.c
 */
#define RA(i)	(base+GETARG_A(i))
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#define RC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
#define RKB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))
#define RKC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))

/*
 * Produce the runtime information of the instruction.
 * Returns 1 if the instruction can be compiled, else returns 0.
 */
int trygatherrt(CallInfo *ci, Instruction i, RuntimeRec *rt) {
  TValue *base = ci->u.l.base;
  TValue *k = getproto(ci->func)->k;
  switch (GET_OPCODE(i)) {
    case OP_ADD:
    case OP_SUB:
    case OP_MUL: {
      TValue *rkb = RKB(i);
      TValue *rkc = RKC(i);
      rt->u.binoptypes.rb = ttype(rkb);
      rt->u.binoptypes.rc = ttype(rkc);
      /* only compiles int/float ops */
      return ttisnumber(rkb) && ttisnumber(rkc);
    }
    case OP_FORLOOP: {
      TValue *ra = RA(i);
      rt->u.forlooptype = ttype(ra);
      return 1;
    }
    default:
      break;
  }
  return 0;
}

void flR_start(lua_State *L) {
  assert(!recflag(L));
  assert(!tracerec(L));
  recflag(L) = 1;
  tracerec(L) = flJ_createtracerec(L);
}

void flR_stop(lua_State *L) {
  assert(recflag(L));
  assert(tracerec(L));
  recflag(L) = 0;
  flJ_compile(L, tracerec(L));
  flJ_destroytracerec(L, tracerec(L));
  tracerec(L) = NULL;
}

void flR_record_(struct lua_State *L, struct CallInfo* ci) {
  TraceRec *tr = tracerec(L);
  const Instruction *i = ci->u.l.savedpc - 1;
  if (!tr->start) {
    tr->start = i;
  }
  else if (tr->start == i) {
    /* back to the start */
    flR_stop(L);
    return;
  }
  luaM_growvector(L, tr->rt, tr->n, tr->rtsize, RuntimeRec, MAX_INT, "");
  if (trygatherrt(ci, *i, &tr->rt[tr->n])) {
    tr->n++;
  } else {
    flR_stop(L);
  }
}


