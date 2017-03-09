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
#include "lobject.h"
#include "lstate.h"
#include "lvm.h"

#include "fl_logger.h"
#include "fl_rec.h"
#include "fl_vm.h"

void flvm_profile(struct lua_State *L, CallInfo *ci, int loopcount) {
  fll_assert(loopcount > 0, "flprof_profile: loopcount <= 0");
  if (!flrec_isrecording(L)) {
    Proto *p = getproto(ci->func);
    Instruction *i = fli_currentinstr(ci, p);
    int *count = &fli_getext(p, i)->u.count;
    fll_assert(*count < FL_JIT_THRESHOLD, "threshold already reached");
    *count += loopcount;
    if (*count >= FL_JIT_THRESHOLD) {
      fli_reset(p, i);
      flrec_start(L);
    }
  }
}

