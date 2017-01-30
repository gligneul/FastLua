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
#include "lopcodes.h"

#include "fl_instr.h"

void fli_reset(Instruction *i) {
  switch (GET_OPCODE(*i)) {
    case OP_FORPREP_PROF:   SET_OPCODE(*i, OP_FORPREP); break;
    case OP_FORLOOP_JIT:    SET_OPCODE(*i, OP_FORLOOP); break;
    default: break;
  }
}

void fli_toprof(Instruction *i) {
  switch (GET_OPCODE(*i)) {
    case OP_FORPREP:    SET_OPCODE(*i, OP_FORPREP_PROF); break;
    default: break;
  }
}

void fli_tojit(Instruction *i) {
  switch (GET_OPCODE(*i)) {
    case OP_FORLOOP:    SET_OPCODE(*i, OP_FORLOOP_JIT); break;
    default: break;
  }
}
