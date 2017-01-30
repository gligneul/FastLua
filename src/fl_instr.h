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

/*
 * Manipulate the instructions in Lua proto.
 */

#ifndef fl_instr_h
#define fl_instr_h

#include "llimits.h"
#include "lopcodes.h"

/* Obtain the instruction index given the address. */
#define fli_instrindex(p, addr) ((addr) - (p)->code)

/* Obtain the current instruction index. */
#define fli_currentinstr(ci, p) fli_instrindex(p, ci->u.l.savedpc - 1)

/* Convert the opcode back to the original one. */
void fli_reset(Instruction *i);

/* Convert the opcode to the profiling one. */
void fli_toprof(Instruction *i);

/* Convert the opcode to the jit one. */
void fli_tojit(Instruction *i);

#endif

