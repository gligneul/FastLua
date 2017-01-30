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
 * Compile a ir function into machine code.
 */

#ifndef fl_asm_h
#define fl_asm_h

#include "fl_defs.h"

struct IRFunction;
struct lua_State;
struct lua_TValue;
struct Proto;

/* Jitted function prototype. */
typedef int (*AsmFunction)(struct lua_State *L, struct lua_TValue *base);

/* Jitted function return codes */
enum AsmReturnCode {
  FL_SUCCESS,
  FL_EARLY_EXIT,
  FL_SIDE_EXIT
};

/* Opaque data that should be saved in the Lua proto. */
typedef struct AsmInstrData AsmInstrData;

/* Obtain the function given the instruction. */
AsmFunction flasm_getfunction(struct Proto *p, int pc);

/* Compile a function and add it to the proto. */
void flasm_compile(struct lua_State *L, struct Proto *p, int i,
                   struct IRFunction *F);

/* Delete a function and change the opcode to the default one. */
void flasm_destroy(struct lua_State *L, struct Proto *p, int i);

/* Destroy all asm functions in the proto. */
void flasm_closeproto(struct lua_State *L, struct Proto *p);

#endif

