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

#include <assert.h>

#include "lprefix.h"

#include "lmem.h"

#include "fl_ir.h"

static IRValue* createvalue(IRFunction *F, int type, int cmd) {
  IRBBlock *bb = F->currbb;
  int n = bb->nvalues++;
  luaM_growvector(F->L, bb->values, n, bb->sizevalues, IRValue, MAX_INT, "");
  IRValue* v = &bb->values[n];
  v->id = n;
  v->type = type;
  v->cmd = cmd;
  return v;
}

IRFunction *IRcreatefunc(struct lua_State *L) {
  IRFunction *F = luaM_new(L, IRFunction);
  F->L = L;
  F->currbb = NULL;
  F->bbs = NULL;
  F->nbbs = 0;
  F->sizebbs = 0;
  return F;
}

void IRdestroyfunc(IRFunction *F) {
  int i;
  for (i = 0; i < F->nbbs; ++i) {
    IRBBlock *bb = &F->bbs[i];
    luaM_freearray(F->L, bb->values, bb->sizevalues);
  }
  luaM_freearray(F->L, F->bbs, F->sizebbs);
  luaM_free(F->L, F);
}

IRBBlock *IRcreatebb(IRFunction *F) {
  IRBBlock *bb = NULL;
  luaM_growvector(F->L, F->bbs, F->nbbs, F->sizebbs, IRBBlock, MAX_INT, "");
  bb = &F->bbs[F->nbbs++];
  bb->values = NULL;
  bb->nvalues = 0;
  bb->sizevalues = 0;
  return bb;
}

IRValue* IRconst_c(IRFunction *F, char k) {
  IRValue *v = createvalue(F, IR_CHAR, IR_CONST);
  v->args.konst.u.c = k;
  return v;
}

IRValue* IRconst_s(IRFunction *F, short k) {
  IRValue *v = createvalue(F, IR_SHORT, IR_CONST);
  v->args.konst.u.s = k;
  return v;
}

IRValue* IRconst_i(IRFunction *F, int k) {
  IRValue *v = createvalue(F, IR_INT, IR_CONST);
  v->args.konst.u.i = k;
  return v;
}

IRValue* IRconst_li(IRFunction *F, lua_Integer k) {
  IRValue *v = createvalue(F, IR_LUAINT, IR_CONST);
  v->args.konst.u.li = k;
  return v;
}

IRValue* IRconst_lf(IRFunction *F, lua_Number k) {
  IRValue *v = createvalue(F, IR_LUAFLT, IR_CONST);
  v->args.konst.u.lf = k;
  return v;
}

IRValue* IRconst_iptr(IRFunction *F, l_mem k) {
  IRValue *v = createvalue(F, IR_INTPTR, IR_CONST);
  v->args.konst.u.iptr = k;
  return v;
}

IRValue* IRconst_p(IRFunction *F, void *k) {
  IRValue *v = createvalue(F, IR_PTR, IR_CONST);
  v->args.konst.u.p = k;
  return v;
}

IRValue* IRgetarg(IRFunction *F, int type, int n) {
  IRValue *v = createvalue(F, type, IR_GETARG);
  v->args.getarg.n = n;
  return v;
}

IRValue* IRload(IRFunction *F, int type, IRValue *mem) {
  IRValue *v = createvalue(F, type, IR_LOAD);
  v->args.load.mem = mem;
  return v;
}

IRValue* IRstore(IRFunction *F, int type, IRValue *mem, IRValue *val) {
  IRValue *v = createvalue(F, type, IR_STORE);
  assert(IRequivt(type, v->type));
  v->args.store.mem = mem;
  v->args.store.v = val;
  return v;
}

IRValue* IRbinop(IRFunction *F, int op, IRValue *l, IRValue *r) {
  int outt = IRisintt(l->type) ? IR_LUAINT : l->type; /* promote to luaint */
  IRValue *v = createvalue(F, outt, op);
  assert(IRequivt(l->type, r->type));
  v->args.binop.l = l;
  v->args.binop.r = r;
  return v;
}

