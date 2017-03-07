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

TSCC_IMPL_VECTOR_WA(JitRTInfoVector, flt_rtvec_, struct JitRTInfo,
    struct lua_State *, luaM_realloc_)

JitTrace *flt_createtrace(struct lua_State *L) {
  JitTrace *tr = luaM_new(L, JitTrace);
  tr->L = L;
  tr->p = NULL;
  tr->start = NULL;
  tr->n = 0;
  tr->rtinfo = flt_rtvec_createwa(L);
  tr->completeloop = 0;
  return tr;
}

void flt_destroytrace(JitTrace *tr) {
  flt_rtvec_destroy(tr->rtinfo);
  luaM_free(tr->L, tr);
}

