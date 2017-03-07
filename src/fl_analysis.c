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

#include "llimits.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"

#include "fl_analysis.h"

#define SET     (1 << 1)
#define LOADED  (1 << 2)
#define PHI     (1 << 3)

#define getflag(A, regpos, flag) !!(A->isphivalue[regpos] & flag)
#define setflag(A, regpos, flag) (A->isphivalue[regpos] |= flag)

/* The register was read by the instruction. */
static void readregister(struct JitAnalysis *A, int regpos) {
  if (!getflag(A, regpos, SET))
    setflag(A, regpos, LOADED);
}

/* Call readregister if the argument is a register. */
static void readrk(struct JitAnalysis *A, int regpos) {
  if (!ISK(regpos)) readregister(A, regpos);
}

/* The value was set by the instruction. */
static void setregister(struct JitAnalysis *A, int regpos) {
  if (getflag(A, regpos, LOADED) && !getflag(A, regpos, SET))
    setflag(A, regpos, PHI);
  setflag(A, regpos, SET);
}

/* Analyse the instruction. */
static void analyseinstruction(struct JitAnalysis *A, struct JitRTInfo *rt) {
  Instruction i = rt->instr;
  switch (GET_OPCODE(i)) {
    case OP_LOADK:
      setregister(A, GETARG_A(i));
      break;
    case OP_ADD:
      readrk(A, GETARG_B(i));
      readrk(A, GETARG_C(i));
      setregister(A, GETARG_A(i));
      break;
    case OP_FORLOOP:
      readregister(A, GETARG_A(i));
      readregister(A, GETARG_A(i) + 1);
      readregister(A, GETARG_A(i) + 2);
      setregister(A, GETARG_A(i));
      setregister(A, GETARG_A(i) + 3);
      break;
    default:
      fll_error("unhandled opcode");
      break;
  }
}

/* Discover which registers must be phi values given the instruction. */
static void electphivalues(struct JitAnalysis *A, JitTrace *t){
  int i;
  for (i = 0; i < t->p->maxstacksize; ++i) {
    A->isphivalue[i] = getflag(A, i, PHI);
  }
}

void fla_initanalysis(struct JitAnalysis *A, JitTrace *t) {
  A->isphivalue = luaM_newvector(t->L, t->p->maxstacksize, lu_byte);
  memset(A->isphivalue, 0, t->p->maxstacksize * sizeof(lu_byte));
  flt_rtvec_foreach(t->rtinfo, rt, analyseinstruction(A, &rt));
  electphivalues(A, t);
}

void fla_closeanalysis(struct JitAnalysis *A, JitTrace *t) {
  luaM_freearray(t->L, A->isphivalue, t->p->maxstacksize);
}

