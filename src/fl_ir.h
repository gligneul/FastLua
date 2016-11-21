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
 * Possible types for the ir values.
 */
enum IRType {
  IR_CHAR,
  IR_SHORT,
  IR_INT,
  IR_LUAINT,
  IR_INTPTR,
  IR_LUAFLT,
  IR_PTR,
};

#define IRisintt(t) (t <= IR_INTPTR)
#define IRequivt(t1, t2) (t1 == t2 || (IRisintt(t1) && IRisintt(t2)))

/*
 * Constants
 */
union IRConstant {
  char c;
  short s;
  int i;
  lua_Integer li;
  lua_Number lf;
  l_mem iptr;
  void *p;
};

/*
 * Commands
 */
enum IRCommand {
  IR_CONST,
  IR_GETARG,
  IR_LOAD,
  IR_STORE,
  IR_ADD,
  IR_SUB,
  IR_MUL,
  IR_DIV,
};

/* 
 * SSA values
 */
typedef struct IRValue {
  int id; /* unique id, indexed from 0 to n - 1 */
  int type; /* value type */
  int cmd; /* command that generated the value */
  union { /* the command arguments */
    struct { union IRConstant u; } konst;
    struct { int n; } getarg;
    struct { struct IRValue *mem; } load;
    struct { struct IRValue *mem, *v; } store;
    struct { struct IRValue *l, *r; } binop;
  } args;
} IRValue;

/*
 * Basic blocks
 */
typedef struct IRBBlock {
  IRValue *values;
  int nvalues;
  int sizevalues;
} IRBBlock;

/*
 * Functions
 */
typedef struct IRFunction {
  struct lua_State *L;
  IRBBlock *currbb; /* current basic block */
  IRBBlock *bbs; /* basic block array */
  int nbbs;
  int sizebbs;
} IRFunction;

/*
 * Creates/destroy the ir function.
 */
IRFunction *IRcreatefunc(struct lua_State *L);
void IRdestroyfunc(IRFunction *F);

/*
 * Creates a basic block.
 * The bblock is destroyed with the function.
 */
IRBBlock *IRcreatebb(IRFunction *F);

/*
 * Sets the bblock as the current block.
 */
#define IRsetcurrbb(F, bb) (F->currbb = bb)

/*
 * Creates a command and return the generated value.
 * The value is destroyed with the function.
 */
IRValue* IRconst_c(IRFunction *F, char k);
IRValue* IRconst_s(IRFunction *F, short k);
IRValue* IRconst_i(IRFunction *F, int k);
IRValue* IRconst_li(IRFunction *F, lua_Integer k);
IRValue* IRconst_lf(IRFunction *F, lua_Number k);
IRValue* IRconst_iptr(IRFunction *F, l_mem k);
IRValue* IRconst_p(IRFunction *F, void *k);
IRValue* IRgetarg(IRFunction *F, int type, int n);
IRValue* IRload(IRFunction *F, int type, IRValue *mem);
IRValue* IRstore(IRFunction *F, int type, IRValue *mem, IRValue *val);
IRValue* IRbinop(IRFunction *F, int op, IRValue *l, IRValue *r);

#endif

