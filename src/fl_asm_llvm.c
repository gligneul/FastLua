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
#include <llvm-c/ExecutionEngine.h>
#pragma GCC diagnostic pop

#include "lprefix.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_asm.h"
#include "fl_ir.h"
#include "fl_instr.h"

/* Containers */
TSCC_DECL_HASHTABLE_WA(AsmBBlockTable, bt_, IRBBlock *, LLVMBasicBlockRef,
    lua_State *)
TSCC_IMPL_HASHTABLE_WA(AsmBBlockTable, bt_, IRBBlock *, LLVMBasicBlockRef,
    tscc_ptr_hashfunc, tscc_general_compare, struct lua_State *, luaM_realloc_)

TSCC_DECL_HASHTABLE_WA(AsmValueTable, vt_, IRValue *, LLVMValueRef,
    lua_State *)
TSCC_IMPL_HASHTABLE_WA(AsmValueTable, vt_, IRValue *, LLVMValueRef,
    tscc_ptr_hashfunc, tscc_general_compare, struct lua_State *, luaM_realloc_)

/* Access the asmdata inside the proto. */
#define asmdata(proto, i) (proto->fl.instr[i].asmdata)

/* Data saved in Lua proto. */
struct AsmInstrData {
  AsmFunction func;                 /* compiled function */
  LLVMExecutionEngineRef ee;        /* LLVM execution engine */
};

/* State during the compilation. */
typedef struct AsmState {
  lua_State *L;                     /* Lua state */
  IRFunction *irfunc;               /* IR function */
  LLVMModuleRef module;             /* LLVM module */
  LLVMValueRef func;                /* LLVM function */
  AsmBBlockTable *bbtable;          /* map a IR bb to a LLVM bb */
  AsmValueTable *valuetable;        /* map a IR bb to a LLVM bb */
} AsmState;

/* IR define trick. */
#define _irfunc (A->irfunc)

/* Initalize the AsmState. */
static void asmstateinit(AsmState *A, lua_State *L, IRFunction *irfunc) {
  A->L = L;
  A->irfunc = irfunc;
  A->module = NULL;
  A->func = NULL;
  A->bbtable = bt_createwa(ir_nbblocks(), L);
  A->valuetable = vt_createwa(ir_nvalues(), L);
}

/* Destroy the state internal data. */
static void asmstateclose(AsmState *A) {
  /* TODO: delete llvm module */
  bt_destroy(A->bbtable);
  vt_destroy(A->valuetable);
}

/* Create the llvm module and function. */
static void initmodule(AsmState *A) {
  LLVMInitializeNativeTarget();
  A->module = LLVMModuleCreateWithName("fl.asm");
  A->func = NULL;
}

/* Create the basic blocks. */
static void createbblocks(AsmState *A) {
  ir_foreach_bb(bb, {
    LLVMBasicBlockRef llvmbb = LLVMAppendBasicBlock(A->func, "");
    bt_insert(A->bbtable, bb, llvmbb);
  });
}

/* Verify if the LLVM module is correct. */
static void verifymodule(AsmState *A) {
  (void)A;
#if 0
  char *error = NULL;
  LLVMVerifyModule(A->module, LLVMPrintMessageAction, &error);
  LLVMDisposeMessage(error);
#endif
}

static int fakefunc(lua_State *L, struct lua_TValue *v) {
  (void)L;
  (void)v;
  return FL_EARLY_EXIT;
}

/* Save the function in the AsmInstrData. */
static void savefunction(AsmState *A, AsmInstrData *instrdata) {
  (void)A;
  instrdata->func = fakefunc;
  instrdata->ee = NULL;
}

/* Compile the ir function and save it in the asm function. */
static void compile(struct lua_State *L, IRFunction *F,
                    AsmInstrData *instrdata) {
  AsmState A;
  asmstateinit(&A, L, F);
  initmodule(&A);
  createbblocks(&A);
  verifymodule(&A);
  savefunction(&A, instrdata);
  asmstateclose(&A);
}

/*
 * Target independent section
 */

AsmFunction flasm_getfunction(struct Proto *p, int i) {
  return asmdata(p, i)->func;
}

void flasm_compile(struct lua_State *L, struct Proto *p, int i,
                   struct IRFunction *F) {
  asmdata(p, i) = luaM_new(L, AsmInstrData);
  compile(L, F, asmdata(p, i));
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

