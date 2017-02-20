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
 * VM for FL instructions.
 */

#ifndef fl_vm_h
#define fl_vm_h

#include "fl_instr.h"

struct lua_State;
struct lua_TValue;

/* Counts the number of times that a loop is executed. When the inner part of
 * the loop is executed enough times (JIT_THRESHOLD), the fl_rec module is
 * called and the trace is recorded.  */
void flvm_profile(struct lua_State *L, CallInfo *ci, int loopcount);

#define flvm_execute() { \
  int idx = fli_currentinstr(ci, cl->p); \
  i = fli_getext(cl->p, idx)->original; \
  ra = RA(i); \
  switch (fli_getflop(cl->p, idx)) { \
    case FLOP_FORPREP_PROF: { \
      TValue *init = ra; \
      TValue *plimit = ra + 1; \
      TValue *pstep = ra + 2; \
      lua_Integer ilimit; \
      int stopnow; \
      lua_Integer loopcount = 0; \
      if (ttisinteger(init) && ttisinteger(pstep) && \
          forlimit(plimit, &ilimit, ivalue(pstep), &stopnow)) { \
        /* all values are integer */ \
        lua_Integer initv = (stopnow ? 0 : ivalue(init)); \
        setivalue(plimit, ilimit); \
        setivalue(init, intop(-, initv, ivalue(pstep))); \
        if (ivalue(pstep) == 0) \
          loopcount = FL_JIT_THRESHOLD; \
        else \
          loopcount = intop(/, intop(-, ilimit, ivalue(init)), ivalue(pstep)); \
      } \
      else {  /* try making all values floats */ \
        lua_Number ninit; lua_Number nlimit; lua_Number nstep; \
        if (!tonumber(plimit, &nlimit)) \
          luaG_runerror(L, "'for' limit must be a number"); \
        setfltvalue(plimit, nlimit); \
        if (!tonumber(pstep, &nstep)) \
          luaG_runerror(L, "'for' step must be a number"); \
        setfltvalue(pstep, nstep); \
        if (!tonumber(init, &ninit)) \
          luaG_runerror(L, "'for' initial value must be a number"); \
        setfltvalue(init, luai_numsub(L, ninit, nstep)); \
        if (nstep == 0.0) \
          loopcount = FL_JIT_THRESHOLD; \
        else \
          loopcount = cast_int((nlimit - fltvalue(init)) / nstep); \
      } \
      if (loopcount > 0) { \
        short lc = (loopcount > FL_JIT_THRESHOLD ? \
                    FL_JIT_THRESHOLD : loopcount); \
        flvm_profile(L, ci, lc); \
      } \
      ci->u.l.savedpc += GETARG_sBx(i); \
      break; \
    } \
    case FLOP_FORLOOP_EXEC: { \
      Proto *p = cl->p; \
      int instr = fli_currentinstr(ci, p); \
      AsmFunction f = flasm_getfunction(p, instr); \
      switch (f(L, base)) { \
        case FL_SUCCESS: \
          break; \
        case FL_EARLY_EXIT: \
          flasm_destroy(L, p, instr); \
          goto l_forloop; \
          break; \
        case FL_SIDE_EXIT: \
          flasm_destroy(L, p, instr); \
          lua_assert(0); \
          break; \
        default: \
          lua_assert(0); \
          break; \
      } \
      break; \
    } \
    default: { \
      fll_error("unhandled flop"); \
      break; \
    } \
  } \
}


#endif

