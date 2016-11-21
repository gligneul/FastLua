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
#include "lstate.h"

#include "fl_rec.h"
#include "fl_jit.h"

#define tracerec(L) (L->tracerec)
#define recflag(L) (flR_recflag(L))

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

void flR_record_(struct lua_State *L, Instruction i) {
  TraceRec *tr = tracerec(L);
  if (tr->n > 5) {
    flR_stop(L);
    return;
  }
  luaM_growvector(L, tr->code, tr->n, tr->codesize, Instruction, MAX_INT, "");
  tr->code[tr->n++] = i;
#if 0
  switch (GET_OPCODE(i)) {
    default:
      flR_stop(L);
      return;
  }
#endif
}


