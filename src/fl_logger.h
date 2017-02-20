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
 * To use this module, the flag FL_LOGGER must be defined in the Makefile.
 */

#ifndef fl_logger_h
#define fl_logger_h

#ifdef FL_LOGGER

struct lua_State;

enum FLLoggerLevel {
  FL_LOGGER_NONE,
  FL_LOGGER_ERROR,
  FL_LOGGER_ALL
};

/* Enable/disable de logger. Since this is a debug module, global variables
 * are used and this module isn't thread safe. */
extern int fll_enable;

/* Print the message in stderr. */
void fllog(const char *format, ...);

/* Print the message in stderr and print a newline. */
void fllogln(const char *format, ...);

/* Print the buffer in stderr (usefull for Lua strings). */
void fll_write(const void *buffer, size_t nbytes);

/* Print the stack of the current function. */
void fll_dumpstack(struct lua_State *L);

/* Print an error message */
void fll_error_(const char *file, int line, const char *message);
#define fll_error(message) fll_error_(__FILE__, __LINE__, message)

/* Print the message if the condition fail. */
#define fll_assert(condition, message) \
  do { \
    if (!(condition)) \
      fll_error(message); \
  } while (0)

/*
 * Define empty function when the logger is disabled:
 */
#else
static __inline void fllog(const char *format, ...) { (void)format; }
#define fll_enable(enable) (void)0
#define fll_write(buffer, nbytes) (void)0
#define fll_dumpstack() (void)0
static __inline void fllog(const char *format, ...) { (void)format; }
#define fll_error(message) (void)0
#define fll_assert(condition, msg) (void)0
#endif

#endif

