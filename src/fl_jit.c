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

#include "fl_jit.h"

TraceRec *flJ_createtracerec(struct lua_State *L) {
  TraceRec *tr = luaM_new(L, TraceRec);
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
#if 0
  int i = 0;
  printf(">>> ");
  for (i = 0; i < tr->n; ++i) {
    int op = GET_OPCODE(tr->code[i]);
    printf("%s, ", luaP_opnames[op]);
  }
  printf("\n");
#endif
}

