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

#include <stdio.h>

#ifdef FL_LOGGER

#include "lprefix.h"
#include "lobject.h"
#include "lstate.h"

#include "fl_logger.h"

int fll_enable = FL_LOGGER_ERROR;

#define fllog_impl(format) \
  do { \
    if (fll_enable < FL_LOGGER_ALL) return; \
    va_list args; \
    va_start(args, format); \
    vfprintf(stderr, format, args); \
    va_end(args); \
  } while (0)

void fllog(const char *format, ...) {
  fllog_impl(format);
}

void fllogln(const char *format, ...) {
  fllog_impl(format);
  fputs("\n", stderr);
}

void fll_write(const void *buffer, size_t nbytes) {
  if (fll_enable < FL_LOGGER_ALL) return;
  fwrite(buffer, sizeof(char), nbytes, stderr);
}

void fll_error_(const char *file, int line, const char *message) {
  if (fll_enable < FL_LOGGER_ERROR) return;
  fprintf(stderr, "error at line %d in %s: %s\n", line, file, message);
}

void fll_dumpstack(lua_State *L) {
  StkId pos;
  for (pos = L->ci->u.l.base; pos != L->top; ++pos) {
    size_t len;
    const char *s;
    TValue val = *pos;
    luaO_tostring(L, &val);
    fllog("%p: ", (void*)pos);
    len = vslen(&val);
    s = svalue(&val);
    fll_write(s, len);
    fllog("\n");
  }
}

#endif

