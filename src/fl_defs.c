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
#include "lobject.h"
#include "lstate.h"

#include "fl_asm.h"
#include "fl_defs.h"
#include "fl_instr.h"

void fl_initstate(struct lua_State *L) {
  L->fl.trace = NULL;
}

void fl_closestate(struct lua_State *L) {
  (void)L;
}

void fl_initproto(struct Proto *p) {
  p->fl.initialized = 0;
}

void fl_closeproto(struct lua_State *L, struct Proto *p) {
  if (!p->fl.initialized) return;
  flasm_closeproto(L, p);
  fliv_destroy(&p->fl.instr);
}

void fl_loadproto(struct lua_State *L, struct Proto *p) {
  p->fl.initialized = 1;
  fliv_create(&p->fl.instr, L);
  fli_foreach(p, i, fli_toprof(p, i));
}

