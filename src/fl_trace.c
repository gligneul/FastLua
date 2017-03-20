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

#include "lprefix.h"
#include "lmem.h"
#include "lstate.h"

#include "fl_trace.h"

TraceRecording *flt_createtrace(struct lua_State *L) {
  TraceRecording *tr = luaM_new(L, TraceRecording);
  tr->L = L;
  tr->p = NULL;
  tr->start = NULL;
  flt_rtvec_create(&tr->instrs, L);
  tr->regs = NULL;
  tr->completeloop = 0;
  return tr;
}

void flt_destroytrace(TraceRecording *tr) {
  if (tr->p) luaM_freearray(tr->L, tr->regs, tr->p->maxstacksize);
  flt_rtvec_destroy(&tr->instrs);
  luaM_free(tr->L, tr);
}

