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

#include <stdio.h>

#include "lprefix.h"

#include "lmem.h"
#include "lopcodes.h"
#include "lstate.h"

#include "fl_ir.h"
#include "fl_jit.h"

/*
 * Stores the jit compilation state.
 */
typedef struct JitState {
  lua_State *L;
  TraceRec *tr;
  IRFunction *F;
  IRValue vstate;
  IRValue vci;
  IRValue vbase;
  IRValue *rtable;
} JitState;

/*
 * Creates/destroy the jit state.
 */
static JitState *createjitstate(lua_State *L, TraceRec *tr) {
  JitState *J = luaM_new(L, JitState);
  J->L = L;
  J->tr = tr;
  J->F = flI_createfunc(L);
  J->vstate = IRNullValue;
  J->vci = IRNullValue;
  J->vbase = IRNullValue;
  J->rtable = luaM_newvector(L, tr->p->maxstacksize, IRValue);
  return J;
}

static void destroyjitstate(JitState *J) {
  flI_destroyfunc(J->F);
  luaM_freearray(J->L, J->rtable, J->tr->p->maxstacksize);
  luaM_free(J->L, J);
}

/*
 * Creates the entry basic block.
 * This block contains the trace (loop) invariants.
 */
static void initentryblock(JitState *J) {
  IRFunction *F = J->F;
  flI_createbb(F);
  J->vstate = flI_getarg(F, IR_PTR, 0);
  J->vci = flI_getarg(F, IR_PTR, 1);
  J->vbase = flI_getarg(F, IR_PTR, 2);
}

/*
 * Obtains the next instruction position.
 */
static l_mem getnextpc(l_mem oldpc, Instruction i) {
  l_mem pc = oldpc;
  switch (GET_OPCODE(i)) {
    case OP_FORLOOP:
      pc += GETARG_sBx(i) + 1;
      break;
    default:
      pc += 1;
      break;
  }
  return pc;
}

TraceRec *flJ_createtracerec(struct lua_State *L) {
  TraceRec *tr = luaM_new(L, TraceRec);
  tr->p = NULL;
  tr->start = NULL;
  tr->n = 0;
  tr->rt = NULL;
  tr->rtsize = 0;
  tr->completeloop = 0;
  return tr;
}

void flJ_destroytracerec(struct lua_State *L, TraceRec *tr) {
  luaM_freearray(L, tr->rt, tr->rtsize);
  luaM_free(L, tr);
}

void flJ_compile(struct lua_State *L, TraceRec *tr) {
  JitState *J = createjitstate(L, tr);
  IRFunction *F = J->F;
  Proto *p = tr->p;
  initentryblock(J);
  if (tr->completeloop) {
    IRId mainbb = flI_createbb(F);
    int i;
    l_mem pc = tr->start - p->code;
    for (i = 0; i < tr->n; ++i) {
      (void)mainbb;
      pc = getnextpc(pc, p->code[pc]);
    }
  }
  flI_print(J->F);
  destroyjitstate(J);
}

