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

/* Runtime information for each instruction */
struct TraceInstr {
  Instruction instr;            /* instruction */
  union {                       /* specific fields for each opcode */
    struct { lu_byte steplt0; } forloop;
  } u;
};

/* TraceInstr container */
TSCC_DECL_VECTOR_WA(TraceInstrVector, flt_rtvec_, struct TraceInstr,
    struct lua_State *)
#define flt_rtvec_foreach(vec, val, cmd) \
    TSCC_VECTOR_FOREACH(flt_rtvec_, vec, struct TraceInstr, val, cmd)

/* Runtime information about the registers */
struct TraceRegister {
  lu_byte tag;                  /* register's tag */
  lu_byte loadedtag;            /* tag when loaded from the stack */
  lu_byte tagset : 1;           /* the tag was set */
  lu_byte checktag : 1;         /* the tag should be checked */
  lu_byte loaded : 1;           /* should be loaded from the stack */
  lu_byte set : 1;              /* the register changed */
};

/* TraceRecording */
typedef struct TraceRecording {
  struct lua_State *L;          /* Lua state */
  struct Proto *p;              /* Lua function */
  const Instruction *start;     /* first instruction of the trace */
  TraceInstrVector *instrs;     /* runtime info for each instruction */
  struct TraceRegister *regs;   /* runtime info for each register */
  lu_byte completeloop;         /* tell if the trace is a full loop */
} TraceRecording;

/* Creates/destroys a trace recording. */
TraceRecording *flt_createtrace(struct lua_State *L);
void flt_destroytrace(TraceRecording *tr);

#endif

