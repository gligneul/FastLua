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

#include "lopcodes.h"
#include "lstate.h"

#include "fl_ir.h"
#include "fl_jit.h"

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
  return tr;
}

void flJ_destroytracerec(struct lua_State *L, TraceRec *tr) {
  luaM_freearray(L, tr->rt, tr->rtsize);
  luaM_free(L, tr);
}

void flJ_compile(struct lua_State *L, TraceRec *tr) {
  (void)L;
  (void)tr;
  (void)getnextpc;

  IRFunction *F = flI_createfunc(L);
  IRId bb = flI_createbb(F);
  flI_setcurrbb(F, bb);

  IRValue lstate = flI_getarg(F, IR_PTR, 0);
  flI_return(F, lstate);

  flI_print(F);
  flI_destroyfunc(F);

#if 0
  printf("flJ: %d\n", (int)sizeof(IRCommand));
  int i;
  Proto *p = tr->p;
  l_mem pc = tr->start - p->code;
  printf(" >>> ");
  for (i = 0; i < tr->n; ++i) {
    int op = GET_OPCODE(p->code[pc]);
    printf("[%lu]%s, ", pc, luaP_opnames[op]);
    pc = getnextpc(pc, p->code[pc]);
  }
  printf("\n");
#endif
}

