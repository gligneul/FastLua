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
#include "lopcodes.h"

#include "fl_instr.h"
#include "fl_logger.h"

#define flivec(p) (p->fl.instr)

TSCC_IMPL_VECTOR_WA(FLInstrExtVector, fliv_, FLInstrExt, struct lua_State *,
    luaM_realloc_)

void fli_loadproto(struct Proto *p) {
  int i;
  for (i = 0; i < p->sizecode; ++i)
    fli_toprof(p, i);
}

FLInstrExt *fli_getext(struct Proto *p, int i) {
  fll_assert(fli_isfl(p, i), "invalid opcode");
  return fliv_data(flivec(p)) + fli_getextindex(p, i);
}

void fli_reset(struct Proto *p, int i) {
  size_t ei, removedei;
  fll_assert(fli_isfl(p, i), "invalid opcode");
  removedei = fli_getextindex(p, i);
  p->code[i] = fli_getext(p, i)->original;
  fliv_erase(flivec(p), removedei);
  for (ei = removedei; ei < fliv_size(flivec(p)); ++ei) {
    FLInstrExt ext = fliv_get(flivec(p), ei);
    fli_setextindex(p, ext.index, ei);
  }
}

/* Convert a instruction to a fl instruction */
static void convertinstr(struct Proto *p, int i, enum FLOpcode flop) {
  size_t extidx = fliv_size(flivec(p));
  FLInstrExt ext;
  memset(&ext, 0, sizeof(FLInstrExt));
  ext.original = p->code[i];
  ext.index = i;
  fliv_push(flivec(p), ext);
  SET_OPCODE(p->code[i], OP_FLVM);
  fli_setflop(p, i, flop);
  fli_setextindex(p, i, extidx);
}

void fli_toprof(struct Proto *p, int i) {
  switch (GET_OPCODE(p->code[i])) {
    case OP_FORPREP:    convertinstr(p, i, FLOP_FORPREP_PROF); break;
    default: break;
  }
}

void fli_tojit(struct Proto *p, int i) {
  switch (GET_OPCODE(p->code[i])) {
    case OP_FORLOOP:    convertinstr(p, i, FLOP_FORLOOP_EXEC); break;
    default: break;
  }
}

