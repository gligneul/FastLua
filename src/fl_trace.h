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
 * The structure that contains the recored trace data.
 */

#ifndef fl_trace_h
#define fl_trace_h

#include "fl_containers.h"

/* Foward declarations */
struct lua_State;

/* Types defined in this module */
typedef struct JitTrace JitTrace;

/* Containers */
struct JitRTInfo;
TSCC_DECL_VECTOR_WA(JitRTInfoVector, flt_rtvec_, struct JitRTInfo,
    struct lua_State *)
#define flt_rtvec_foreach(vec, val, cmd) \
    TSCC_VECTOR_FOREACH(flt_rtvec_, vec, struct JitRTInfo, val, cmd)

/* Runtime information for each instruction */
struct JitRTInfo {
  Instruction instr;            /* instruction */
  union {                       /* specific fields for each opcode */
    struct { lu_byte type; } forloop;
    struct { lu_byte rb, rc; } binop;
  } u;
};

/* JitTrace */
struct JitTrace {
  struct lua_State *L;          /* Lua state */
  struct Proto *p;              /* Lua function */
  const Instruction *start;     /* first instruction of the trace */
  size_t n;                     /* number of instruction in the recording */
  JitRTInfoVector *rtinfo;      /* runtime info for each instruction */
  lu_byte completeloop;         /* tell if the trace is a full loop */
};

/* Creates/destroys a trace recording. */
JitTrace *flt_createtrace(struct lua_State *L);
void flt_destroytrace(JitTrace *tr);

#endif

