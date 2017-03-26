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
#include "fl_logger.h"

/* Optimization level set in LLVM. */
#define ASM_OPT_LEVEL 2

/* Access the asmdata inside the proto. */
#define asmdata(p, i) (fli_getext(p, i)->u.asmdata)

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
  LLVMBasicBlockRef *bbentry;       /* map a IR bb to a LLVM bb entry */
  LLVMBasicBlockRef *bbexit;        /* map a IR bb to a LLVM bb exit */
  LLVMValueRef *values;             /* map a IR value to a LLVM value */
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
    case IR_LONG:   return llvmintof(void *);
    case IR_PTR:    return llvmptr();
    case IR_FLOAT:  return llvmflt();
    case IR_VOID:   return LLVMVoidType();
  }
  return NULL;
}

/* Convert an IR binop to LLVM op */
static LLVMOpcode convertbinop(enum IRBinOp op, enum IRType t) {
  switch (op) {
    case IR_ADD: return t == IR_FLOAT ? LLVMFAdd : LLVMAdd;
    case IR_SUB: return t == IR_FLOAT ? LLVMFSub : LLVMSub;
    case IR_MUL: return t == IR_FLOAT ? LLVMFMul : LLVMMul;
    case IR_DIV: return t == IR_FLOAT ? LLVMFDiv : LLVMSDiv;
  }
  return 0;
}

/* Convert an IR cmp opcode to LLVM int predicate */
static LLVMIntPredicate converticmp(enum IRCmpOp op) {
  switch (op) {
    case IR_NE: return LLVMIntNE;
    case IR_EQ: return LLVMIntEQ;
    case IR_LE: return LLVMIntSLE;
    case IR_LT: return LLVMIntSLT;
    case IR_GE: return LLVMIntSGE;
    case IR_GT: return LLVMIntSGT;
  }
  return 0;
}

/* Convert an IR cmp opcode to LLVM float predicate */
static LLVMIntPredicate convertfcmp(enum IRCmpOp op) {
  switch (op) {
    case IR_NE: return LLVMRealONE;
    case IR_EQ: return LLVMRealOEQ;
    case IR_LE: return LLVMRealOLE;
    case IR_LT: return LLVMRealOLT;
    case IR_GE: return LLVMRealOGE;
    case IR_GT: return LLVMRealOGT;
  }
  return 0;
}

/* Obtain the LLVM value given the IR value. */
static LLVMValueRef getllvmvalue(AsmState *A, IRValue v) {
  return A->values[ir_instr(v)->id];
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
  A->bbentry = luaM_newvector(A->L, ir_nbblocks(), LLVMBasicBlockRef);
  A->bbexit = luaM_newvector(A->L, ir_nbblocks(), LLVMBasicBlockRef);
  A->values = luaM_newvector(A->L, ir_ninstrs(), LLVMValueRef);
}

/* Destroy the state internal data. */
static void asmstateclose(AsmState *A) {
  LLVMDisposeModule(A->module);
  LLVMDisposeBuilder(A->builder);
  luaM_freearray(A->L, A->bbentry, ir_nbblocks());
  luaM_freearray(A->L, A->bbexit, ir_nbblocks());
  luaM_freearray(A->L, A->values, ir_ninstrs());
}

/* Create the basic blocks. */
static void createbblocks(AsmState *A) {
  size_t i, n = ir_nbblocks();
  for (i = 0; i < n; ++i) {
    A->bbentry[i] = A->bbexit[i] = LLVMAppendBasicBlock(A->func, "bb");
  }
}

/* Compile an ir value into a LLVM value. */
static void compilevalue(AsmState *A, IRValue v) {
  LLVMValueRef llvmval = NULL;
  IRInstr *i = ir_instr(v);
  switch (i->tag) {
    case IR_CONST: {
      LLVMTypeRef llvmtype = converttype(i->type);
      if (ir_isintt(i->type))
        llvmval = LLVMConstInt(llvmtype, i->args.konst.i, 1);
      else if (i->type == IR_FLOAT)
        llvmval = LLVMConstReal(llvmtype, i->args.konst.f);
      else if (i->type == IR_PTR) {
        LLVMValueRef addr = LLVMConstInt(llvmintof(void *),
            (ptrdiff_t)i->args.konst.p, 0);
        llvmval = LLVMBuildIntToPtr(A->builder, addr, llvmtype, "");
      }
      else
        fll_error("invalid constant type");
      break;
    }
    case IR_GETARG: {
      llvmval = LLVMGetParam(A->func, i->args.getarg.n);
      break;
    }
    case IR_LOAD: {
      LLVMValueRef addr = getllvmvalue(A, i->args.load.addr);
      enum IRType irtype = i->args.load.type;
      LLVMTypeRef type = llvmptrof(converttype(irtype));
      LLVMValueRef indices[] = {
          LLVMConstInt(llvmint(), i->args.load.offset, 0) };
      LLVMValueRef finaladdr = LLVMBuildGEP(A->builder, addr, indices, 1, "");
      LLVMValueRef ptr = LLVMBuildPointerCast(A->builder, finaladdr, type, "");
      llvmval = LLVMBuildLoad(A->builder, ptr, "");
      break;
    }
    case IR_STORE: {
      LLVMValueRef addr = getllvmvalue(A, i->args.store.addr);
      enum IRType irtype = ir_instr(i->args.store.val)->type;
      LLVMTypeRef type = converttype(irtype);
      LLVMTypeRef ptrtype = llvmptrof(type);
      LLVMValueRef indices[] = {
          LLVMConstInt(llvmint(), i->args.store.offset, 0) };
      LLVMValueRef faddr = LLVMBuildGEP(A->builder, addr, indices, 1, "");
      LLVMValueRef ptr = LLVMBuildPointerCast(A->builder, faddr, ptrtype, "");
      LLVMValueRef val = getllvmvalue(A, i->args.store.val);
      llvmval = LLVMBuildStore(A->builder, val, ptr);
      break;
    }
    case IR_CAST: {
      enum IRType fromtype = ir_instr(i->args.cast.val)->type;
      enum IRType desttype = i->args.cast.type;
      LLVMValueRef val = getllvmvalue(A, i->args.cast.val);
      LLVMTypeRef llvmtype = converttype(desttype);
      LLVMValueRef (*castfunc)(LLVMBuilderRef, LLVMValueRef, LLVMTypeRef,
          const char *) = 0;
      if (ir_isintt(fromtype) && ir_isintt(desttype))
        castfunc = LLVMBuildIntCast;
      else if (ir_isintt(fromtype) && desttype == IR_FLOAT)
        castfunc = LLVMBuildSIToFP;
      else if (fromtype == IR_FLOAT && ir_isintt(desttype))
        castfunc = LLVMBuildFPToSI;
      else
        fll_error("invalid cast");
      llvmval = castfunc(A->builder, val, llvmtype, "");
      break;
    }
    case IR_BINOP: {
      LLVMValueRef l = getllvmvalue(A, i->args.binop.lhs);
      LLVMValueRef r = getllvmvalue(A, i->args.binop.rhs);
      LLVMOpcode op = convertbinop(i->args.binop.op, i->type);
      llvmval = LLVMBuildBinOp(A->builder, op, l, r, "");
      break;
    }
    case IR_CMP: {
      LLVMValueRef l = getllvmvalue(A, i->args.cmp.lhs);
      LLVMValueRef r = getllvmvalue(A, i->args.cmp.rhs);
      LLVMValueRef result = (ir_instr(i->args.cmp.lhs)->type == IR_FLOAT) ?
          LLVMBuildFCmp(A->builder, convertfcmp(i->args.cmp.op), l, r, "") :
          LLVMBuildICmp(A->builder, converticmp(i->args.cmp.op), l, r, "");
      LLVMBasicBlockRef currbb = A->bbexit[v.bblock];
      LLVMBasicBlockRef truebr = A->bbentry[i->args.cmp.dest];
      LLVMBasicBlockRef falsebr = LLVMAppendBasicBlock(A->func, "bb");
      LLVMMoveBasicBlockAfter(falsebr, currbb);
      A->bbexit[v.bblock] = falsebr;
      llvmval = LLVMBuildCondBr(A->builder, result, truebr, falsebr);
      break;
    }
    case IR_JMP: {
      LLVMBasicBlockRef dest = A->bbentry[i->args.jmp.dest];
      llvmval = LLVMBuildBr(A->builder, dest);
      break;
    }
    case IR_RET: {
      LLVMValueRef ret = getllvmvalue(A, i->args.ret.val);
      llvmval = LLVMBuildRet(A->builder, ret);
      break;
    }
    case IR_PHI: {
      LLVMTypeRef type = converttype(i->type);
      llvmval = LLVMBuildPhi(A->builder, type, "");
      break;
    }
  }
  A->values[i->id] = llvmval;
}

/* Link the phi values. */
static void linkphivalues(AsmState *A) {
  irbbv_foreach(&A->irfunc->bblocks, bb, {
    irbb_foreach(bb, instr, {
      if (instr->tag == IR_PHI) {
        IRPhiIncVector *pv = &instr->args.phi.inc;
        size_t i = 0;
        size_t n = irpv_size(pv);
        LLVMValueRef *incvalues = luaM_newvector(A->L, n, LLVMValueRef);
        LLVMBasicBlockRef *incblocks =
            luaM_newvector(A->L, n, LLVMBasicBlockRef);
        LLVMValueRef llvmphi = A->values[instr->id];
        irpv_foreach(pv, inc, {
          incvalues[i] = getllvmvalue(A, inc->value);
          incblocks[i] = A->bbexit[inc->bblock];
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
  size_t bb;
  for (bb = 0; bb < ir_nbblocks(); ++bb) {
    size_t i, n = irbb_size(irbbv_getref(&A->irfunc->bblocks, bb));
    for (i = 0; i < n; ++i) {
      LLVMBasicBlockRef llvmbb = A->bbexit[bb];
      LLVMPositionBuilderAtEnd(A->builder, llvmbb);
      compilevalue(A, ir_createvalue(bb, i));
    }
  }
}

/* Verify if the LLVM module is correct. */
static int verifymodule(AsmState *A) {
  char *error = NULL;
  if (LLVMVerifyModule(A->module, LLVMReturnStatusAction, &error)) {
    fllog("LLVM Error: %s\n", error);
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
  errcode = verifymodule(&A) ||
            savefunction(&A, instrdata);
  asmstateclose(&A);
  return errcode;
}

/*
 * Target independent section
 */

AsmFunction flasm_getfunction(struct Proto *p, Instruction *i) {
  return asmdata(p, i)->func;
}

void flasm_compile(struct lua_State *L, struct Proto *p, Instruction *i,
                   struct IRFunction *F) {
  fli_tojit(p, i);
  asmdata(p, i) = luaM_new(L, AsmInstrData);
  asmdata(p, i)->ee = NULL;
  asmdata(p, i)->func = NULL;
  fllogln("flasm_compile: starting compilation");
  if (compile(L, F, asmdata(p, i)) == ASM_ERROR) {
    flasm_destroy(L, p, i);
    fllogln("flasm_compile: compilation failed");
  }
  else {
    fllogln("flasm_compile: compilation succeed");
  }
}

void flasm_destroy(struct lua_State *L, struct Proto *p, Instruction *i) {
  AsmInstrData *data = asmdata(p, i);
  if (data->ee)
    LLVMDisposeExecutionEngine(data->ee);
  luaM_free(L, data);
  asmdata(p, i) = NULL;
  fli_reset(p, i);
}

void flasm_closeproto(struct lua_State *L, struct Proto *p) {
  fli_foreach(p, i, { if (fli_isexec(i)) flasm_destroy(L, p, i); });
}

