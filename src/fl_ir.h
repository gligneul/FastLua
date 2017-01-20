/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2016 Gabriel de Quadros Ligneul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

#include "fl_container.h"

/* Foward declarations */
struct lua_State;

/* Types defined in this module */
typedef struct IRFunction IRFunction;
typedef struct IRBBlock IRBBlock;
typedef struct IRCommand IRCommand;
typedef struct IRPhiNode IRPhiNode;

/* A SSA value is a reference to the command that generates it. */
typedef struct IRCommand *IRValue;

/* All integer values are promoted to word-size (ptr-size) integers. */
typedef l_mem IRInt;

/* Containers */
TSCC_DECL_VECTOR_WA(IRBBlockVector, ir_bbvec_, IRBBlock *, struct lua_State *)
TSCC_DECL_VECTOR_WA(IRCommandVector, ir_cmdvec_, IRCommand *,
    struct lua_State *)
TSCC_DECL_VECTOR_WA(IRPhiNodeVector, ir_phivec_, IRPhiNode *,
    struct lua_State *)
TSCC_DECL_HASHTABLE_WA(IRBBlockTable, ir_bbtab_, IRBBlock *, int,
    struct lua_State *)
TSCC_DECL_HASHTABLE_WA(IRCommandTable, ir_cmdtab_, IRCommand *, int,
    struct lua_State *)

/* Value types */
enum IRType {
  IR_CHAR,
  IR_SHORT,
  IR_INT,
  IR_LUAINT,
  IR_INTPTR,
  IR_LUAFLT,
  IR_VOID,
};

/* Command types */
enum IRCommandType {
  IR_CONST,
  IR_GETARG,
  IR_LOAD,
  IR_STORE,
  IR_ADD,
  IR_SUB,
  IR_MUL,
  IR_DIV,
  IR_RET,
  IR_PHI,
};

/* union IRConstant
 * Constants are either integers/pointers or float numbers. */
union IRConstant {
  lua_Number f;     /* float */
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
 * Basicaly a list of commands. */
struct IRBBlock {
  IRCommandVector *cmds;
};

/* IRCommand */
struct IRCommand {
  enum IRType type;             /* value type */
  enum IRCommandType cmdtype;   /* command type */
  IRBBlock *bblock;             /* basic block */
  union {                       /* command arguments */
    union IRConstant konst;
    struct { int n; } getarg;
    struct { IRValue mem; enum IRType type; } load;
    struct { IRValue mem, v; } store;
    struct { IRValue l, r; } binop;
    struct { IRValue v; } ret;
    IRPhiNodeVector *phi;
  } args;
};

/* PhiNode
 * Map a basic block to the corresponding SSA value. */
struct IRPhiNode {
  IRValue value;
  IRBBlock *bblock;
};

/* Create/destroy the IR function. */
IRFunction *ir_create(struct lua_State *L);
void _ir_destroy(IRFunction *F);
#define ir_destroy() _ir_destroy(_irfunc)

/* Verify if a type is an integer. */
#define ir_isintt(t) (t <= IR_INTPTR)

/* Create a basic block, set as the current one and returns it. */
IRBBlock *_ir_addbblock(IRFunction *F);
#define ir_addbblock() _ir_addbblock(_irfunc)

/* Create a command and return the generated value. */
IRValue _ir_consti(IRFunction *F, IRInt i);
IRValue _ir_constf(IRFunction *F, lua_Number f);
IRValue _ir_getarg(IRFunction *F, enum IRType type, int n);
IRValue _ir_load(IRFunction *F, enum IRType type, IRValue mem);
IRValue _ir_store(IRFunction *F, enum IRType type, IRValue mem, IRValue val);
IRValue _ir_binop(IRFunction *F, enum IRCommandType op, IRValue l, IRValue r);
IRValue _ir_return(IRFunction *F, IRValue v);
IRValue _ir_phi(IRFunction *F, enum IRType type);
#define ir_consti(i) _ir_consti(_irfunc, i)
#define ir_constf(f) _ir_constf(_irfunc, f)
#define ir_getarg(type, n) _ir_getarg(_irfunc, type, n)
#define ir_load(type, mem) _ir_load(_irfunc, type, mem)
#define ir_store(type, mem, val) _ir_store(_irfunc, type, mem, val)
#define ir_binop(op, l, r) _ir_binop(_irfunc, op, l, r)
#define ir_return(v) _ir_return(_irfunc, v)
#define ir_phi(type) _ir_phi(_irfunc, type)

/* Adds a phi node to the phi command. */
void _ir_addphinode(IRFunction *F, IRCommand *phi, IRValue value,
    IRBBlock *bblock);
#define ir_addphinode(phi, value, bblock) \
    _ir_addphinode(_irfunc, phi, value, bblock)

/* Replace the usage of a value for another in the bblock. */
void _ir_replacevalue(IRFunction *F, IRBBlock *b, IRValue old, IRValue new);
#define ir_replacevalue(b, old, new) _ir_replacevalue(_irfunc, b, old, new)

/* Obtains the address of a struct's field.  */
#define _ir_getfieldptr(F, ptr, strukt, field) \
  (offsetof(strukt, field) == 0 ? ptr : \
    ir_binop(F, IR_ADD, ptr, ir_consti(F, offsetof(strukt, field))))
#define ir_getfieldptr(ptr, strukt, field) \
    _ir_getfieldptr(_irfunc, ptr, strukt, field)

/* Loads the field value. */
#define _ir_loadfield(F, type, ptr, strukt, field) \
  (ir_load(F, type, ir_getfieldptr(F, ptr, strukt, field)))
#define ir_loadfield(type, ptr, strukt, field) \
    _ir_loadfield(_irfunc, type, ptr, strukt, field)
 
/* DEBUG: Prints the function */
void _ir_print(IRFunction *F);
#define ir_print(_irfunc)

#endif

