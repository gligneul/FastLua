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
 * Some Lua instructions are replaced by FL instructions (eg. for profiling and
 * execution of jited code).
 * The original opcode is replaced by the OP_FLVM opcode and a FL opcode is
 * stored in the A argument.
 * Aditional information about the instruction is stored in the FLInstrExt
 * struct.
 * The extension vector only contains data about the converted instructions.
 * Therefore, the B argument is used to store the index of the instruction's
 * extension.
 */

#ifndef fl_instr_h
#define fl_instr_h

#include "llimits.h"
#include "lopcodes.h"

#include "fl_defs.h"
#include "fl_containers.h"

struct Proto;

/* FL opcodes. These opcodes are executed in the fl_vm. */
enum FLOpcode {
  FLOP_FORPREP_PROF,
  FLOP_FORLOOP_EXEC
};

/* Extra information about a FL instruction. */
typedef struct FLInstrExt {
  Instruction original;             /* original instruction */
  int index;                        /* instruction index in p->code */
  union {
    int count;                      /* number of times executed */
    struct AsmInstrData *asmdata;   /* compiled function */
  } u;
} FLInstrExt;

/* struct FLInstrExt container. */
TSCC_DECL_VECTOR_WA(FLInstrExtVector, fliv_, FLInstrExt,
    struct lua_State *)

/* Obtain the instruction index given the address. */
#define fli_instrindex(p, addr) ((addr) - (p)->code)

/* Obtain the current instruction index. */
#define fli_currentinstr(ci, p) fli_instrindex(p, ci->u.l.savedpc - 1)

/* Manipulate the fl fields in the Lua instruction. */
#define fli_getflop(p, i)           (GETARG_A(p->code[i]))
#define fli_setflop(p, i, op)       (SETARG_A(p->code[i], op))
#define fli_getextindex(p, i)       (GETARG_B(p->code[i]))
#define fli_setextindex(p, i, idx)  (SETARG_B(p->code[i], idx))
#define fli_isfl(p, i)              (GET_OPCODE(p->code[i]) == OP_FLVM)
#define fli_isexec(p, i) \
    (fli_isfl(p, i) && fli_getflop(p, i) >= FLOP_FORLOOP_EXEC)

/* Initialize the profiling instructions in the Lua proto. */
void fli_loadproto(struct Proto *p);

/* Obtain the instruction's extension. */
FLInstrExt *fli_getext(struct Proto *p, int i);

/* Convert an instruction back to the original one. */
void fli_reset(struct Proto *p, int i);

/* Convert an instruction to the profiling one. */
void fli_toprof(struct Proto *p, int i);

/* Convert an instruction to the jit one. */
void fli_tojit(struct Proto *p, int i);

#endif

