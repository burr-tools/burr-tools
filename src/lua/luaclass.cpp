/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "luaclass.h"

extern "C" {
#include "lauxlib.h"
}

#include <string.h>

luaClass_c::luaClass_c(void) {

  L = luaL_newstate();

}
luaClass_c::~luaClass_c(void) {

  lua_close(L);

}

/* functions to get and set variables */
void luaClass_c::setNumber(const char *name, lua_Number value) {
}
void luaClass_c::setString(const char *name, const char *value) {
}
void luaClass_c::setBool(const char *name, bool value) {
}

lua_Number luaClass_c::getNumber(const char *name) {
  lua_getglobal(L, name);
  if (!lua_isnumber(L, -1)) throw new luaTypeException_c();
  return lua_tointeger(L, -1);
}
bool luaClass_c::getBool(const char *name) {
  lua_getglobal(L, name);
  return lua_toboolean(L, -1) != 0;
}

/* functions to evaluate lua code */
int luaClass_c::doFile(const char *fname) {
  luaL_loadfile(L, fname) || lua_pcall(L, 0, 0, 0);
}
int luaClass_c::doString(const char *code) {
  luaL_loadbuffer(L, code, strlen(code), "line") || lua_pcall(L, 0, 0, 0);
}

/* functions that allow calling lua functions
 *
 * I decided to leave out the generic interface and only provide some
 * overloaded functions to call a lua function with specific parameters
 * the return value is encoded within the name, the othe parameters
 * should be selected by the ...
 */
void luaClass_c::callV(const char * fname) {
}
void luaClass_c::callV(const char * fname, lua_Number p1) {
}
lua_Number luaClass_c::callN(const char * fname) {
}
lua_Number luaClass_c::callN(const char * fname, lua_Number p1) {
}

