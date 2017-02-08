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
 * This module does the profiling step of the jit compiler.
 * The flrec_initproto routine changes the default opcodes of a Lua function to
 * the corresponding profiling ones. Then, the profiling opcodes call the
 * flrec_profile routine, which counts the number of times that a loop is
 * executed. When the inner part of the loop is executed enough times
 * (JIT_THRESHOLD), the fl_rec module is called and the trace is recorded.
 */

#ifndef fl_prof_h
#define fl_prof_h

#include "llimits.h"

#include "fl_defs.h"

struct CallInfo;

/* Swap the default opcodes for the profiling ones */
void flprof_initopcodes(Instruction *code, int n);

/* General profiling step.
 * loopcount should be greater than 0. */
void flprof_profile(struct lua_State *L, struct CallInfo *ci, int loopcount);

#endif

