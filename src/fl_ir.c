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

/*
 * Creates a command and returns the value by reference.
 */
static IRCommand *createvalue(IRFunction *F, lu_byte type, lu_byte cmdtype,
    IRValue *v) {
  IRBBlock *bb = flI_getbb(F, F->currbb);
  IRId id = bb->ncmds++;
  IRCommand *cmd = NULL;
  luaM_growvector(F->L, bb->cmds, id, bb->sizecmds, IRCommand, MAX_INT, "");
  v->bb = F->currbb;
  v->cmd = id;
  cmd = flI_getcmd(F, *v);
  cmd->type = type;
  cmd->cmdtype = cmdtype;
  return cmd;
}

IRFunction *flI_createfunc(struct lua_State *L) {
  IRFunction *F = luaM_new(L, IRFunction);
  F->L = L;
  F->bbs = NULL;
  F->nbbs = 0;
  F->sizebbs = 0;
  F->currbb = 0;
  return F;
}

void flI_destroyfunc(IRFunction *F) {
  IRBBlock *bb;
  for (bb = F->bbs; bb != F->bbs + F->nbbs; ++bb)
    luaM_freearray(F->L, bb->cmds, bb->sizecmds);
  luaM_freearray(F->L, F->bbs, F->sizebbs);
  luaM_free(F->L, F);
}

IRId flI_createbb(IRFunction *F) {
  IRBBlock *bb = NULL;
  IRId id = F->nbbs++;
  luaM_growvector(F->L, F->bbs, id, F->sizebbs, IRBBlock, MAX_INT, "");
  bb = &F->bbs[id];
  bb->cmds = NULL;
  bb->ncmds = 0;
  bb->sizecmds = 0;
  return id;
}

IRValue flI_consti(IRFunction *F, IRInt k) {
  IRValue v;
  IRCommand *cmd = createvalue(F, IR_INT, IR_CONST, &v);
  cmd->args.konst.i = k;
  return v;
}

IRValue flI_constf(IRFunction *F, lua_Number k) {
  IRValue v;
  IRCommand *cmd = createvalue(F, IR_LUAFLT, IR_CONST, &v);
  cmd->args.konst.f = k;
  return v;
}

IRValue flI_constp(IRFunction *F, void *k) {
  IRValue v;
  IRCommand *cmd = createvalue(F, IR_PTR, IR_CONST, &v);
  cmd->args.konst.p = k;
  return v;
}

IRValue flI_getarg(IRFunction *F, lu_byte type, int n) {
  IRValue v;
  IRCommand *cmd = createvalue(F, type, IR_GETARG, &v);
  cmd->args.getarg.n = n;
  return v;
}

IRValue flI_load(IRFunction *F, lu_byte type, IRValue mem) {
  IRValue v;
  IRCommand *cmd = createvalue(F, type, IR_LOAD, &v);
  cmd->args.load.mem = mem;
  return v;
}

IRValue flI_store(IRFunction *F, lu_byte type, IRValue mem, IRValue val) {
  IRValue v;
  IRCommand *cmd = createvalue(F, type, IR_STORE, &v);
  cmd->args.store.mem = mem;
  cmd->args.store.v = val;
  assert((flI_getcmd(F, val)->type == type) ||
         (flI_getcmd(F, val)->type == IR_INTPTR && flI_isintt(type)));
  return v;
}

IRValue flI_binop(IRFunction *F, lu_byte op, IRValue l, IRValue r) {
  /* promote integers to luaint */
  IRCommand *lcmd = flI_getcmd(F, l);
  lu_byte outt = flI_isintt(lcmd->type) ? IR_INTPTR : lcmd->type;
  IRValue v;
  IRCommand *cmd = createvalue(F, outt, op, &v);
  cmd->args.binop.l = l;
  cmd->args.binop.r = r;
  assert(lcmd->type == flI_getcmd(F, r)->type);
  return v;
}

/*
 * Printing functions for debug
 */

static void printtype(lu_byte type) {
  switch (type) {
    case IR_CHAR:   printf("char"); break;
    case IR_SHORT:  printf("short"); break;
    case IR_INT:    printf("int"); break;
    case IR_LUAINT: printf("luaint"); break;
    case IR_INTPTR: printf("intptr"); break;
    case IR_LUAFLT: printf("luafloat"); break;
    case IR_PTR:    printf("ptr"); break;
    default: assert(0); break;
  }
}

static void printconst(lu_byte type, IRUConstant k) {
  switch (type) {
    case IR_INTPTR: printf("%td", k.i); break;
    case IR_LUAFLT: printf("%f", k.f); break;
    case IR_PTR:    printf("%p", k.p); break;
    default: assert(0); break;
  }
}

static void printbinop(lu_byte cmd) {
  switch (cmd) {
    case IR_ADD: printf("add"); break;
    case IR_SUB: printf("sub"); break;
    case IR_MUL: printf("mul"); break;
    case IR_DIV: printf("div"); break;
    default: assert(0); break;
  }
}

static void printvalue(IRValue v) {
  printf("v%d_%d", v.bb, v.cmd);
}

static void printcmd(IRFunction *F, IRId bbid, IRId cmdid) {
  IRValue v = {bbid, cmdid};
  IRCommand *cmd = flI_getcmd(F, v);
  printvalue(v);
  printf(" : ");
  printtype(cmd->type);
  printf(" = ");
  switch (cmd->cmdtype) {
    case IR_CONST:
      printf("const ");
      printconst(cmd->type, cmd->args.konst);
      break;
    case IR_GETARG:
      printf("getarg %d", cmd->args.getarg.n);
      break;
    case IR_LOAD:
      printf("load ");
      printvalue(cmd->args.load.mem);
      break;
    case IR_STORE:
      printf("store ");
      printvalue(cmd->args.store.mem);
      printf(" <- ");
      printvalue(cmd->args.store.v);
      break;
    case IR_ADD:
    case IR_SUB:
    case IR_MUL:
    case IR_DIV:
      printbinop(cmd->cmdtype);
      printvalue(cmd->args.binop.l);
      printvalue(cmd->args.binop.r);
      break;
  }
  printf("\n");
}

void IRprint(IRFunction *F) {
  IRId bbid, cmdid;
  printf("IR DEBUG - F (%p)\n", (void *)F);
  for (bbid = 0; bbid < F->nbbs; ++bbid) {
    IRBBlock *bb = flI_getbb(F, bbid);
    printf("bblock %d:\n", bbid);
    for (cmdid = 0; cmdid < bb->ncmds; ++cmdid)
      printcmd(F, bbid, cmdid);
    printf("\n");
  }
  printf("\n");
}


