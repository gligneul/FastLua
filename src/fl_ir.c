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
#include <stdarg.h>
#include <stdio.h>

#include "lprefix.h"

#include "lmem.h"

#include "fl_ir.h"

/* Conteiners implementation */
TSCC_IMPL_VECTOR_WA(IRBBlockVector, ir_bbvec_, IRBBlock *, struct lua_State *,
    luaM_realloc_)
TSCC_IMPL_VECTOR_WA(IRCommandVector, ir_cmdvec_, IRCommand *,
    struct lua_State *, luaM_realloc_)
TSCC_IMPL_VECTOR_WA(IRPhiNodeVector, ir_phivec_, IRPhiNode *,
    struct lua_State *, luaM_realloc_)
TSCC_IMPL_HASHTABLE_WA(IRBBlockTable, ir_bbtab_, IRBBlock *, int,
    tscc_ptr_hashfunc, tscc_general_compare, struct lua_State *, luaM_realloc_)
TSCC_IMPL_HASHTABLE_WA(IRCommandTable, ir_cmdtab_, IRCommand *, int,
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
    ir_cmdvec_foreach(bb->cmds, c, {
      if (c->cmdtype == IR_PHI) {
        ir_phivec_foreach(c->args.phi, phi, luaM_free(L, phi));
        ir_phivec_destroy(c->args.phi);
      }
      luaM_free(L, c);
    });
    ir_cmdvec_destroy(bb->cmds);
    luaM_free(L, bb);
  });
  ir_bbvec_destroy(F->bblocks);
  luaM_free(L, F);
}

IRBBlock *_ir_addbblock(IRFunction *F) {
  IRBBlock *bb = luaM_new(F->L, IRBBlock);
  bb->cmds = ir_cmdvec_createwa(F->L);
  ir_bbvec_push(F->bblocks, bb);
  F->currbb = bb;
  return bb;
}

IRBBlock *_ir_getbblock(IRFunction *F, size_t pos) {
  return ir_bbvec_get(F->bblocks, pos);
}

/* Create a command in the current basic block */
static IRCommand *createcmd(IRFunction *F, enum IRType type,
                            enum IRCommandType cmdtype) {
  IRBBlock *bb = F->currbb;
  IRCommand *c = luaM_new(F->L, IRCommand);
  c->type = type;
  c->cmdtype = cmdtype;
  c->bblock = bb;
  ir_cmdvec_push(bb->cmds, c);
  return c;
}

IRValue _ir_consti(IRFunction *F, IRInt i) {
  IRCommand *c = createcmd(F, IR_INTPTR, IR_CONST);
  c->args.konst.i = i;
  return c;
}

IRValue _ir_constf(IRFunction *F, lua_Number f) {
  IRCommand *c = createcmd(F, IR_LUAFLT, IR_CONST);
  c->args.konst.f = f;
  return c;
}

IRValue _ir_getarg(IRFunction *F, enum IRType type, int n) {
  IRCommand *c = createcmd(F, type, IR_GETARG);
  c->args.getarg.n = n;
  return c;
}

IRValue _ir_load(IRFunction *F, enum IRType type, IRValue mem) {
  /* promote integers to intptr */
  enum IRType finaltype = ir_isintt(type) ? IR_INTPTR : type;
  IRCommand *c = createcmd(F, finaltype, IR_LOAD);
  c->args.load.mem = mem;
  c->args.load.type = type;
  return c;
}

IRValue _ir_store(IRFunction *F, enum IRType type, IRValue mem, IRValue v) {
  IRCommand *c = createcmd(F, IR_VOID, IR_STORE);
  c->args.store.mem = mem;
  c->args.store.v = v;
  c->args.store.type = type;
  assert(v->type == type || (v->type == IR_INTPTR && ir_isintt(type)));
  return c;
}

IRValue _ir_binop(IRFunction *F, enum IRCommandType op, IRValue l, IRValue r) {
  IRCommand *c = createcmd(F, l->type, op);
  c->args.binop.l = l;
  c->args.binop.r = r;
  assert(l->type == r->type);
  return c;
}

IRValue _ir_jne(IRFunction *F, IRValue l, IRValue r, IRBBlock *bb) {
  IRCommand *c = createcmd(F, IR_VOID, IR_JNE);
  c->args.jne.l = l;
  c->args.jne.r = r;
  c->args.jne.bb = bb;
  assert(l->type == r->type);
  return c;
}

IRValue _ir_jmp(IRFunction *F, IRBBlock *bb) {
  IRCommand *c = createcmd(F, IR_VOID, IR_JMP);
  c->args.jmp = bb;
  return c;
}

IRValue _ir_return(IRFunction *F, IRValue v) {
  IRCommand *c = createcmd(F, IR_VOID, IR_RET);
  c->args.ret.v = v;
  return c;
}

IRValue _ir_phi(IRFunction *F, enum IRType type) {
  IRCommand *c = createcmd(F, type, IR_PHI);
  c->args.phi = ir_phivec_createwa(F->L);
  return c;
}

void _ir_addphinode(IRFunction *F, IRCommand *c, IRValue value,
                    IRBBlock *bblock) {
  IRPhiNode *phi = luaM_new(F->L, IRPhiNode);
  phi->value = value;
  phi->bblock = bblock;
  ir_phivec_push(c->args.phi, phi);
  assert(c->cmdtype == IR_PHI);
  assert(c->type == value->type);
}

void _ir_move(IRFunction *F, IRBBlock *bb, size_t from, size_t to) {
  (void)F;
  IRCommand *c = ir_cmdvec_get(bb->cmds, from);
  ir_cmdvec_erase(bb->cmds, from);
  ir_cmdvec_insert(bb->cmds, to, c);
}

/* Replace helper */
#define replacehelper(cell, old, new) \
  do { if (cell == old) cell = new; } while (0)

static void replacerec(IRFunction *F, IRBBlock *bb, IRValue old, IRValue new,
                       IRBBlockTable *visited)
{
  if (ir_bbtab_contains(visited, bb))
    return;
  else
    ir_bbtab_insert(visited, bb, 1);
  ir_cmdvec_foreach(bb->cmds, c, {
    if (c == old || c == new)
      continue;
    switch (c->cmdtype) {
      case IR_CONST: case IR_GETARG: case IR_JMP:
        /* do nothing */
        break;
      case IR_LOAD:
        replacehelper(c->args.load.mem, old, new);
        break;
      case IR_STORE:
        replacehelper(c->args.store.mem, old, new);
        replacehelper(c->args.store.v, old, new);
        break;
      case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV:
        replacehelper(c->args.binop.l, old, new);
        replacehelper(c->args.binop.r, old, new);
        break;
      case IR_JNE:
        replacehelper(c->args.jne.l, old, new);
        replacehelper(c->args.jne.r, old, new);
        break;
      case IR_RET:
        replacehelper(c->args.ret.v, old, new);
        break;
      case IR_PHI:
        ir_phivec_foreach(c->args.phi, phi, {
          replacehelper(phi->value, old, new);
          replacerec(F, phi->bblock, old, new, visited);
        });
        break;
    }
  });
}

void _ir_replacevalue(IRFunction *F, IRBBlock *bb, IRValue old, IRValue new) {
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
    case IR_INTPTR: ir_log("iptr"); break;
    case IR_LUAFLT: ir_log("luafloat"); break;
  }
}

static void printconst(enum IRType type, union IRConstant k) {
  ir_log("(const ");
  printtype(type);
  ir_log(" ");
  switch (type) {
    case IR_INTPTR: ir_log("%td", k.i); break;
    case IR_LUAFLT: ir_log("%f", k.f); break;
    default: assert(0); break;
  }
  ir_log(")");
}

static void printbinop(enum IRCommandType c) {
  switch (c) {
    case IR_ADD: ir_log("add"); break;
    case IR_SUB: ir_log("sub"); break;
    case IR_MUL: ir_log("mul"); break;
    case IR_DIV: ir_log("div"); break;
    default: assert(0); break;
  }
}

static void printvalue(IRValue v, IRCommandTable *indices) {
  if (v->cmdtype == IR_CONST)
    printconst(v->type, v->args.konst);
  else
    ir_log("%%%d", ir_cmdtab_get(indices, v, -1));
}

static void printbblock(IRBBlock *bb, IRBBlockTable *indices) {
  ir_log("bb_%d", ir_bbtab_get(indices, bb, -1));
}

static void printcmd(IRCommand *c, IRBBlockTable *bbindices,
                     IRCommandTable *cmdindices) {
  if (c->cmdtype == IR_CONST)
    return;
  ir_log("  ");
  if (c->type != IR_VOID) {
    printvalue(c, cmdindices);
    /* ir_log(" : "); printtype(c->type); */
    ir_log(" = ");
  }
  switch (c->cmdtype) {
    case IR_CONST:
      /* do nothing */
      break;
    case IR_GETARG:
      ir_log("getarg %d", c->args.getarg.n);
      break;
    case IR_LOAD:
      ir_log("load ");
      printtype(c->args.load.type);
      ir_log(" ");
      printvalue(c->args.load.mem, cmdindices);
      break;
    case IR_STORE:
      ir_log("store ");
      printvalue(c->args.store.mem, cmdindices);
      ir_log(" <- ");
      printtype(c->args.store.type);
      ir_log(" ");
      printvalue(c->args.store.v, cmdindices);
      break;
    case IR_ADD:
    case IR_SUB:
    case IR_MUL:
    case IR_DIV:
      printbinop(c->cmdtype);
      ir_log(" ");
      printvalue(c->args.binop.l, cmdindices);
      ir_log(" ");
      printvalue(c->args.binop.r, cmdindices);
      break;
    case IR_JNE:
      ir_log("jne ");
      printvalue(c->args.jne.l, cmdindices);
      ir_log(" ");
      printvalue(c->args.jne.r, cmdindices);
      ir_log(" ");
      printbblock(c->args.jne.bb, bbindices);
      break;
    case IR_JMP:
      ir_log("jmp ");
      printbblock(c->args.jmp, bbindices);
      break;
    case IR_RET:
      ir_log("ret ");
      printvalue(c->args.ret.v, cmdindices);
      break;
    case IR_PHI: {
      size_t i, n = ir_phivec_size(c->args.phi);
      ir_log("phi [<");
      for (i = 0; i < n; ++i) {
        IRPhiNode *phi = ir_phivec_get(c->args.phi, i);
        printbblock(phi->bblock, bbindices);
        ir_log(", ");
        printvalue(phi->value, cmdindices);
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
                        IRCommandTable *cmdindices) {
  int bbindex = 0, cmdindex = 0;
  ir_bbvec_foreach(F->bblocks, bb, {
    ir_bbtab_insert(bbindices, bb, bbindex++);
    ir_cmdvec_foreach(bb->cmds, c, {
      if (c->cmdtype != IR_CONST && c->type != IR_VOID)
        ir_cmdtab_insert(cmdindices, c, cmdindex++);
    });
  });
}

static size_t getnumberofcmds(IRFunction *F) {
  size_t ncmds = 0;
  ir_bbvec_foreach(F->bblocks, bb, ncmds++);
  return ncmds;
}

void _ir_print(IRFunction *F) {
  size_t nblocks = ir_bbvec_size(F->bblocks);
  size_t ncmds = getnumberofcmds(F);
  IRBBlockTable *bbindices = ir_bbtab_createwa(nblocks, F->L);
  IRCommandTable *cmdindices = ir_cmdtab_createwa(ncmds, F->L);;
  fillindices(F, bbindices, cmdindices);
  ir_log("IR function (%p)\n", (void *)F);
  ir_bbvec_foreach(F->bblocks, bb, {
    printbblock(bb, bbindices);
    ir_log(":\n");
    ir_cmdvec_foreach(bb->cmds, c, {
      printcmd(c, bbindices, cmdindices);
    });
    ir_log("\n");
  });
  ir_log("\n");
  ir_bbtab_destroy(bbindices);
  ir_cmdtab_destroy(cmdindices);
}


