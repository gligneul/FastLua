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

#include <stdio.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#pragma GCC diagnostic pop

#include "lprefix.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_asm.h"
#include "fl_ir.h"
#include "fl_instr.h"

#define asmdata(proto, i) (proto->fl.instr[i].asmdata)

struct AsmData {
  AsmFunction func; /* compiled function */
  void *buffer;     /* buffer for internal usage */
};

static int fakefunc(struct lua_State *L, struct lua_TValue *base) {
  (void)L;
  (void)base;
  printf("OI!\n");
  return FL_EARLY_EXIT;
}

/* Compile the ir function and save it in the asm function. */
static void compile(struct lua_State *L, IRFunction *F, AsmData *af) {
  LLVMModuleRef module = LLVMModuleCreateWithName("monga-executable");
  char *error = NULL;
  LLVMVerifyModule(module, LLVMPrintMessageAction, &error);
  LLVMDisposeMessage(error);

  af->func = fakefunc;
  af->buffer = NULL;

  (void)L;
  (void)F;
  (void)af;
}

AsmFunction flasm_getfunction(struct Proto *p, int i) {
  return asmdata(p, i)->func;
}

void flasm_compile(struct lua_State *L, struct Proto *p, int i,
                   struct IRFunction *F) {
  AsmData *af = luaM_new(L, AsmData);
  compile(L, F, af);
  asmdata(p, i) = af;
  fli_tojit(&p->code[i]);
}

void flasm_destroy(struct lua_State *L, struct Proto *p, int i) {
  luaM_free(L, asmdata(p, i));
  asmdata(p, i) = NULL;
  fli_reset(&p->code[i]);
}

void flasm_closeproto(struct lua_State *L, struct Proto *p) {
  int i;
  for (i = 0; i < p->sizecode; ++i)
    if (GET_OPCODE(p->code[i]) == OP_FORLOOP_JIT)
      flasm_destroy(L, p, i);
}

