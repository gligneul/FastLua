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

#include <stdarg.h>
#include <stdio.h>

#include "lprefix.h"
#include "lmem.h"

#include "fl_logger.h"
#include "fl_ir.h"

/* Conteiners implementation */
TSCC_DECL_HASHTABLE(IRBBlockTable, ir_bbtab_, IRBBlock *, int,
    tscc_ptr_hashfunc, tscc_general_compare)
TSCC_DECL_HASHTABLE(IRValueTable, ir_valtab_, IRValue *, int,
    tscc_ptr_hashfunc, tscc_general_compare)

IRFunction *ir_create(struct lua_State *L) {
  IRFunction *F = luaM_new(L, IRFunction);
  F->L = L;
  F->currbb = NULL;
  ir_bbvec_create(&F->bblocks, L);
  return F;
}

void ir_destroy(IRFunction *F) {
  lua_State *L = F->L;
  ir_bbvec_foreach(&F->bblocks, bb, {
    ir_valvec_foreach(&bb->values, v, {
      if (v->instr == IR_PHI) {
        ir_phivec_foreach(&v->args.phi, phi, luaM_free(L, phi));
        ir_phivec_destroy(&v->args.phi);
      }
      luaM_free(L, v);
    });
    ir_valvec_destroy(&bb->values);
    luaM_free(L, bb);
  });
  ir_bbvec_destroy(&F->bblocks);
  luaM_free(L, F);
}

/* Creates a basic block */
static IRBBlock *createbblock(IRFunction *F) {
  IRBBlock *bb = luaM_new(F->L, IRBBlock);
  ir_valvec_create(&bb->values, F->L);
  return bb;
}

IRBBlock *_ir_addbblock(IRFunction *F) {
  IRBBlock *bb = createbblock(F);
  ir_bbvec_push(&F->bblocks, bb);
  return bb;
}

IRBBlock *_ir_insertbblock(IRFunction *F, IRBBlock *prevbb) {
  IRBBlock *bb = createbblock(F);
  size_t pos = 1;
  ir_bbvec_foreach(&F->bblocks, thisbb, {
    if (thisbb == prevbb)
      break;
    else
      pos++;
  });
  ir_bbvec_insert(&F->bblocks, pos, bb);
  return bb;
}

size_t _ir_nvalues(IRFunction *F) {
  size_t n = 0;
  ir_bbvec_foreach(&F->bblocks, bb, {
    n += ir_valvec_size(&bb->values);
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
  ir_valvec_push(&bb->values, v);
  return v;
}

IRValue *_ir_consti(IRFunction *F, IRInt i, enum IRType type) {
  IRValue *v = createvalue(F, type, IR_CONST);
  v->args.konst.i = i;
  return v;
}

IRValue *_ir_constf(IRFunction *F, IRFloat f) {
  IRValue *v = createvalue(F, IR_FLOAT, IR_CONST);
  v->args.konst.f = f;
  return v;
}

IRValue *_ir_constp(IRFunction *F, void *p) {
  IRValue *v = createvalue(F, IR_PTR, IR_CONST);
  v->args.konst.p = p;
  return v;
}

IRValue *_ir_getarg(IRFunction *F, enum IRType type, int n) {
  IRValue *v = createvalue(F, type, IR_GETARG);
  v->args.getarg.n = n;
  return v;
}

IRValue *_ir_load(IRFunction *F, enum IRType type, IRValue *mem,
                  int offset) {
  IRValue *v = createvalue(F, type, IR_LOAD);
  v->args.load.mem = mem;
  v->args.load.offset = offset;
  v->args.load.type = type;
  return v;
}

IRValue *_ir_store(IRFunction *F, IRValue *mem, IRValue *val, int offset) {
  IRValue *v = createvalue(F, IR_VOID, IR_STORE);
  v->args.store.mem = mem;
  v->args.store.v = val;
  v->args.store.offset = offset;
  fll_assert(mem->type == IR_PTR, "ir_store: mem ins't a pointer");
  return v;
}

IRValue *_ir_cast(IRFunction *F, IRValue *val, enum IRType type) {
  IRValue *v = createvalue(F, type, IR_CAST);
  v->args.cast.v = val;
  v->args.cast.type = type;
  return v;
}

IRValue *_ir_binop(IRFunction *F, enum IRBinOp op, IRValue *l, IRValue *r) {
  IRValue *v = createvalue(F, l->type, IR_BINOP);
  v->args.binop.op = op;
  v->args.binop.l = l;
  v->args.binop.r = r;
  fll_assert(l->type == r->type, "ir_binop: type mismatch");
  return v;
}

/* Perform a const comparison operation. */
#define performcmp(op, l, r) \
  do { \
    switch (op) { \
      case IR_NE: return (l) != (r); \
      case IR_EQ: return (l) == (r); \
      case IR_LE: return (l) <= (r); \
      case IR_LT: return (l) < (r); \
      case IR_GE: return (l) >= (r); \
      case IR_GT: return (l) > (r); \
    } \
  } while (0)

static int foldcmp(enum IRCmpOp op, IRValue *l, IRValue *r) {
  if (ir_isintt(l->type))
    performcmp(op, l->args.konst.i, r->args.konst.i);
  else if (l->type == IR_FLOAT)
    performcmp(op, l->args.konst.f, r->args.konst.f);
  else
    performcmp(op, l->args.konst.p, r->args.konst.p);
  return 0;
}

IRValue *_ir_cmp(IRFunction *F, enum IRCmpOp op, IRValue *l, IRValue *r,
                 IRBBlock *jmp) {
  fll_assert(l->type == r->type, "ir_cmp: type mismatch");
  if (l->instr == IR_CONST && r->instr == IR_CONST) {
    if (foldcmp(op, l, r))
      return _ir_jmp(F, jmp);
    else
      return NULL;
  }
  else {
    IRValue *v = createvalue(F, IR_VOID, IR_CMP);
    v->args.cmp.op = op;
    v->args.cmp.l = l;
    v->args.cmp.r = r;
    v->args.cmp.jmp = jmp;
    return v;
  }
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
  ir_phivec_create(&v->args.phi, F->L);
  return v;
}

void _ir_addphinode(IRFunction *F, IRValue *v, IRValue *value,
                    IRBBlock *bblock) {
  IRPhiNode *phi = luaM_new(F->L, IRPhiNode);
  phi->value = value;
  phi->bblock = bblock;
  ir_phivec_push(&v->args.phi, phi);
  fll_assert(v->instr == IR_PHI, "ir_addphinode: value ins't a phi");
  fll_assert(v->type == value->type, "ir_addphinode: type mismatch");
}

/*
 * Printing functions for debug
 */

static void printtype(enum IRType type) {
  switch (type) {
    case IR_VOID:   fllog("void"); break;
    case IR_CHAR:   fllog("char"); break;
    case IR_SHORT:  fllog("short"); break;
    case IR_INT:    fllog("int"); break;
    case IR_LUAINT: fllog("luaint"); break;
    case IR_LONG:   fllog("long"); break;
    case IR_PTR:    fllog("ptr"); break;
    case IR_FLOAT:  fllog("luafloat"); break;
  }
}

static void printconst(enum IRType type, union IRConstant k) {
  fllog("(const ");
  printtype(type);
  fllog(" ");
  switch (type) {
    case IR_CHAR: case IR_SHORT: case IR_INT: case IR_LUAINT: case IR_LONG:
      fllog("%td", k.i);
      break;
    case IR_PTR:    fllog("%p", k.p); break;
    case IR_FLOAT:  fllog("%f", k.f); break;
    default: fll_error("ir::printconst: invalid type"); break;
  }
  fllog(")");
}

static void printbinop(enum IRBinOp op) {
  switch (op) {
    case IR_ADD: fllog("add"); break;
    case IR_SUB: fllog("sub"); break;
    case IR_MUL: fllog("mul"); break;
    case IR_DIV: fllog("div"); break;
  }
}

static void printcmpop(enum IRCmpOp op) {
  switch (op) {
    case IR_NE: fllog("!="); break;
    case IR_EQ: fllog("=="); break;
    case IR_LE: fllog("<="); break;
    case IR_LT: fllog("<"); break;
    case IR_GE: fllog(">="); break;
    case IR_GT: fllog(">"); break;
  }
}

static void printvalue(IRValue *v, IRValueTable *indices) {
  if (v->instr == IR_CONST)
    printconst(v->type, v->args.konst);
  else
    fllog("%%%d", ir_valtab_get(indices, v, -1));
}

static void printbblock(IRBBlock *bb, IRBBlockTable *indices) {
  fllog("bb%d", ir_bbtab_get(indices, bb, -1));
}

static void printinstr(IRValue *v, IRBBlockTable *bbindices,
                     IRValueTable *valindices) {
  if (v->instr == IR_CONST)
    return;
  fllog("  ");
  if (v->type != IR_VOID) {
    printvalue(v, valindices);
    /* fllog(" : "); printtype(v->type); */
    fllog(" = ");
  }
  switch (v->instr) {
    case IR_CONST:
      /* do nothing */
      break;
    case IR_GETARG: {
      fllog("getarg %d", v->args.getarg.n);
      break;
    }
    case IR_LOAD: {
      int offset = v->args.load.offset;
      fllog("load ");
      printtype(v->args.load.type);
      fllog(" ");
      if (offset > 0)
        fllog("%d(", offset);
      printvalue(v->args.load.mem, valindices);
      if (offset > 0)
        fllog(")");
      break;
    }
    case IR_STORE: {
      int offset = v->args.store.offset;
      fllog("store ");
      if (offset > 0)
        fllog("%d(", offset);
      printvalue(v->args.store.mem, valindices);
      if (offset > 0)
        fllog(")");
      fllog(" <- ");
      printvalue(v->args.store.v, valindices);
      break;
    }
    case IR_CAST: {
      fllog("cast ");
      printtype(v->args.cast.type);
      fllog(" <- ");
      printtype(v->args.cast.v->type);
      fllog(" ");
      printvalue(v->args.cast.v, valindices);
      break;
    }
    case IR_BINOP: {
      printbinop(v->args.binop.op);
      fllog(" ");
      printvalue(v->args.binop.l, valindices);
      fllog(" ");
      printvalue(v->args.binop.r, valindices);
      break;
    }
    case IR_CMP: {
      fllog("if ");
      printvalue(v->args.cmp.l, valindices);
      fllog(" ");
      printcmpop(v->args.cmp.op);
      fllog(" ");
      printvalue(v->args.cmp.r, valindices);
      fllog(" then ");
      printbblock(v->args.cmp.jmp, bbindices);
      break;
    }
    case IR_JMP: {
      fllog("jmp ");
      printbblock(v->args.jmp, bbindices);
      break;
    }
    case IR_RET: {
      fllog("ret ");
      printvalue(v->args.ret.v, valindices);
      break;
    }
    case IR_PHI: {
      size_t i, n = ir_phivec_size(&v->args.phi);
      fllog("phi [<");
      for (i = 0; i < n; ++i) {
        IRPhiNode *phi = ir_phivec_get(&v->args.phi, i);
        printbblock(phi->bblock, bbindices);
        fllog(", ");
        printvalue(phi->value, valindices);
        if (i != n - 1)
          fllog(">, <");
      }
      fllog(">]");
      break;
    }
  }
  fllog("\n");
}

static void fillindices(IRFunction *F, IRBBlockTable *bbindices,
                        IRValueTable *valindices) {
  int bbindex = 0, valindex = 0;
  ir_bbvec_foreach(&F->bblocks, bb, {
    ir_bbtab_insert(bbindices, bb, bbindex++);
    ir_valvec_foreach(&bb->values, v, {
      if (v->instr != IR_CONST && v->type != IR_VOID)
        ir_valtab_insert(valindices, v, valindex++);
    });
  });
}

void _ir_print(IRFunction *F) {
  size_t nblocks = _ir_nbblocks(F);
  size_t nvalues = _ir_nvalues(F);
  IRBBlockTable bbindices;
  IRValueTable valindices;
  ir_bbtab_create(&bbindices, nblocks, F->L);
  ir_valtab_create(&valindices, nvalues, F->L);
  fillindices(F, &bbindices, &valindices);
  fllog("IR function (%p)\n", (void *)F);
  ir_bbvec_foreach(&F->bblocks, bb, {
    printbblock(bb, &bbindices);
    fllog(":\n");
    ir_valvec_foreach(&bb->values, v, {
      printinstr(v, &bbindices, &valindices);
    });
    fllog("\n");
  });
  fllog("\n");
  ir_bbtab_destroy(&bbindices);
  ir_valtab_destroy(&valindices);
}


