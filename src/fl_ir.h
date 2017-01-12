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
 * Intermediate representation used by the fastlua jit.
 */

#ifndef fl_flir_h
#define fl_flir_h

#include <limits.h>

#include "llimits.h"

#include "fl_luacontainer.h"

struct IRBBlock;
struct IRCommand;
struct lua_State;

enum IRType {
  IR_CHAR,
  IR_SHORT,
  IR_INT,
  IR_LUAINT,
  IR_INTPTR,
  IR_LUAFLT,
  IR_VOID,
};

/* All integer values are promoted to word-size (ptr-size) integers.  */
typedef l_mem IRInt;

/* Constants. */
typedef union {
  lua_Number f; /* float */
  IRInt i; /* int/pointers */
} IRConstant;

/* A SSA value is a reference to a command that generates it. */
typedef struct IRCommand *IRValue;

/* Possible command types. */
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

/* TODO comment */
typedef struct IRPhiNode {
  IRValue value;
  IRBBlock bblock;
  struct IRPhiNode *next;
} PhiNode;

typedef struct IRCommand {
  lu_byte type;             /* value type */
  lu_byte cmdtype;          /* command type */
  IRBBlock *bblock;         /* basic block */
  struct IRCommand *next;   /* next element in the linked list */
  union {                   /* command arguments */
    IRConstant konst;
    struct { int n; } getarg;
    struct { IRValue mem; lu_byte type; } load;
    struct { IRValue mem, v; } store;
    struct { IRValue l, r; } binop;
    struct { IRValue v; } ret;
    fllist_decl(IRPhiNode, phi);
  } args;
} IRCommand;

typedef struct IRBBlock {
  fllist_decl(IRCommand, cmds);
  struct IRBBlock *next;
} IRBBlock;

/* Root structure of the module */
typedef struct IRFunction {
  struct lua_State *L;
  IRBBlock *currbb;
  fllist_decl(IRBBlock, bblocks);
} IRFunction;


/*
 * The IRFunction is the main structure of this module.  Most of the routines
 * expects the function object to be named _irfunc. You should either declare
 * the function variable with this name or use a macro.  eg.: 
 * #define _irfunc (mystruct->irfunc)
 * You can also use the alternative routines that receive the explicit function
 * as the first parameter.
 */

/* Create/destroy the IR function. */
IRFunction *flir_create(struct lua_State *L);
void _flir_destroy(struct IRFunction *F);
#define flir_destroy() _flir_destroy

/* Verify if a type is an integer. */
#define flir_isintt(t) (t <= IR_INTPTR)

/* Create a basic block, set as the current one and returns it. */
IRBBlock *_flir_addbblock(IRFunction *F);
#define flir_addbblock() _flir_addbblock(_irfunc)

/* Create a command and return the generated value. */
IRValue _flir_consti(IRFunction *F, IRInt i);
IRValue _flir_constf(IRFunction *F, lua_Number f);
IRValue _flir_getarg(IRFunction *F, lu_byte type, int n);
IRValue _flir_load(IRFunction *F, lu_byte type, IRValue mem);
IRValue _flir_store(IRFunction *F, lu_byte type, IRValue mem, IRValue val);
IRValue _flir_binop(IRFunction *F, lu_byte op, IRValue l, IRValue r);
IRValue _flir_return(IRFunction *F, IRValue v);
IRValue _flir_phi(IRFunction *F, IRType type);
#define flir_consti(i) _flir_consti(_irfunc, i)
#define flir_constf(f) _flir_constf(_irfunc, f)
#define flir_getarg(type, n) _flir_getarg(_irfunc, type, n)
#define flir_load(type, mem) _flir_load(_irfunc, type, mem)
#define flir_store(type, mem, val) _flir_store(_irfunc, type, mem, val)
#define flir_binop(op, l, r) _flir_binop(_irfunc, op, l, r)
#define flir_return(v) _flir_return(_irfunc, v)
#define flir_phi(type) _flir_phi(_irfunc, type)

/* Adds a phi node to the phi command. */
void _flir_addphinode(IRFunction *F, IRCommand *phi, IRValue value,
    IRBBlock *bblock);
#define flir_addphinode(phi, value, bblock) \
    _flir_addphinode(_irfunc, phi, value, bblock)

/* Replace the usage of a value for another in the bblock. */
void _flir_replacevalue(IRFunction *F, IRBBlock *b, IRValue old, IRValue new);
#define flir_replacevalue(b, old, new) _flir_replacevalue(_irfunc, b, old, new)

/* Obtains the address of a struct's field.  */
#define _flir_getfieldptr(F, ptr, strukt, field) \
  (offsetof(strukt, field) == 0 ? ptr : \
    flir_binop(F, IR_ADD, ptr, flir_consti(F, offsetof(strukt, field))))
#define flir_getfieldptr(ptr, strukt, field) \
    _flir_getfieldptr(_irfunc, ptr, strukt, field)

/* Loads the field value. */
#define _flir_loadfield(F, type, ptr, strukt, field) \
  (flir_load(F, type, flir_getfieldptr(F, ptr, strukt, field)))
#define flir_loadfield(type, ptr, strukt, field) \
    _flir_loadfield(_irfunc, type, ptr, strukt, field)
 
/* DEBUG: Prints the function */
void _flir_print(IRFunction *F);
#define flir_print(_irfunc)

#endif

