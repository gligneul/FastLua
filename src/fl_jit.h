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

/*
 * This module is responsable for compiling the recorded trace into machine
 * code.
 */

#ifndef fl_jit_h
#define fl_jit_h

struct lua_State;

/*
 * Runtime information for each instruction
 */
typedef struct RuntimeRec {
  union {
    lu_byte forlooptype;
    struct {
      lu_byte rb, rc;
    } binoptypes;
  } u;
} RuntimeRec;

/*
 * Trace recording
 */
typedef struct TraceRec {
  Proto *p;
  const Instruction *start;
  int n;
  RuntimeRec *rt;
  int rtsize;
} TraceRec;

/*
 * Creates/destroys a trace recording
 */
TraceRec *flJ_createtracerec(struct lua_State *L);
void flJ_destroytracerec(struct lua_State *L, TraceRec *tr);

/*
 * Compiles the trace recording
 */
void flJ_compile(struct lua_State *L, TraceRec *tr);

#endif

