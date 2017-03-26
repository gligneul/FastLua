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

#include "lprefix.h"
#include "lmem.h"

#include "fl_logger.h"
#include "fl_ir.h"

void _ir_init(IRFunction *F, struct lua_State *L) {
  F->L = L;
  F->currbb = -1;
  F->ninstrs = 0;
  irbbv_create(&F->bblocks, L);
}

void _ir_close(IRFunction *F) {
  irbbv_foreach(&F->bblocks, bb, {
    irbb_foreach(bb, instr, {
      if (instr->tag == IR_PHI)
        irpv_destroy(&instr->args.phi.inc);
    });
    irbb_destroy(bb);
  });
  irbbv_destroy(&F->bblocks);
}

IRName _ir_addbblock(IRFunction *F) {
  IRBBlock bb;
  irbb_create(&bb, F->L);
  irbbv_push(&F->bblocks, bb);
  return (IRName)irbbv_size(&F->bblocks) - 1;
}

IRInstr *_ir_instr(IRFunction *F, IRValue v) {
  IRBBlock *bb = irbbv_getref(&F->bblocks, v.bblock);
  return irbb_getref(bb, v.instr);
}

IRValue ir_createvalue(IRName bblock, IRName instr) {
  IRValue v;
  v.bblock = bblock;
  v.instr = instr;
  return v;
}

/* Create a value pointing to the last instruction added. */
static IRValue lastvalue(IRFunction *F) {
  IRBBlock *bb = irbbv_getref(&F->bblocks, F->currbb);
  return ir_createvalue((IRName)F->currbb, (IRName)irbb_size(bb) - 1);
}

/* Create a instruction in the current basic block. */
static IRInstr *createinstr(IRFunction *F, enum IRType type,
                            enum IRInstrTag tag) {
  IRBBlock *bb = irbbv_getref(&F->bblocks, F->currbb);
  IRInstr i;
  i.type = type;
  i.tag = tag;
  i.bblock = F->currbb;
  i.id = F->ninstrs++;
  irbb_push(bb, i);
  return irbb_getref(bb, irbb_size(bb) - 1);
}

IRValue _ir_consti(IRFunction *F, IRInt k, enum IRType type) {
  IRInstr *i = createinstr(F, type, IR_CONST);
  i->args.konst.i = k;
  return lastvalue(F);
}

IRValue _ir_constf(IRFunction *F, IRFloat f) {
  IRInstr *i = createinstr(F, IR_FLOAT, IR_CONST);
  i->args.konst.f = f;
  return lastvalue(F);
}

IRValue _ir_constp(IRFunction *F, void *p) {
  IRInstr *i = createinstr(F, IR_PTR, IR_CONST);
  i->args.konst.p = p;
  return lastvalue(F);
}

IRValue _ir_getarg(IRFunction *F, enum IRType type, int n) {
  IRInstr *i = createinstr(F, type, IR_GETARG);
  i->args.getarg.n = n;
  return lastvalue(F);
}

IRValue _ir_load(IRFunction *F, enum IRType type, IRValue addr, int offset) {
  IRInstr *i = createinstr(F, type, IR_LOAD);
  i->args.load.addr = addr;
  i->args.load.offset = offset;
  i->args.load.type = type;
  return lastvalue(F);
}

IRValue _ir_store(IRFunction *F, IRValue addr, IRValue val, int offset) {
  IRInstr *i = createinstr(F, IR_VOID, IR_STORE);
  i->args.store.addr = addr;
  i->args.store.val = val;
  i->args.store.offset = offset;
  fll_assert(_ir_instr(F, addr)->type == IR_PTR, "addr not a pointer");
  return lastvalue(F);
}

IRValue _ir_cast(IRFunction *F, IRValue val, enum IRType type) {
  IRInstr *i = createinstr(F, type, IR_CAST);
  i->args.cast.val = val;
  i->args.cast.type = type;
  return lastvalue(F);
}

IRValue _ir_binop(IRFunction *F, enum IRBinOp op, IRValue lhs, IRValue rhs) {
  IRInstr *i = createinstr(F, _ir_instr(F, lhs)->type, IR_BINOP);
  i->args.binop.op = op;
  i->args.binop.lhs = lhs;
  i->args.binop.rhs = rhs;
  fll_assert(_ir_instr(F, lhs)->type == _ir_instr(F, rhs)->type,
             "binop type mismatch");
  return lastvalue(F);
}

/* Perform a const comparison operation. */
#define computecmp_(op, l, r) \
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

static int computecmp(enum IRCmpOp op, IRInstr *l, IRInstr *r) {
  if (ir_isintt(l->type))
    computecmp_(op, l->args.konst.i, r->args.konst.i);
  else if (l->type == IR_FLOAT)
    computecmp_(op, l->args.konst.f, r->args.konst.f);
  else
    computecmp_(op, l->args.konst.p, r->args.konst.p);
  return 0;
}

IRValue _ir_cmp(IRFunction *F, enum IRCmpOp op, IRValue lhs, IRValue rhs,
                IRName dest) {
  IRInstr *li = _ir_instr(F, lhs);
  IRInstr *ri = _ir_instr(F, rhs);
  fll_assert(li->type == ri->type, "cmp type mismatch");
  if (li->tag == IR_CONST && ri->tag == IR_CONST) {
    if (computecmp(op, li, ri))
      return _ir_jmp(F, dest);
    else
      return ir_nullvalue();
  }
  else {
    IRInstr *i = createinstr(F, IR_VOID, IR_CMP);
    i->args.cmp.op = op;
    i->args.cmp.lhs = lhs;
    i->args.cmp.rhs = rhs;
    i->args.cmp.dest = dest;
    return lastvalue(F);
  }
}

IRValue _ir_jmp(IRFunction *F, IRName dest) {
  IRInstr *i = createinstr(F, IR_VOID, IR_JMP);
  i->args.jmp.dest = dest;
  return lastvalue(F);
}

IRValue _ir_return(IRFunction *F, IRValue val) {
  IRInstr *i = createinstr(F, IR_VOID, IR_RET);
  i->args.ret.val = val;
  return lastvalue(F);
}

IRValue _ir_phi(IRFunction *F, enum IRType type) {
  IRInstr *i = createinstr(F, type, IR_PHI);
  irpv_create(&i->args.phi.inc, F->L);
  return lastvalue(F);
}

void _ir_addphiinc(IRFunction *F, IRValue phi, IRValue value, IRName bblock) {
  IRInstr *pi = _ir_instr(F, phi);
  IRInstr *vi = _ir_instr(F, value);
  IRPhiInc inc;
  inc.value = value;
  inc.bblock = bblock;
  irpv_push(&pi->args.phi.inc, inc);
  fll_assert(pi->tag == IR_PHI, "not a phi instruction");
  fll_assert(pi->type == vi->type, "phi type mismatch");
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

static void printconst(IRInstr *i) {
  fllog("(const ");
  printtype(i->type);
  fllog(" ");
  switch (i->type) {
    case IR_CHAR: case IR_SHORT: case IR_INT: case IR_LUAINT: case IR_LONG:
      fllog("%td", i->args.konst.i);
      break;
    case IR_PTR:    fllog("%p", i->args.konst.p); break;
    case IR_FLOAT:  fllog("%f", i->args.konst.f); break;
    default: fll_error("invalid constant type"); break;
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

static void printinstrvalue(IRInstr *i) {
  if (i->tag == IR_CONST)
    printconst(i);
  else
    fllog("%%%02d", i->id);
}

static void printvalue(IRFunction *F, IRValue v) {
  printinstrvalue(_ir_instr(F, v));
}

static void printbblock(IRName bblock) {
  fllog("bb%d", bblock);
}

static void printinstr(IRFunction *F, IRInstr *i) {
  if (i->tag == IR_CONST) return;
  fllog("  ");
  printinstrvalue(i);
  fllog(" = ");
  switch (i->tag) {
    case IR_CONST:
      /* do nothing */
      break;
    case IR_GETARG: {
      fllog("getarg %d", i->args.getarg.n);
      break;
    }
    case IR_LOAD: {
      int offset = i->args.load.offset;
      fllog("load ");
      printtype(i->args.load.type);
      fllog(" ");
      if (offset > 0)
        fllog("%d(", offset);
      printvalue(F, i->args.load.addr);
      if (offset > 0)
        fllog(")");
      break;
    }
    case IR_STORE: {
      int offset = i->args.store.offset;
      fllog("store ");
      if (offset > 0)
        fllog("%d(", offset);
      printvalue(F, i->args.store.addr);
      if (offset > 0)
        fllog(")");
      fllog(" <- ");
      printvalue(F, i->args.store.val);
      break;
    }
    case IR_CAST: {
      IRValue val = i->args.cast.val;
      fllog("cast ");
      printtype(i->args.cast.type);
      fllog(" <- ");
      printtype(_ir_instr(F, val)->type);
      fllog(" ");
      printvalue(F, val);
      break;
    }
    case IR_BINOP: {
      printbinop(i->args.binop.op);
      fllog(" ");
      printvalue(F, i->args.binop.lhs);
      fllog(" ");
      printvalue(F, i->args.binop.rhs);
      break;
    }
    case IR_CMP: {
      fllog("if ");
      printvalue(F, i->args.cmp.lhs);
      fllog(" ");
      printcmpop(i->args.cmp.op);
      fllog(" ");
      printvalue(F, i->args.cmp.rhs);
      fllog(" then ");
      printbblock(i->args.cmp.dest);
      break;
    }
    case IR_JMP: {
      fllog("jmp ");
      printbblock(i->args.jmp.dest);
      break;
    }
    case IR_RET: {
      fllog("ret ");
      printvalue(F, i->args.ret.val);
      break;
    }
    case IR_PHI: {
      IRPhiIncVector *pv = &i->args.phi.inc;
      size_t j, n = irpv_size(pv);
      fllog("phi [<");
      for (j = 0; j < n; ++j) {
        IRPhiInc *inc = irpv_getref(pv, j);
        printbblock(inc->bblock);
        fllog(", ");
        printvalue(F, inc->value);
        if (j != n - 1) fllog(">, <");
      }
      fllog(">]");
      break;
    }
  }
  fllog(" : ");
  printtype(i->type);
  fllog("\n");
}

void _ir_print(IRFunction *F) {
  IRName id = 0;
  fllog("IR %p:\n", (void *)F);
  irbbv_foreach(&F->bblocks, bb, {
    printbblock(id++);
    fllog(":\n");
    irbb_foreach(bb, i, {
      printinstr(F, i);
    });
    fllog("\n");
  });
  fllog("\n");
}

