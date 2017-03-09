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

TSCC_IMPL_VECTOR_WA(FLInstrExtVector, fliv_, struct FLInstrExt,
    struct lua_State *, luaM_realloc_)

struct FLInstrExt *fli_getext(struct Proto *p, Instruction *i) {
  fll_assert(fli_isfl(i), "invalid opcode");
  return fliv_getref(flivec(p), fli_getextindex(i));
}

void fli_reset(struct Proto *p, Instruction *i) {
  size_t extidx, removed;
  fll_assert(fli_isfl(i), "invalid opcode");
  removed = fli_getextindex(i);
  *i = fli_getext(p, i)->original;
  fliv_erase(flivec(p), removed);
  for (extidx = removed; extidx < fliv_size(flivec(p)); ++extidx) {
    struct FLInstrExt *ext = fliv_getref(flivec(p), extidx);
    fli_setextindex(ext->address, extidx);
  }
}

/* Convert a instruction to a fl instruction */
static void convertinstr(struct Proto *p, Instruction *i, enum FLOpcode flop) {
  size_t extidx = fliv_size(flivec(p));
  struct FLInstrExt ext;
  memset(&ext, 0, sizeof(struct FLInstrExt));
  ext.original = *i;
  ext.address = i;
  fliv_push(flivec(p), ext);
  SET_OPCODE(*i, OP_FLVM);
  fli_setflop(i, flop);
  fli_setextindex(i, extidx);
}

void fli_toprof(struct Proto *p, Instruction *i) {
  switch (GET_OPCODE(*i)) {
    case OP_FORPREP:    convertinstr(p, i, FLOP_FORPREP_PROF); break;
    default: break;
  }
}

void fli_tojit(struct Proto *p, Instruction *i) {
  switch (GET_OPCODE(*i)) {
    case OP_FORLOOP:    convertinstr(p, i, FLOP_FORLOOP_EXEC); break;
    default: break;
  }
}

