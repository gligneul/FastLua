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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "lprefix.h"

#include "lmem.h"

#include "fl_ir.h"

/* Conteiners implementation */
TSCC_IMPL_VECTOR_WA(IRBBlockVector, ir_bbvec_, IRBBlock *, struct lua_State *,
    luaM_realloc_)
TSCC_IMPL_VECTOR_WA(IRValueVector, ir_valvec_, IRValue *, struct lua_State *,
    luaM_realloc_)
TSCC_IMPL_VECTOR_WA(IRPhiNodeVector, ir_phivec_, IRPhiNode *,
    struct lua_State *, luaM_realloc_)
TSCC_DECL_HASHTABLE_WA(IRBBlockTable, ir_bbtab_, IRBBlock *, int,
    struct lua_State *)
TSCC_IMPL_HASHTABLE_WA(IRBBlockTable, ir_bbtab_, IRBBlock *, int,
    tscc_ptr_hashfunc, tscc_general_compare, struct lua_State *, luaM_realloc_)
TSCC_DECL_HASHTABLE_WA(IRValueTable, ir_valtab_, IRValue *, int,
    struct lua_State *)
TSCC_IMPL_HASHTABLE_WA(IRValueTable, ir_valtab_, IRValue *, int,
    tscc_ptr_hashfunc, tscc_general_compare, struct lua_State *, luaM_realloc_)

IRFunction *ir_create(struct lua_State *L) {
  IRFunction *F = luaM_new(L, IRFunction);
  F->L = L;
  F->currbb = NULL;
  F->bblocks = ir_bbvec_createwa(L);
  return F;
}

void ir_destroy(IRFunction *F) {
  lua_State *L = F->L;
  ir_bbvec_foreach(F->bblocks, bb, {
    ir_valvec_foreach(bb->values, v, {
      if (v->instr == IR_PHI) {
        ir_phivec_foreach(v->args.phi, phi, luaM_free(L, phi));
        ir_phivec_destroy(v->args.phi);
      }
      luaM_free(L, v);
    });
    ir_valvec_destroy(bb->values);
    luaM_free(L, bb);
  });
  ir_bbvec_destroy(F->bblocks);
  luaM_free(L, F);
}

/* Creates a basic block */
static IRBBlock *createbblock(IRFunction *F) {
  IRBBlock *bb = luaM_new(F->L, IRBBlock);
  bb->values = ir_valvec_createwa(F->L);
  return bb;
}

IRBBlock *_ir_addbblock(IRFunction *F) {
  IRBBlock *bb = createbblock(F);
  ir_bbvec_push(F->bblocks, bb);
  return bb;
}

IRBBlock *_ir_insertbblock(IRFunction *F, IRBBlock *prevbb) {
  IRBBlock *bb = createbblock(F);
  size_t pos = 1;
  ir_bbvec_foreach(F->bblocks, thisbb, {
    if (thisbb == prevbb)
      break;
    else
      pos++;
  });
  ir_bbvec_insert(F->bblocks, pos, bb);
  return bb;
}

size_t _ir_nvalues(IRFunction *F) {
  size_t n = 0;
  ir_bbvec_foreach(F->bblocks, bb, {
    n += ir_valvec_size(bb->values);
  });
  return n;
}

/* Create a value in the current basic block. */
static IRValue *createvalue(IRFunction *F, enum IRType type,
                            enum IRInstruction instr) {
  IRBBlock *bb = F->currbb;
  IRValue *v = luaM_new(F->L, IRValue);
  v->type = type;
  v->instr = instr;
  v->bblock = bb;
  ir_valvec_push(bb->values, v);
  return v;
}

IRValue *_ir_consti(IRFunction *F, IRInt i) {
  IRValue *v = createvalue(F, IR_LONG, IR_CONST);
  v->args.konst.i = i;
  return v;
}

IRValue *_ir_constf(IRFunction *F, IRFloat f) {
  IRValue *v = createvalue(F, IR_FLOAT, IR_CONST);
  v->args.konst.f = f;
  return v;
}

IRValue *_ir_getarg(IRFunction *F, enum IRType type, int n) {
  IRValue *v = createvalue(F, type, IR_GETARG);
  v->args.getarg.n = n;
  return v;
}

IRValue *_ir_load(IRFunction *F, enum IRType type, IRValue *mem,
                  int offset) {
  /* promote integers to intptr */
  enum IRType finaltype = ir_isintt(type) ? IR_LONG : type;
  IRValue *v = createvalue(F, finaltype, IR_LOAD);
  v->args.load.mem = mem;
  v->args.load.offset = offset;
  v->args.load.type = type;
  return v;
}

IRValue *_ir_store(IRFunction *F, enum IRType type, IRValue *mem,
                   IRValue *val, int offset) {
  IRValue *v = createvalue(F, IR_VOID, IR_STORE);
  v->args.store.mem = mem;
  v->args.store.v = val;
  v->args.store.offset = offset;
  v->args.store.type = type;
  assert(val->type == type || (val->type == IR_LONG && ir_isintt(type)));
  assert(mem->type == IR_PTR);
  return v;
}

IRValue *_ir_binop(IRFunction *F, enum IRBinOp op, IRValue *l, IRValue *r) {
  IRValue *v = createvalue(F, l->type, IR_BINOP);
  v->args.binop.op = op;
  v->args.binop.l = l;
  v->args.binop.r = r;
  assert(l->type == r->type);
  return v;
}

IRValue *_ir_cmp(IRFunction *F, enum IRCmpOp op, IRValue *l, IRValue *r,
                 IRBBlock *truebr, IRBBlock *falsebr) {
  IRValue *v = createvalue(F, IR_VOID, IR_CMP);
  v->args.cmp.op = op;
  v->args.cmp.l = l;
  v->args.cmp.r = r;
  v->args.cmp.truebr = truebr;
  v->args.cmp.falsebr = falsebr;
  assert(l->type == r->type);
  return v;
}

IRValue *_ir_jmp(IRFunction *F, IRBBlock *bb) {
  IRValue *v = createvalue(F, IR_VOID, IR_JMP);
  v->args.jmp = bb;
  return v;
}

IRValue *_ir_return(IRFunction *F, IRValue *val) {
  IRValue *v = createvalue(F, IR_VOID, IR_RET);
  v->args.ret.v = val;
  return v;
}

IRValue *_ir_phi(IRFunction *F, enum IRType type) {
  IRValue *v = createvalue(F, type, IR_PHI);
  v->args.phi = ir_phivec_createwa(F->L);
  return v;
}

void _ir_addphinode(IRFunction *F, IRValue *v, IRValue *value,
                    IRBBlock *bblock) {
  IRPhiNode *phi = luaM_new(F->L, IRPhiNode);
  phi->value = value;
  phi->bblock = bblock;
  ir_phivec_push(v->args.phi, phi);
  assert(v->instr == IR_PHI);
  assert(v->type == value->type);
}

void _ir_move(IRFunction *F, IRBBlock *bb, size_t from, size_t to) {
  IRValue *v = ir_valvec_get(bb->values, from);
  (void)F;
  ir_valvec_erase(bb->values, from);
  ir_valvec_insert(bb->values, to, v);
}

/* Replace helper */
#define replacehelper(cell, old, new) \
  do { if (cell == old) cell = new; } while (0)

static void replacerec(IRFunction *F, IRBBlock *bb, IRValue *old, IRValue *new,
                       IRBBlockTable *visited)
{
  if (ir_bbtab_contains(visited, bb))
    return;
  else
    ir_bbtab_insert(visited, bb, 1);
  ir_valvec_foreach(bb->values, v, {
    if (v == old || v == new)
      continue;
    switch (v->instr) {
      case IR_CONST: case IR_GETARG: case IR_JMP:
        /* do nothing */
        break;
      case IR_LOAD:
        replacehelper(v->args.load.mem, old, new);
        break;
      case IR_STORE:
        replacehelper(v->args.store.mem, old, new);
        replacehelper(v->args.store.v, old, new);
        break;
      case IR_BINOP:
        replacehelper(v->args.binop.l, old, new);
        replacehelper(v->args.binop.r, old, new);
        break;
      case IR_CMP:
        replacehelper(v->args.cmp.l, old, new);
        replacehelper(v->args.cmp.r, old, new);
        replacerec(F, v->args.cmp.truebr, old, new, visited);
        replacerec(F, v->args.cmp.falsebr, old, new, visited);
        break;
      case IR_RET:
        replacehelper(v->args.ret.v, old, new);
        break;
      case IR_PHI:
        ir_phivec_foreach(v->args.phi, phi, {
          replacehelper(phi->value, old, new);
          replacerec(F, phi->bblock, old, new, visited);
        });
        break;
    }
  });
}

void _ir_replacevalue(IRFunction *F, IRBBlock *bb, IRValue *old, IRValue *new) {
  size_t n = ir_bbvec_size(F->bblocks);
  IRBBlockTable *visited = ir_bbtab_createwa(n, F->L);
  replacerec(F, bb, old, new, visited);
  ir_bbtab_destroy(visited);
}

/*
 * Printing functions for debug
 */

static void ir_log(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

static void printtype(enum IRType type) {
  switch (type) {
    case IR_VOID:   ir_log("void"); break;
    case IR_CHAR:   ir_log("char"); break;
    case IR_SHORT:  ir_log("short"); break;
    case IR_INT:    ir_log("int"); break;
    case IR_LUAINT: ir_log("luaint"); break;
    case IR_LONG:   ir_log("long"); break;
    case IR_PTR:    ir_log("ptr"); break;
    case IR_FLOAT:  ir_log("luafloat"); break;
  }
}

static void printconst(enum IRType type, union IRConstant k) {
  ir_log("(const ");
  printtype(type);
  ir_log(" ");
  switch (type) {
    case IR_LONG:   ir_log("%td", k.i); break;
    case IR_PTR:    ir_log("%p", k.p); break;
    case IR_FLOAT:  ir_log("%f", k.f); break;
    default: assert(0); break;
  }
  ir_log(")");
}

static void printbinop(enum IRBinOp op) {
  switch (op) {
    case IR_ADD: ir_log("add"); break;
    case IR_SUB: ir_log("sub"); break;
    case IR_MUL: ir_log("mul"); break;
    case IR_DIV: ir_log("div"); break;
  }
}

static void printcmpop(enum IRCmpOp op) {
  switch (op) {
    case IR_NE: ir_log("!="); break;
    case IR_EQ: ir_log("=="); break;
    case IR_LE: ir_log("<="); break;
    case IR_LT: ir_log("<"); break;
    case IR_GE: ir_log(">="); break;
    case IR_GT: ir_log(">"); break;
  }
}

static void printvalue(IRValue *v, IRValueTable *indices) {
  if (v->instr == IR_CONST)
    printconst(v->type, v->args.konst);
  else
    ir_log("%%%d", ir_valtab_get(indices, v, -1));
}

static void printbblock(IRBBlock *bb, IRBBlockTable *indices) {
  ir_log("bb_%d", ir_bbtab_get(indices, bb, -1));
}

static void printinstr(IRValue *v, IRBBlockTable *bbindices,
                     IRValueTable *valindices) {
  if (v->instr == IR_CONST)
    return;
  ir_log("  ");
  if (v->type != IR_VOID) {
    printvalue(v, valindices);
    /* ir_log(" : "); printtype(v->type); */
    ir_log(" = ");
  }
  switch (v->instr) {
    case IR_CONST:
      /* do nothing */
      break;
    case IR_GETARG: {
      ir_log("getarg %d", v->args.getarg.n);
      break;
    }
    case IR_LOAD: {
      int offset = v->args.load.offset;
      ir_log("load ");
      printtype(v->args.load.type);
      ir_log(" ");
      if (offset > 0)
        ir_log("%d(", offset);
      printvalue(v->args.load.mem, valindices);
      if (offset > 0)
        ir_log(")");
      break;
    }
    case IR_STORE: {
      int offset = v->args.store.offset;
      ir_log("store ");
      if (offset > 0)
        ir_log("%d(", offset);
      printvalue(v->args.store.mem, valindices);
      if (offset > 0)
        ir_log(")");
      ir_log(" <- ");
      printtype(v->args.store.type);
      ir_log(" ");
      printvalue(v->args.store.v, valindices);
      break;
    }
    case IR_BINOP: {
      printbinop(v->args.binop.op);
      ir_log(" ");
      printvalue(v->args.binop.l, valindices);
      ir_log(" ");
      printvalue(v->args.binop.r, valindices);
      break;
    }
    case IR_CMP: {
      ir_log("if ");
      printvalue(v->args.cmp.l, valindices);
      ir_log(" ");
      printcmpop(v->args.cmp.op);
      ir_log(" ");
      printvalue(v->args.cmp.r, valindices);
      ir_log(" then ");
      printbblock(v->args.cmp.truebr, bbindices);
      ir_log(" else ");
      printbblock(v->args.cmp.falsebr, bbindices);
      break;
    }
    case IR_JMP: {
      ir_log("jmp ");
      printbblock(v->args.jmp, bbindices);
      break;
    }
    case IR_RET: {
      ir_log("ret ");
      printvalue(v->args.ret.v, valindices);
      break;
    }
    case IR_PHI: {
      size_t i, n = ir_phivec_size(v->args.phi);
      ir_log("phi [<");
      for (i = 0; i < n; ++i) {
        IRPhiNode *phi = ir_phivec_get(v->args.phi, i);
        printbblock(phi->bblock, bbindices);
        ir_log(", ");
        printvalue(phi->value, valindices);
        if (i != n - 1)
          ir_log(">, <");
      }
      ir_log(">]");
      break;
    }
  }
  ir_log("\n");
}

static void fillindices(IRFunction *F, IRBBlockTable *bbindices,
                        IRValueTable *valindices) {
  int bbindex = 0, valindex = 0;
  ir_bbvec_foreach(F->bblocks, bb, {
    ir_bbtab_insert(bbindices, bb, bbindex++);
    ir_valvec_foreach(bb->values, v, {
      if (v->instr != IR_CONST && v->type != IR_VOID)
        ir_valtab_insert(valindices, v, valindex++);
    });
  });
}

void _ir_print(IRFunction *F) {
  size_t nblocks = _ir_nbblocks(F);
  size_t nvalues = _ir_nvalues(F);
  IRBBlockTable *bbindices = ir_bbtab_createwa(nblocks, F->L);
  IRValueTable *valindices = ir_valtab_createwa(nvalues, F->L);;
  fillindices(F, bbindices, valindices);
  ir_log("IR function (%p)\n", (void *)F);
  ir_bbvec_foreach(F->bblocks, bb, {
    printbblock(bb, bbindices);
    ir_log(":\n");
    ir_valvec_foreach(bb->values, v, {
      printinstr(v, bbindices, valindices);
    });
    ir_log("\n");
  });
  ir_log("\n");
  ir_bbtab_destroy(bbindices);
  ir_valtab_destroy(valindices);
}


