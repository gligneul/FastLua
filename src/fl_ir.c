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
#include <stdio.h>

#include "lprefix.h"

#include "lmem.h"

#include "fl_ir.h"

IRFunction *flir_create(struct lua_State *L) {
  IRFunction *F = luaM_new(L, IRFunction);
  F->L = L;
  F->currbb = NULL;
  fllist_init(IRBBlock, F->bblocks);
  return F;
}

void _flir_destroy(IRFunction *F) {
  IRBBlock *b = F->bblocks.first;
  while (b != NULL) {
    IRCommand *c = b->cmds.first;
    while (c != NULL) {
      if (c->cmdtype == IR_PHI) {
        IRPhiNode *p = c->args.phi.first;
        while (p != NULL)
          fllist_destroy(IRPhiNode, F->L, p);
      }
      fllist_destroy(IRCommand, F->L, c);
    }
    fllist_destroy(IRBBlock, F->L, b);
  }
  luaM_free(F->L, F);
}

IRBBlock *_flir_addbblock(IRFunction *F) {
  IRBBlock *b;
  fllist_insert(IRBBlock, F->L, F->bblocks, b);
  fllist_init(IRCommand, b->cmds);
  F->currbb = b;
  return b;
}

/* Create a command in the current basic block */
IRCommand *createcmd(IRFunction *F, lu_byte type, lu_byte cmdtype) {
  IRBBlock *b = F->currbb;
  IRCommand *c;
  fllist_insert(IRValue, F->L, b->cmds, c);
  c->type = type;
  c->cmdtype = cmdtype;
  c->bblock = b;
  return c;
}

IRValue _flir_consti(IRFunction *F, l_mem i) {
  IRCommand *c = createcmd(F, IR_INTPTR, IR_CONST);
  c->args.konst.i = i;
  return c;
}

IRValue _flir_constf(IRFunction *F, lua_Number f) {
  IRCommand *c = createcmd(F, IR_LUAFLT, IR_CONST);
  c->args.konst.f = f;
  return c;
}

IRValue _flir_getarg(IRFunction *F, lu_byte type, int n) {
  IRCommand *c = createcmd(F, type, IR_GETARG);
  c->args.getarg.n = n;
  return c;
}

IRValue _flir_load(IRFunction *F, lu_byte type, IRValue mem) {
  /* promote integers to intptr */
  lu_byte finaltype = flI_isintt(type) ? IR_INTPTR : type;
  IRCommand *c = createcmd(F, finaltype, IR_LOAD);
  c->args.load.mem = mem;
  c->args.load.type = type;
  return c;
}

IRValue _flir_store(IRFunction *F, lu_byte type, IRValue mem, IRValue v) {
  IRCommand *c = createcmd(F, type, IR_STORE);
  c->args.store.mem = mem;
  c->args.store.v = v;
  assert(v->type == type || (v->type == IR_INTPTR && flir_isintt(type)));
  return c;
}

IRValue _flir_binop(IRFunction *F, lu_byte op, IRValue l, IRValue r) {
  IRCommand *c = createcmd(F, l->type, op);
  c->args.binop.l = l;
  c->args.binop.r = r;
  assert(l->type == r->type);
  return c;
}

IRValue _flir_return(IRFunction *F, IRValue v) {
  IRCommand *c = createcmd(F, IR_VOID, IR_RET);
  c->args.ret.v = v;
  return c;
}

IRValue _flir_phi(IRFunction *F, IRType type) {
  IRCommand *c = createcmd(F, type, IR_PHI);
  fllist_init(IRPhiNode, c->args.phi);
  return c;
}

void _flir_addphinode(IRFunction *F, IRCommand *phi, IRValue value,
    IRBBlock *bblock) {
  IRPhiNode *p;
  assert(phi->cmdtype == IR_PHI);
  assert(phi->type == value->type);
  fllist_insert(IRPhiNode, F->L, phi->args.phi, p);
  p->value = value;
  p->bblock = bblock;
}

/* Replace helper */
#define replacehelper(cell, old, new) \
  do { if (cell == old) cell = new; } while (0)

void _flir_

void _flir_replacevalue(IRFunction *F, IRBBlock *b, IRValue old, IRValue new) {
  
}


void flI_replacevalue(IRFunction *F, IRId bbid, IRValue old, IRValue new) {
  IRBBlock *bb = flI_getbb(F, bbid);
  IRId cmdid;
  for (cmdid = 0; cmdid < bb->ncmds; ++cmdid) {
    IRValue v = {bbid, cmdid};
    IRCommand *cmd = flI_getcmd(F, v);
    if (!flI_valueeq(v, old) && !flI_valueeq(v, new)) {
      switch (cmd->cmdtype) {
        case IR_CONST: case IR_GETARG: case IR_STUB:
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
        case IR_PHI:
          replacehelper(cmd->args.phi.entry, old, new);
          replacehelper(cmd->args.phi.loop, old, new);
          break;
        default:
          assert(0);
      }
    }
  }
}

void flI_swapvalues(IRFunction *F, IRValue a, IRValue b) {
  IRCommand *acmd = flI_getcmd(F, a);
  IRCommand *bcmd = flI_getcmd(F, b);
  IRCommand tmp = *acmd;
  *acmd = *bcmd;
  *bcmd = tmp;
}

/*
 * Printing functions for debug
 */

static void printtype(lu_byte type) {
  switch (type) {
    case IR_CHAR:   flI_log("char"); break;
    case IR_SHORT:  flI_log("short"); break;
    case IR_INT:    flI_log("int"); break;
    case IR_LUAINT: flI_log("luaint"); break;
    case IR_INTPTR: flI_log("intptr"); break;
    case IR_LUAFLT: flI_log("luafloat"); break;
    default: assert(0); break;
  }
}

static void printconst(lu_byte type, IRUConstant k) {
  switch (type) {
    case IR_INTPTR: flI_log("%td", k.i); break;
    case IR_LUAFLT: flI_log("%f", k.f); break;
    default: assert(0); break;
  }
}

static void printbinop(lu_byte cmd) {
  switch (cmd) {
    case IR_ADD: flI_log("add"); break;
    case IR_SUB: flI_log("sub"); break;
    case IR_MUL: flI_log("mul"); break;
    case IR_DIV: flI_log("div"); break;
    default: assert(0); break;
  }
}

static void printvalue(IRValue v, int bbstart[]) {
  flI_log("%%%d", bbstart[v.bb] + v.cmd);
}

static void printcmd(IRFunction *F, IRId bbid, IRId cmdid, int bbstart[]) {
  IRValue v = {bbid, cmdid};
  IRCommand *cmd = flI_getcmd(F, v);
  if (cmd->cmdtype == IR_STUB)
    return;
  flI_log("  ");
  if (cmd->type != IR_VOID) {
    printvalue(v, bbstart);
    flI_log(" : ");
    printtype(cmd->type);
    flI_log(" = ");
  }
  switch (cmd->cmdtype) {
    case IR_CONST:
      flI_log("const ");
      printconst(cmd->type, cmd->args.konst);
      break;
    case IR_GETARG:
      flI_log("getarg %d", cmd->args.getarg.n);
      break;
    case IR_LOAD:
      flI_log("load ");
      printtype(cmd->args.load.type);
      flI_log(" ");
      printvalue(cmd->args.load.mem, bbstart);
      break;
    case IR_STORE:
      flI_log("store ");
      printvalue(cmd->args.store.mem, bbstart);
      flI_log(" <- ");
      printvalue(cmd->args.store.v, bbstart);
      break;
    case IR_ADD:
    case IR_SUB:
    case IR_MUL:
    case IR_DIV:
      printbinop(cmd->cmdtype);
      flI_log(" ");
      printvalue(cmd->args.binop.l, bbstart);
      flI_log(" ");
      printvalue(cmd->args.binop.r, bbstart);
      break;
    case IR_RET:
      flI_log("ret ");
      printvalue(cmd->args.ret.v, bbstart);
      break;
    case IR_PHI:
      flI_log("phi ");
      printvalue(cmd->args.phi.entry, bbstart);
      flI_log(" ");
      printvalue(cmd->args.phi.loop, bbstart);
      break;
    case IR_STUB:
      break;
    default:
      assert(0);
  }
  flI_log("\n");
}

void flI_print(IRFunction *F) {
  int ncmd = 0;
  int bbstart[F->nbbs];
  IRId bbid, cmdid;
  for (bbid = 0; bbid < F->nbbs; ++bbid) {
    bbstart[bbid] = ncmd;
    ncmd += F->bbs[bbid].ncmds;
  }
  flI_log("IR DEBUG - function (%p)\n", (void *)F);
  for (bbid = 0; bbid < F->nbbs; ++bbid) {
    IRBBlock *bb = flI_getbb(F, bbid);
    flI_log("bblock %d:\n", bbid);
    for (cmdid = 0; cmdid < bb->ncmds; ++cmdid)
      printcmd(F, bbid, cmdid, bbstart);
    flI_log("\n");
  }
  flI_log("\n");
}


