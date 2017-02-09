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

#include <string.h>

#include "lprefix.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "fl_logger.h"

/*
 * Change the logger level.
 * Parameters:
 *  level : string      Possible values: 'none', 'error', 'all'
 */
static int logger(lua_State *L) {
  const char *level = luaL_checkstring(L, 1);
  if (!strcmp(level, "none"))
    fll_enable = FL_LOGGER_NONE;
  else if (!strcmp(level, "error"))
    fll_enable = FL_LOGGER_ERROR;
  else if (!strcmp(level, "all"))
    fll_enable = FL_LOGGER_ALL;
  else
    luaL_error(L, "bad argument #1 to 'logger' "
                  "('none', 'error' or 'all' expected)");
  return 0;
}

static const luaL_Reg jit_funcs[] = {
  {"logger", logger},
  {NULL, NULL}
};

LUAMOD_API int luaopen_jit(lua_State *L) {
  luaL_newlib(L, jit_funcs);
  return 1;
}

