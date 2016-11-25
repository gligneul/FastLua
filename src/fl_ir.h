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
 * The compilations steps are: lua bytecode -> fastlua ir -> machine code.
 */

#ifndef fl_ir_h
#define fl_ir_h

#include "llimits.h"

struct lua_State;

/*
 * Ids (indices) are used in this representation instead of pointers to
 * reduce the structures size.
 */
typedef unsigned short IRId;

/*
 * All small integer values are promoted to word-size (ptr-size) integers.
 */
typedef l_mem IRInt;

/*
 * Types.
 */
enum IRType {
  IR_CHAR,
  IR_SHORT,
  IR_INT,
  IR_LUAINT,
  IR_INTPTR,
  IR_LUAFLT,
  IR_PTR,
  IR_VOID,
};

/* Verifies if a type is an integer. */
#define flI_isintt(t) (t <= IR_INTPTR)

/*
 * Constants.
 */
typedef union {
  lua_Number f; /* float */
  l_mem i; /* int */
  void *p;
} IRUConstant;

/*
 * Commands.
 */
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
};

/*
 * Values are references to it's basic block and command.
 */
typedef struct IRValue {
  IRId bb;
  IRId cmd;
} IRValue;

/* 
 * Commands
 */
typedef struct IRCommand {
  lu_byte type; /* value type */
  lu_byte cmdtype; /* command type */
  union { /* the command arguments */
    IRUConstant konst;
    struct { int n; } getarg;
    struct { IRValue mem; } load;
    struct { IRValue mem, v; } store;
    struct { IRValue l, r; } binop;
    struct { IRValue v; } ret;
  } args;
} IRCommand;

/*
 * Basic blocks
 */
typedef struct IRBBlock {
  IRCommand *cmds; /* commands vector */
  int ncmds;
  int sizecmds;
} IRBBlock;

/*
 * Functions
 */
typedef struct IRFunction {
  struct lua_State *L;
  IRBBlock *bbs; /* basic blocks vector */
  int nbbs;
  int sizebbs;
  IRId currbb; /* current basic block */
} IRFunction;

/* Given a bblock id, obtains the bblock. */
#define flI_getbb(F, id) (&F->bbs[id])

/* Given value, obtains the command. */
#define flI_getcmd(F, value) (&F->bbs[(value).bb].cmds[(value).cmd])

/*
 * Creates/destroy the ir function.
 */
IRFunction *flI_createfunc(struct lua_State *L);
void flI_destroyfunc(IRFunction *F);

/*
 * Creates a basic block and returns it id.
 */
IRId flI_createbb(IRFunction *F);

/*
 * Sets the bblock as the current block.
 */
#define flI_setcurrbb(F, bbid) (F->currbb = bbid)

/*
 * Creates a command and return the generated value.
 */
IRValue flI_consti(IRFunction *F, l_mem i);
IRValue flI_constf(IRFunction *F, lua_Number f);
IRValue flI_constp(IRFunction *F, void *p);
IRValue flI_getarg(IRFunction *F, lu_byte type, int n);
IRValue flI_load(IRFunction *F, lu_byte type, IRValue mem);
IRValue flI_store(IRFunction *F, lu_byte type, IRValue mem, IRValue val);
IRValue flI_binop(IRFunction *F, lu_byte op, IRValue l, IRValue r);
IRValue flI_return(IRFunction *F, IRValue v);
 
/*
 * DEBUG: Prints the function
 */
#define flI_log(...) fprintf(stderr, __VA_ARGS__)
void flI_print(IRFunction *F);

#endif

