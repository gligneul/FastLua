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
 * This module compiles the recorded trace into machine code.
 */

#ifndef fl_jit_h
#define fl_jit_h

#include "llimits.h"

#include "fl_containers.h"
#include "fl_defs.h"

/* Foward declarations */
struct lua_State;

/* Types defined in this module */
typedef struct JitTrace JitTrace;
union JitRTInfo;

/* Containers */
TSCC_DECL_VECTOR_WA(JitRTInfoVector, fljit_rtvec_, union JitRTInfo,
    struct lua_State *)

/* Runtime information for each opcode */
union JitRTInfo {
  struct { lu_byte type; } forloop;
  struct { lu_byte rb, rc; } binop;
};

/* JitTrace */
struct JitTrace {
  struct lua_State *L;          /* Lua state */
  struct Proto *p;              /* Lua function */
  const Instruction *start;     /* first opcode of the trace */
  size_t n;                     /* number of opcodes in the recording */
  JitRTInfoVector *rtinfo;      /* runtime info for each opcode */
  lu_byte completeloop;         /* tell if the trace is a full loop */
};

/* Creates/destroys a trace recording. */
JitTrace *fljit_createtrace(struct lua_State *L);
void fljit_destroytrace(JitTrace *tr);

/* Compiles the trace recording. */
void fljit_compile(JitTrace *tr);

#endif

