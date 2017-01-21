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
  size_t i, j, k;
  for (i = 0; i < ir_bbvec_size(F->bblocks); ++i) {
    IRBBlock *bb = ir_bbvec_get(F->bblocks, i);
    for (j = 0; j < ir_cmdvec_size(bb->cmds); ++j) {
      IRCommand *cmd = ir_cmdvec_get(bb->cmds, j);
      if (cmd->cmdtype == IR_PHI) {
        for (k = 0; k < ir_phivec_size(cmd->args.phi); ++k)
          luaM_free(L, ir_phivec_get(cmd->args.phi, k));
        ir_phivec_destroy(cmd->args.phi);
      }
      luaM_free(L, cmd);
    }
    ir_cmdvec_destroy(bb->cmds);
    luaM_free(L, bb);
  }
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
  IRCommand *c = createcmd(F, type, IR_STORE);
  c->args.store.mem = mem;
  c->args.store.v = v;
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

void _ir_addphinode(IRFunction *F, IRCommand *cmd, IRValue value,
                    IRBBlock *bblock) {
  IRPhiNode *phi = luaM_new(F->L, IRPhiNode);
  phi->value = value;
  phi->bblock = bblock;
  ir_phivec_push(cmd->args.phi, phi);
  assert(cmd->cmdtype == IR_PHI);
  assert(cmd->type == value->type);
}

void _ir_move(IRFunction *F, IRBBlock *bb, size_t from, size_t to) {
  (void)F;
  IRCommand *cmd = ir_cmdvec_get(bb->cmds, from);
  ir_cmdvec_erase(bb->cmds, from);
  ir_cmdvec_insert(bb->cmds, to, cmd);
}

/* Replace helper */
#define replacehelper(cell, old, new) \
  do { if (cell == old) cell = new; } while (0)

static void replacerec(IRFunction *F, IRBBlock *bb, IRValue old, IRValue new,
                       IRBBlockTable *visited)
{
  size_t i;
  if (ir_bbtab_contains(visited, bb))
    return;
  else
    ir_bbtab_insert(visited, bb, 1);
  for (i = 0; i < ir_cmdvec_size(bb->cmds); ++i) {
    IRCommand *cmd = ir_cmdvec_get(bb->cmds, i);
    if (cmd == old || cmd == new)
      continue;
    switch (cmd->cmdtype) {
      case IR_CONST: case IR_GETARG:
        /* do nothing */
        break;
      case IR_LOAD:
        replacehelper(cmd->args.load.mem, old, new);
        break;
      case IR_STORE:
        replacehelper(cmd->args.store.mem, old, new);
        replacehelper(cmd->args.store.v, old, new);
        break;
      case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV:
        replacehelper(cmd->args.binop.l, old, new);
        replacehelper(cmd->args.binop.r, old, new);
        break;
      case IR_RET:
        replacehelper(cmd->args.ret.v, old, new);
        break;
      case IR_PHI: {
        size_t j;
        for (j = 0; j < ir_phivec_size(cmd->args.phi); ++j) {
          IRPhiNode *phi = ir_phivec_get(cmd->args.phi, j);
          replacehelper(phi->value, old, new);
          replacerec(F, phi->bblock, old, new, visited);
        }
        break;
      }
    }
  }
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
    case IR_INTPTR: ir_log("intptr"); break;
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

static void printbinop(enum IRCommandType cmd) {
  switch (cmd) {
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

static void printcmd(IRCommand *cmd, IRBBlockTable *bbindices,
                     IRCommandTable *cmdindices) {
  if (cmd->cmdtype == IR_CONST)
    return;
  ir_log("  ");
  if (cmd->type != IR_VOID) {
    printvalue(cmd, cmdindices);
    /* ir_log(" : "); printtype(cmd->type); */
    ir_log(" = ");
  }
  switch (cmd->cmdtype) {
    case IR_CONST:
      /* do nothing */
      break;
    case IR_GETARG:
      ir_log("getarg %d", cmd->args.getarg.n);
      break;
    case IR_LOAD:
      ir_log("load ");
      printtype(cmd->args.load.type);
      ir_log(" ");
      printvalue(cmd->args.load.mem, cmdindices);
      break;
    case IR_STORE:
      ir_log("store ");
      printvalue(cmd->args.store.mem, cmdindices);
      ir_log(" <- ");
      printvalue(cmd->args.store.v, cmdindices);
      break;
    case IR_ADD:
    case IR_SUB:
    case IR_MUL:
    case IR_DIV:
      printbinop(cmd->cmdtype);
      ir_log(" ");
      printvalue(cmd->args.binop.l, cmdindices);
      ir_log(" ");
      printvalue(cmd->args.binop.r, cmdindices);
      break;
    case IR_RET:
      ir_log("ret ");
      printvalue(cmd->args.ret.v, cmdindices);
      break;
    case IR_PHI: {
      size_t i, n = ir_phivec_size(cmd->args.phi);
      ir_log("phi [<");
      for (i = 0; i < n; ++i) {
        IRPhiNode *phi = ir_phivec_get(cmd->args.phi, i);
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
  size_t i, j;
  int cmdindex = 0;
  for (i = 0; i < ir_bbvec_size(F->bblocks); ++i) {
    IRBBlock *bb = ir_bbvec_get(F->bblocks, i);
    ir_bbtab_insert(bbindices, bb, (int)i);
    for (j = 0; j < ir_cmdvec_size(bb->cmds); ++j) {
      IRCommand *cmd = ir_cmdvec_get(bb->cmds, j);
      if (cmd->cmdtype == IR_CONST || cmd->type == IR_VOID)
        continue;
      ir_cmdtab_insert(cmdindices, cmd, cmdindex++);
    }
  }
}

static size_t getnumberofcmds(IRFunction *F) {
  size_t i, ncmds = 0;
  for (i = 0; i < ir_bbvec_size(F->bblocks); ++i) {
    IRBBlock *bb = ir_bbvec_get(F->bblocks, i);
    ncmds += ir_cmdvec_size(bb->cmds);
  }
  return ncmds;
}

void _ir_print(IRFunction *F) {
  size_t i, j;
  size_t nblocks = ir_bbvec_size(F->bblocks);
  size_t ncmds = getnumberofcmds(F);
  IRBBlockTable *bbindices = ir_bbtab_createwa(nblocks, F->L);
  IRCommandTable *cmdindices = ir_cmdtab_createwa(ncmds, F->L);;
  fillindices(F, bbindices, cmdindices);
  ir_log("IR function (%p)\n", (void *)F);
  for (i = 0; i < ir_bbvec_size(F->bblocks); ++i) {
    IRBBlock *bb = ir_bbvec_get(F->bblocks, i);
    printbblock(bb, bbindices);
    ir_log(":\n");
    for (j = 0; j < ir_cmdvec_size(bb->cmds); ++j) {
      IRCommand *cmd = ir_cmdvec_get(bb->cmds, j);
      printcmd(cmd, bbindices, cmdindices);
    }
    ir_log("\n");
  }
  ir_log("\n");
  ir_bbtab_destroy(bbindices);
  ir_cmdtab_destroy(cmdindices);
}


