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

/* Optimization level set in LLVM. */
#define ASM_OPT_LEVEL 2

/* Access the asmdata inside the proto. */
#define asmdata(proto, i) (proto->fl.instr[i].asmdata)

/* Containers */
TSCC_DECL_HASHTABLE_WA(AsmBBlockTable, asm_bbtab_, IRBBlock *, LLVMBasicBlockRef,
    lua_State *)
TSCC_IMPL_HASHTABLE_WA(AsmBBlockTable, asm_bbtab_, IRBBlock *, LLVMBasicBlockRef,
    tscc_ptr_hashfunc, tscc_general_compare, struct lua_State *, luaM_realloc_)

TSCC_DECL_HASHTABLE_WA(AsmValueTable, asm_valtab_, IRValue *, LLVMValueRef,
    lua_State *)
TSCC_IMPL_HASHTABLE_WA(AsmValueTable, asm_valtab_, IRValue *, LLVMValueRef,
    tscc_ptr_hashfunc, tscc_general_compare, struct lua_State *, luaM_realloc_)

/* Return code for functions that may fail. */
enum AsmRetCode {
  ASM_OK,
  ASM_ERROR
};

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
  LLVMBuilderRef builder;           /* LLVM builder */
  AsmBBlockTable *bbtable;          /* map a IR bb to a LLVM bb */
  AsmValueTable *valtable;          /* map a IR bb to a LLVM bb */
} AsmState;

/* IR define trick. */
#define _irfunc (A->irfunc)

/* Macros for creating LLVM types. */
#define llvmintof(T)            LLVMIntType(8 * sizeof(T))
#define llvmint()               llvmintof(void *)
#define llvmptrof(llvmt)        LLVMPointerType(llvmt, 0)
#define llvmptr()               llvmptrof(llvmintof(char))
#define llvmflt()               LLVMDoubleType()

/* Convert an IR type to a LLVM type */
static LLVMTypeRef converttype(enum IRType t) {
  switch (t) {
    case IR_CHAR:   return llvmintof(char);
    case IR_SHORT:  return llvmintof(short);
    case IR_INT:    return llvmintof(int);
    case IR_LUAINT: return llvmintof(lua_Integer);
    case IR_IPTR:   return llvmintof(void *);
    case IR_FLOAT:  return llvmflt();
    case IR_VOID:   return LLVMVoidType();
  }
  return NULL;
}

/* Convert an IR binop to LLVM op */
static LLVMOpcode convertbinop(enum IRBinOp op, enum IRType t) {
  switch (op) {
    case IR_ADD:    return t == IR_FLOAT ? LLVMFAdd : LLVMAdd;
    case IR_SUB:    return t == IR_FLOAT ? LLVMFSub : LLVMSub;
    case IR_MUL:    return t == IR_FLOAT ? LLVMFMul : LLVMMul;
    case IR_DIV:    return t == IR_FLOAT ? LLVMFDiv : LLVMSDiv;
  }
  return 0;
}

/* Convert an IR cmp opcode to LLVM int predicate */
static LLVMIntPredicate converticmp(enum IRCmpOp op) {
  switch (op) {
    case IR_NE:   return LLVMIntNE;
    case IR_EQ:   return LLVMIntEQ;
    case IR_LE:   return LLVMIntSLE;
    case IR_LT:   return LLVMIntSLT;
    case IR_GE:   return LLVMIntSGE;
    case IR_GT:   return LLVMIntSGT;
  }
  return 0;
}

/* Convert an IR cmp opcode to LLVM float predicate */
static LLVMIntPredicate convertfcmp(enum IRCmpOp op) {
  switch (op) {
    case IR_NE:   return LLVMRealONE;
    case IR_EQ:   return LLVMRealOEQ;
    case IR_LE:   return LLVMRealOLE;
    case IR_LT:   return LLVMRealOLT;
    case IR_GE:   return LLVMRealOGE;
    case IR_GT:   return LLVMRealOGT;
  }
  return 0;
}

/* Obtain the LLVM value given the IR value. */
static LLVMValueRef getllvmvalue(AsmState *A, IRValue *v) {
  return asm_valtab_get(A->valtable, v, NULL);
}

/* Obtain the LLVM bblock given the IR bblock. */
static LLVMBasicBlockRef getllvmbb(AsmState *A, IRBBlock *bb) {
  return asm_bbtab_get(A->bbtable, bb, NULL);
}

/* Create the llvm function. */
static LLVMValueRef createllvmfunction(AsmState *A) {
  LLVMTypeRef ret = llvmint();
  LLVMTypeRef args[] = { llvmptr(), llvmptr() };
  LLVMTypeRef functype = LLVMFunctionType(ret, args, 2, 0);
  return LLVMAddFunction(A->module, "f", functype);
}

/* Initalize the AsmState. */
static void asmstateinit(AsmState *A, lua_State *L, IRFunction *irfunc) {
  A->L = L;
  A->irfunc = irfunc;
  A->module = LLVMModuleCreateWithName("fl.asm");
  A->func = createllvmfunction(A);
  A->builder = LLVMCreateBuilder();
  A->bbtable = asm_bbtab_createwa(ir_nbblocks(), L);
  A->valtable = asm_valtab_createwa(ir_nvalues(), L);
}

/* Destroy the state internal data. */
static void asmstateclose(AsmState *A) {
  LLVMDisposeModule(A->module);
  LLVMDisposeBuilder(A->builder);
  asm_bbtab_destroy(A->bbtable);
  asm_valtab_destroy(A->valtable);
}

/* Create the basic blocks. */
static void createbblocks(AsmState *A) {
  ir_foreach_bb(bb, {
    LLVMBasicBlockRef llvmbb = LLVMAppendBasicBlock(A->func, "bb");
    asm_bbtab_insert(A->bbtable, bb, llvmbb);
  });
}

/* Compile an ir value into a LLVM value. */
static void compilevalue(AsmState *A, IRValue *v) {
  LLVMValueRef llvmval = NULL;
  switch (v->instr) {
    case IR_CONST: {
      if (v->type == IR_FLOAT)
        llvmval = LLVMConstReal(llvmflt(), v->args.konst.f);
      else /* v->type == IR_IPTR */
        llvmval = LLVMConstInt(llvmint(), v->args.konst.i, 1);
      break;
    }
    case IR_GETARG: {
      llvmval = LLVMGetParam(A->func, v->args.getarg.n);
      break;
    }
    case IR_LOAD: {
      LLVMValueRef mem = getllvmvalue(A, v->args.load.mem);
      enum IRType irtype = v->args.load.type;
      LLVMTypeRef type = llvmptrof(converttype(irtype));
      LLVMValueRef addr = LLVMBuildPointerCast(A->builder, mem, type, "");
      llvmval = LLVMBuildLoad(A->builder, addr, "");
      if (ir_isintt(irtype) && LLVMTypeOf(llvmval) != llvmint())
        llvmval = LLVMBuildIntCast(A->builder, llvmval, llvmint(), "");
      break;
    }
    case IR_STORE: {
      LLVMValueRef mem = getllvmvalue(A, v->args.store.mem);
      enum IRType irtype = v->args.store.type;
      LLVMTypeRef type = converttype(irtype);
      LLVMTypeRef ptrtype = llvmptrof(type);
      LLVMValueRef addr = LLVMBuildPointerCast(A->builder, mem, ptrtype, "");
      LLVMValueRef val = getllvmvalue(A, v->args.store.v);
      if (ir_isintt(irtype) && type != llvmint())
        val = LLVMBuildIntCast(A->builder, val, type, "");
      llvmval = LLVMBuildStore(A->builder, val, addr);
      break;
    }
    case IR_BINOP: {
      LLVMValueRef l = getllvmvalue(A, v->args.binop.l);
      LLVMValueRef r = getllvmvalue(A, v->args.binop.r);
      if (LLVMTypeOf(l) == llvmptr()) {
        LLVMValueRef indices[] = { r };
        llvmval = LLVMBuildGEP(A->builder, l, indices, 1, "");
      }
      else {
        LLVMOpcode op = convertbinop(v->args.binop.op, v->type);
        llvmval = LLVMBuildBinOp(A->builder, op, l, r, "");
      }
      break;
    }
    case IR_CMP: {
      LLVMValueRef l = getllvmvalue(A, v->args.cmp.l);
      LLVMValueRef r = getllvmvalue(A, v->args.cmp.r);
      LLVMValueRef result = (v->args.cmp.l->type == IR_FLOAT) ?
          LLVMBuildFCmp(A->builder, convertfcmp(v->args.cmp.op), l, r, "") :
          LLVMBuildICmp(A->builder, converticmp(v->args.cmp.op), l, r, "");
      LLVMBasicBlockRef truebr = getllvmbb(A, v->args.cmp.truebr);
      LLVMBasicBlockRef falsebr = getllvmbb(A, v->args.cmp.falsebr);
      llvmval = LLVMBuildCondBr(A->builder, result, truebr, falsebr);
      break;
    }
    case IR_JMP: {
      LLVMBasicBlockRef dest = getllvmbb(A, v->args.jmp);
      llvmval = LLVMBuildBr(A->builder, dest);
      break;
    }
    case IR_RET: {
      LLVMValueRef ret = getllvmvalue(A, v->args.ret.v);
      llvmval = LLVMBuildRet(A->builder, ret);
      break;
    }
    case IR_PHI: {
      LLVMTypeRef type = converttype(v->type);
      llvmval = LLVMBuildPhi(A->builder, type, "");
      break;
    }
  }
  asm_valtab_insert(A->valtable, v, llvmval);
}

/* Link the phi values. */
static void linkphivalues(AsmState *A) {
  ir_foreach_bb(bb, {
    ir_valvec_foreach(bb->values, v, {
      if (v->instr == IR_PHI) {
        size_t i = 0;
        size_t n = ir_phivec_size(v->args.phi);
        LLVMValueRef *incvalues = luaM_newvector(A->L, n, LLVMValueRef);
        LLVMBasicBlockRef *incblocks =
            luaM_newvector(A->L, n, LLVMBasicBlockRef);
        LLVMValueRef llvmphi = getllvmvalue(A, v);
        ir_phivec_foreach(v->args.phi, phinode, {
          incvalues[i] = getllvmvalue(A, phinode->value);
          incblocks[i] = getllvmbb(A, phinode->bblock);
          i++;
        });
        LLVMAddIncoming(llvmphi, incvalues, incblocks, n);
        luaM_freearray(A->L, incvalues, n);
        luaM_freearray(A->L, incblocks, n);
      }
    });
  });
}

/* Compile each basic block. */
static void compilebblocks(AsmState *A) {
  ir_foreach_bb(bb, {
    LLVMBasicBlockRef llvmbb = asm_bbtab_get(A->bbtable, bb, NULL);
    LLVMPositionBuilderAtEnd(A->builder, llvmbb);
    ir_valvec_foreach(bb->values, v, {
      compilevalue(A, v);
    });
  });
}

/* Verify if the LLVM module is correct. */
static int verifymodule(AsmState *A) {
  char *error = NULL;
  if (LLVMVerifyModule(A->module, LLVMReturnStatusAction, &error)) {
    LLVMDisposeMessage(error);
    LLVMDumpModule(A->module);
    return ASM_ERROR;
  }
  return ASM_OK;
}

/* Save the function in the AsmInstrData. */
static int savefunction(AsmState *A, AsmInstrData *data) {
  LLVMModuleRef outmodule;
  char *error = NULL;
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();
  LLVMLinkInMCJIT();
  if (LLVMCreateJITCompilerForModule(&data->ee, A->module, ASM_OPT_LEVEL,
                                     &error)) {
    fprintf(stderr, "LLVMCreateJITCompilerForModule error: %s\n", error);
    return ASM_ERROR;
  }
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpedantic"
  data->func = (AsmFunction)LLVMGetPointerToGlobal(data->ee, A->func);
  #pragma GCC diagnostic pop
  if (LLVMRemoveModule(data->ee, A->module, &outmodule, &error)) {
    fprintf(stderr, "LLVMRemoveModule error: %s\n", error);
    return ASM_ERROR;
  }
  return ASM_OK;
}

/* Compile the ir function and save it in the asm function. */
static int compile(struct lua_State *L, IRFunction *F,
                    AsmInstrData *instrdata) {
  int errcode;
  AsmState A;
  asmstateinit(&A, L, F);
  createbblocks(&A);
  compilebblocks(&A);
  linkphivalues(&A);
  errcode = verifymodule(&A);
  if (errcode == ASM_OK)
    errcode = savefunction(&A, instrdata);
  asmstateclose(&A);
  return errcode;
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
  asmdata(p, i)->ee = NULL;
  asmdata(p, i)->func = NULL;
  fli_tojit(&p->code[i]);
  if (compile(L, F, asmdata(p, i)) == ASM_ERROR) {
    flasm_destroy(L, p, i);
  }
}

void flasm_destroy(struct lua_State *L, struct Proto *p, int i) {
  AsmInstrData *data = asmdata(p, i);
  if (data->ee)
    LLVMDisposeExecutionEngine(data->ee);
  luaM_free(L, data);
  asmdata(p, i) = NULL;
  fli_reset(&p->code[i]);
}

void flasm_closeproto(struct lua_State *L, struct Proto *p) {
  int i;
  for (i = 0; i < p->sizecode; ++i)
    if (GET_OPCODE(p->code[i]) == OP_FORLOOP_JIT)
      flasm_destroy(L, p, i);
}

