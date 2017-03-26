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
 * This is simple SSA intermetiate representation.
 *
 * The FastLua JIT transforms the lua bytecode into this representation.
 * Latter, the IR is optimized and compiled into machine code.
 *
 * The IRFunction is the main structure of this module. Most of the routines
 * expects the IRFunction object to be named _irfunc. The routine
 * ir_addbblock(), for example, receive the _irfunc as a implicit parameter.
 * You should either declare the IRFunction object with this name or use a
 * macro. Example: #define _irfunc (mystruct->irfunc).
 * You can also use the alternative routines that receive the explicit
 * IRFunction object as the first parameter. Example: _ir_addbblock(my_iffunc).
 */

#ifndef fl_ir_h
#define fl_ir_h

#include "llimits.h"

#include "fl_containers.h"

/* Lua definitions */
struct lua_State;
typedef lua_Integer IRInt;
typedef lua_Number IRFloat;

/* Basic blocks and instructions are referenced by indices. */
typedef int IRName;

/* Resulting type of a IR instruction. */
enum IRType {
  IR_CHAR,
  IR_SHORT,
  IR_INT,
  IR_LUAINT,
  IR_LONG,
  IR_PTR,
  IR_FLOAT,
  IR_VOID
};

/* Intruction tag. */
enum IRInstrTag {
  IR_CONST = IR_VOID + 1,
  IR_GETARG,
  IR_LOAD,
  IR_STORE,
  IR_CAST,
  IR_BINOP,
  IR_CMP,
  IR_JMP,
  IR_RET,
  IR_PHI
};

/* Binary operations. */
enum IRBinOp {
  IR_ADD = IR_PHI + 1,
  IR_SUB,
  IR_MUL,
  IR_DIV
};

/* Comparison operations. */
enum IRCmpOp {
  IR_NE = IR_DIV + 1,
  IR_EQ,
  IR_LE,
  IR_LT,
  IR_GE,
  IR_GT
};

/* Values are references to a instruction inside a basic block. */
typedef struct IRValue {
  IRName bblock;
  IRName instr;
} IRValue;

/* Map a basic block to the corresponding SSA value. */
typedef struct IRPhiInc {
  IRValue value;
  IRName bblock;
} IRPhiInc;

/* Vector of phi incoming values. */
TSCC_DECL_VECTOR(IRPhiIncVector, irpv_, IRPhiInc)
#define irpv_foreach(vec, val, cmd) \
    TSCC_VECTOR_FOREACH(irpv_, vec, IRPhiInc, val, cmd)

/* SSA Instruction. */
typedef struct IRInstr {
  enum IRType type;             /* resulting type */
  enum IRInstrTag tag;          /* instruction tag */
  IRName bblock;                /* parent basic block */
  IRName id;                    /* unique instruction id */
  union {                       /* arguments */
    union { IRFloat f; IRInt i; void *p; } konst;
    struct { int n; } getarg;
    struct { IRValue addr; size_t offset; enum IRType type; } load;
    struct { IRValue addr, val; size_t offset; } store;
    struct { IRValue val; enum IRType type; } cast;
    struct { enum IRBinOp op; IRValue lhs, rhs; } binop;
    struct { enum IRCmpOp op; IRValue lhs, rhs; IRName dest; } cmp;
    struct { IRName dest; } jmp;
    struct { IRValue val; } ret;
    struct { IRPhiIncVector inc; } phi;
  } args;
} IRInstr;

/* Basic blocks are a vector of ir instructions. */
TSCC_DECL_VECTOR(IRBBlock, irbb_, IRInstr)
#define irbb_foreach(vec, val, cmd) \
    TSCC_VECTOR_FOREACH(irbb_, vec, IRInstr, val, cmd)

/* Vector of basic blocks. */
TSCC_DECL_VECTOR(IRBBlockVector, irbbv_, IRBBlock)
#define irbbv_foreach(vec, val, cmd) \
    TSCC_VECTOR_FOREACH(irbbv_, vec, IRBBlock, val, cmd)

/* Root structure of the module. */
typedef struct IRFunction {
  struct lua_State *L;              /* lua state */
  IRName currbb;                    /* current basic block */
  IRName ninstrs;                   /* number of instructions */
  IRBBlockVector bblocks;           /* list of basic blocks */
} IRFunction;

/* Null names */
#define IRNull -1
#define ir_isnull(x) (x == IRNull)

/* Create a value given the basic block and instruction. */
IRValue ir_createvalue(IRName bblock, IRName instr);

/* Create a null value. */
#define ir_nullvalue() ir_createvalue(IRNull, IRNull)

/* Compare two values. */
#define ir_cmpvalue(a, b) ((a).bblock == (b).bblock) && (a).instr == (b).instr)

/* Check if a value is null. */
#define ir_isnullvalue(v) (ir_isnull((v).bblock) || ir_isnull((v).instr))

/* Initialize the IR function. */
void _ir_init(IRFunction *F, struct lua_State *L);
#define ir_init(L) _ir_init(_irfunc, L)

/* Deallocate the IR function data. */
void _ir_close(IRFunction *F);
#define ir_close() _ir_close(_irfunc)

/* Verify if a IR type is integer. */
#define ir_isintt(t) (t <= IR_LONG)

/* Add a basic block and return it. */
IRName _ir_addbblock(IRFunction *F);
#define ir_addbblock() _ir_addbblock(_irfunc)

/* Set the current basic block. */
#define _ir_setbblock(F, bblock) (F->currbb = bblock)
#define ir_setbblock(bblock) _ir_setbblock(_irfunc, bblock)

/* Obtain the instruction given the value. */
IRInstr *_ir_instr(IRFunction *F, IRValue v);
#define ir_instr(v) _ir_instr(_irfunc, v)

/* Get the number of basic blocks. */
#define _ir_nbblocks(F) (irbbv_size(&F->bblocks))
#define ir_nbblocks() _ir_nbblocks(_irfunc)

/* Get the number of instructions. */
#define _ir_ninstrs(F) (F->ninstrs)
#define ir_ninstrs() _ir_ninstrs(_irfunc)

/* Add a instruction to the current basic block and return the
 * resulting value. */
IRValue _ir_consti(IRFunction *F, IRInt i, enum IRType type);
IRValue _ir_constf(IRFunction *F, IRFloat f);
IRValue _ir_constp(IRFunction *F, void *p);
IRValue _ir_getarg(IRFunction *F, enum IRType type, int n);
IRValue _ir_load(IRFunction *F, enum IRType type, IRValue addr, int offset);
IRValue _ir_store(IRFunction *F, IRValue addr, IRValue val, int offset);
IRValue _ir_cast(IRFunction *F, IRValue val, enum IRType type);
IRValue _ir_binop(IRFunction *F, enum IRBinOp op, IRValue lhs, IRValue rhs);
IRValue _ir_cmp(IRFunction *F, enum IRCmpOp op, IRValue lhs, IRValue rhs,
                IRName dest);
IRValue _ir_jmp(IRFunction *F, IRName dest);
IRValue _ir_return(IRFunction *F, IRValue v);
IRValue _ir_phi(IRFunction *F, enum IRType type);
#define ir_consti(i, type) _ir_consti(_irfunc, i, type)
#define ir_constf(f) _ir_constf(_irfunc, f)
#define ir_constp(p) _ir_constp(_irfunc, p)
#define ir_getarg(type, n) _ir_getarg(_irfunc, type, n)
#define ir_load(type, addr, offset) _ir_load(_irfunc, type, addr, offset)
#define ir_store(addr, val, offset) _ir_store(_irfunc, addr, val, offset)
#define ir_cast(v, type) _ir_cast(_irfunc, v, type)
#define ir_binop(op, l, r) _ir_binop(_irfunc, op, l, r)
#define ir_cmp(op, l, r, jmp) _ir_cmp(_irfunc, op, l, r, jmp)
#define ir_jmp(bb) _ir_jmp(_irfunc, bb)
#define ir_return(v) _ir_return(_irfunc, v)
#define ir_phi(type) _ir_phi(_irfunc, type)

/* Add a phi incoming value to the phi instruction. */
void _ir_addphiinc(IRFunction *F, IRValue phi, IRValue value, IRName bblock);
#define ir_addphiinc(phi, value, bblock) \
    _ir_addphiinc(_irfunc, phi, value, bblock)

/* DEBUG: Print the function with fllog. */
void _ir_print(IRFunction *F);
#define ir_print() _ir_print(_irfunc)

#endif

