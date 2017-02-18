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
 * This module records the executed opcodes and create the trace.
 * After the trace is recorded, if it is valid, the fl_jit module is called and
 * the trace will be compiled into machine code.
 */

#ifndef fl_rec_h
#define fl_rec_h

#include "llimits.h"

/* Foward declarations */
struct lua_State;
struct CallInfo;

/* Obtains the recording enabled/disabled flag. */
#define flrec_isrecording(L) (L->fl.trace != NULL)

/* Start the recording. */
void flrec_start(struct lua_State *L);

/* Record the current opcode and write it into the current trace. */
#define flrec_record(L, ci) \
  do { if (flrec_isrecording(L)) flrec_record_(L, ci); } while (0)

void flrec_record_(struct lua_State *L, struct CallInfo *ci);

#endif

