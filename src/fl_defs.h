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

/*
 * FastLua global definitions.
 */

#ifndef fl_defs_h
#define fl_defs_h

/* Foward declarations */
struct lua_State;
struct Proto;
struct AsmData;
struct JitTrace;

/* Numbers of opcode executions required to record a trace. */
#ifndef FL_JIT_THRESHOLD
#define FL_JIT_THRESHOLD 50
#endif

/* Global data that should be stored in lua_State. */
struct FLState {
  struct JitTrace *trace;   /* trace beeing recorded */
};

/* Jit information about each instruction in a function. */
union FLInstructionData {
  int count;                /* number of times executed; used in prof */
  struct AsmData *asmdata;  /* compiled function; used for execution */
};

/* Data that should be stored in lua Proto. */
struct FLProto {
  union FLInstructionData *instr;
};

/* Init/destroy FastLua state. */
void fl_initstate(struct lua_State *L);
void fl_closestate(struct lua_State *L);

/* Init/destroy FastLua proto. */
void fl_initproto(struct lua_State *L, struct Proto *p);
void fl_closeproto(struct lua_State *L, struct Proto *p);

#endif

