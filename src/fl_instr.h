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
 * The original opcode is replaced by the OP_FLVM opcode and a new FL opcode is
 * stored in the A argument of the original instruction.
 * Aditional information about the instruction is stored in the FLInstrExt
 * struct.
 * The extension vector only contains data about the converted instructions.
 * Finaly, the B argument is used to store the index of the instruction's
 * extension.
 */

#ifndef fl_instr_h
#define fl_instr_h

#include "llimits.h"

#include "fl_containers.h"

struct Proto;

/* FL opcodes. These opcodes are executed in the fl_vm. */
enum FLOpcode {
  FLOP_FORPREP_PROF,
  FLOP_FORLOOP_EXEC
};

/* Extra information about a FL instruction. */
struct FLInstrExt {
  Instruction original;             /* original instruction */
  Instruction *address;             /* original instruction address */
  union {
    int count;                      /* number of times executed */
    struct AsmInstrData *asmdata;   /* compiled function */
  } u;
};

/* struct FLInstrExt container. */
TSCC_DECL_VECTOR(FLInstrExtVector, fliv_, struct FLInstrExt)

/* Obtain the instruction index given the address. */
#define fli_instrindex(p, addr)     ((addr) - (p)->code)

/* Obtain a instruction given the proto and the index. */
#define fli_getinstr(p, idx)        (p->code + idx)

/* Obtain the current instruction. */
#define fli_currentinstr(ci, p)     (Instruction *)(ci->u.l.savedpc - 1)

/* Foreach instruction in the proto. */
#define fli_foreach(p, i, cmd) \
  do { \
    Instruction *i = p->code; \
    Instruction *last##i = i + p->sizecode; \
    for (; i != last##i; ++i) { \
      cmd; \
    } \
  } while (0)

/* Manipulate the fl fields in the Lua instruction. */
#define fli_getflop(i)              (GETARG_A(*i))
#define fli_setflop(i, op)          (SETARG_A(*i, op))
#define fli_getextindex(i)          (GETARG_B(*i))
#define fli_setextindex(i, idx)     (SETARG_B(*i, idx))
#define fli_isfl(i)                 (GET_OPCODE(*i) == OP_FLVM)
#define fli_isexec(i) \
    (fli_isfl(i) && fli_getflop(i) >= FLOP_FORLOOP_EXEC)

/* Obtain the instruction's extension. */
struct FLInstrExt *fli_getext(struct Proto *p, Instruction *i);

/* Convert an instruction back to the original one. */
void fli_reset(struct Proto *p, Instruction *i);

/* Convert an instruction to the profiling one. */
void fli_toprof(struct Proto *p, Instruction *i);

/* Convert an instruction to the jit one. */
void fli_tojit(struct Proto *p, Instruction *i);

#endif

