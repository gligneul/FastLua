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
#include <string.h>

#include "lprefix.h"

#include "llimits.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_prof.h"
#include "fl_rec.h"

#define SWAP_OPCODE(i, from, to) \
    case from: SET_OPCODE(i, to); break

/* Swap the default opcodes for the profiling ones */
static void init_opcodes(Instruction *code, int n) {
  int i;
  for (i = 0; i < n; ++i) {
    switch (GET_OPCODE(code[i])) {
      SWAP_OPCODE(code[i], OP_FORPREP, OP_FORPREP_PROF);
      default: break;
    }
  }
}

/* Reset the instruction to the non-profiling version */
static void reset_instruction(Instruction *i) {
  switch (GET_OPCODE(*i)) {
    SWAP_OPCODE(*i, OP_FORPREP_PROF, OP_FORPREP);
    default:
      assert(0 && "failed to reset instruction");
      break;
  }
}

void flprof_initproto(struct lua_State *L, struct Proto *p) {
  p->icount = luaM_newvector(L, p->sizecode, short);
  memset(p->icount, 0, sizeof(short) * p->sizecode);
  init_opcodes(p->code, p->sizecode);
}

void flprof_profile(struct lua_State *L, CallInfo *ci, short loopcount) {
  assert(loopcount > 0);
  if (!flrec_isrecording(L)) {
    Proto *p = getproto(ci->func);
    l_mem i = ci->u.l.savedpc - 1 - p->code;
    assert(p->icount[i] < FL_JIT_THRESHOLD);
    p->icount[i] += loopcount;
    if (p->icount[i] >= FL_JIT_THRESHOLD) {
      flrec_start(L);
      reset_instruction(&p->code[i]);
    }
  }
}

