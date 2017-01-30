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
 * The fastlua jit transforms the lua bytecode into this representation.
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

#include <limits.h>

#include "llimits.h"

#include "fl_containers.h"

/* Foward declarations */
struct lua_State;

/* Types defined in this module */
typedef struct IRFunction IRFunction;
typedef struct IRBBlock IRBBlock;
typedef struct IRValue IRValue;
typedef struct IRPhiNode IRPhiNode;

/* All integer values are promoted to word-size (ptr-size) integers. */
typedef l_mem IRInt;

/* Float numbers are based on lua number. */
typedef lua_Number IRFloat;

/* Containers */
TSCC_DECL_VECTOR_WA(IRBBlockVector, ir_bbvec_, IRBBlock *, struct lua_State *)
#define ir_bbvec_foreach(vec, val, _cmd) \
    TSCC_VECTOR_FOREACH(ir_bbvec_, vec, IRBBlock *, val, _cmd)

TSCC_DECL_VECTOR_WA(IRValueVector, ir_valvec_, IRValue *, struct lua_State *)
#define ir_valvec_foreach(vec, val, _cmd) \
    TSCC_VECTOR_FOREACH(ir_valvec_, vec, IRValue *, val, _cmd)

TSCC_DECL_VECTOR_WA(IRPhiNodeVector, ir_phivec_, IRPhiNode *,
    struct lua_State *)
#define ir_phivec_foreach(vec, val, _cmd) \
    TSCC_VECTOR_FOREACH(ir_phivec_, vec, IRPhiNode *, val, _cmd)

TSCC_DECL_HASHTABLE_WA(IRBBlockTable, ir_bbtab_, IRBBlock *, int,
    struct lua_State *)
TSCC_DECL_HASHTABLE_WA(IRValueTable, ir_valtab_, IRValue *, int,
    struct lua_State *)

/* Value types */
enum IRType {
  IR_CHAR,
  IR_SHORT,
  IR_INT,
  IR_LUAINT,
  IR_IPTR,
  IR_FLOAT,
  IR_VOID
};

/* Intruction types */
enum IRInstruction {
  IR_CONST = IR_VOID + 1,
  IR_GETARG,
  IR_LOAD,
  IR_STORE,
  IR_BINOP,
  IR_CMP,
  IR_JMP,
  IR_RET,
  IR_PHI
};

/* Binary operations */
enum IRBinOp {
  IR_ADD = IR_PHI + 1,
  IR_SUB,
  IR_MUL,
  IR_DIV
};

/* Comparisons */
enum IRCmpOp {
  IR_NE = IR_DIV + 1,
  IR_EQ,
  IR_LE,
  IR_LT,
  IR_GE,
  IR_GT
};

/* union IRConstant
 * Constants are either integers/pointers or float numbers. */
union IRConstant {
  IRFloat f;        /* float */
  IRInt i;          /* int/pointers */
};

/* IRFunction
 * Root structure of the module. */
struct IRFunction {
  struct lua_State *L;      /* lua state */
  IRBBlock *currbb;         /* current basic block */
  IRBBlockVector *bblocks;  /* list of basic blocks */
};

/* IRBBlock
 * Basicaly a list of values. */
struct IRBBlock {
  IRValueVector *values;
};

/* IRValue
 * Contains the instruction that generates it. */
struct IRValue {
  enum IRType type;             /* value type */
  enum IRInstruction instr;     /* instruction */
  IRBBlock *bblock;             /* parent basic block */
  union {                       /* instruction arguments */
    union IRConstant konst;
    struct { int n; } getarg;
    struct { IRValue *mem; enum IRType type; } load;
    struct { IRValue *mem, *v; enum IRType type; } store;
    struct { enum IRBinOp op; IRValue *l, *r; } binop;
    struct { enum IRCmpOp op; IRValue *l, *r;
             IRBBlock *truebr, *falsebr; } cmp;
    IRBBlock *jmp;
    struct { IRValue *v; } ret;
    IRPhiNodeVector *phi;
  } args;
};

/* PhiNode
 * Map a basic block to the corresponding SSA value. */
struct IRPhiNode {
  IRValue *value;
  IRBBlock *bblock;
};

/* Create/destroy the IR function. */
IRFunction *ir_create(struct lua_State *L);
void ir_destroy(IRFunction *F);

/* Verify if a type is an integer. */
#define ir_isintt(t) (t <= IR_IPTR)

/* Create a basic block and returns it. */
IRBBlock *_ir_addbblock(IRFunction *F);
#define ir_addbblock() _ir_addbblock(_irfunc)

/* Insert the basic block after bb and returns it. */
IRBBlock *_ir_insertbblock(IRFunction *F, IRBBlock *prevbb);
#define ir_insertbblock(prevbb) _ir_insertbblock(_irfunc, prevbb)

/* Obtain the basic block in the position. */
IRBBlock *_ir_getbblock(IRFunction *F, size_t pos);
#define ir_getbblock(pos) (_ir_getbblock(_irfunc, pos))

/* Access the current basic block. */
#define ir_currbblock() (_irfunc->currbb)

/* Add a value to the current basic block and return it. */
IRValue *_ir_consti(IRFunction *F, IRInt i);
IRValue *_ir_constf(IRFunction *F, IRFloat f);
IRValue *_ir_getarg(IRFunction *F, enum IRType type, int n);
IRValue *_ir_load(IRFunction *F, enum IRType type, IRValue *mem);
IRValue *_ir_store(IRFunction *F, enum IRType type, IRValue *mem,
                   IRValue *val);
IRValue *_ir_binop(IRFunction *F, enum IRBinOp op, IRValue *l, IRValue *r);
IRValue *_ir_cmp(IRFunction *F, enum IRCmpOp op, IRValue *l, IRValue *r,
                 IRBBlock *truebr, IRBBlock *falsebr);
IRValue *_ir_jmp(IRFunction *F, IRBBlock *bb);
IRValue *_ir_return(IRFunction *F, IRValue *v);
IRValue *_ir_phi(IRFunction *F, enum IRType type);
#define ir_consti(i) _ir_consti(_irfunc, i)
#define ir_constf(f) _ir_constf(_irfunc, f)
#define ir_getarg(type, n) _ir_getarg(_irfunc, type, n)
#define ir_load(type, mem) _ir_load(_irfunc, type, mem)
#define ir_store(type, mem, val) _ir_store(_irfunc, type, mem, val)
#define ir_binop(op, l, r) _ir_binop(_irfunc, op, l, r)
#define ir_cmp(op, l, r, truebr, falsebr) \
    _ir_cmp(_irfunc, op, l, r, truebr, falsebr)
#define ir_jmp(bb) _ir_jmp(_irfunc, bb)
#define ir_return(v) _ir_return(_irfunc, v)
#define ir_phi(type) _ir_phi(_irfunc, type)

/* Adds a phi node to the phi value. */
void _ir_addphinode(IRFunction *F, IRValue *phi, IRValue *value,
    IRBBlock *bblock);
#define ir_addphinode(phi, value, bblock) \
    _ir_addphinode(_irfunc, phi, value, bblock)

/* Move the value to the position in the same basic block. */
void _ir_move(IRFunction *F, IRBBlock *bb, size_t from, size_t to);
#define ir_move(bb, from, to) _ir_move(_irfunc, bb, from, to)

/* Replace the usage of a value for another in the bblock. */
void _ir_replacevalue(IRFunction *F, IRBBlock *b, IRValue *old, IRValue *new);
#define ir_replacevalue(b, old, new) _ir_replacevalue(_irfunc, b, old, new)

/* Obtains the address of a struct's field.  */
#define _ir_getfieldaddr(F, ptr, strukt, field) \
  (offsetof(strukt, field) == 0 ? ptr : \
    _ir_binop(F, IR_ADD, ptr, _ir_consti(F, offsetof(strukt, field))))
#define ir_getfieldaddr(ptr, strukt, field) \
    _ir_getfieldaddr(_irfunc, ptr, strukt, field)

/* Loads the field value. */
#define _ir_loadfield(F, type, ptr, strukt, field) \
  (_ir_load(F, type, _ir_getfieldaddr(F, ptr, strukt, field)))
#define ir_loadfield(type, ptr, strukt, field) \
    _ir_loadfield(_irfunc, type, ptr, strukt, field)
 
/* DEBUG: Prints the function */
void _ir_print(IRFunction *F);
#define ir_print() _ir_print(_irfunc)

#endif

