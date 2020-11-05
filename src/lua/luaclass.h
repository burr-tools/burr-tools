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

extern "C" {
#include "lua.h"
}


class luaTypeException_c {
};

/* the lua class encapsulates one lua state with all necessary functions for interaction with it */
class luaClass_c {

  private:

    lua_State *L;

  public:

    luaClass_c(void);
    ~luaClass_c(void);

    /* functions to get and set variables */
    void setNumber(const char *name, lua_Number value);
    void setString(const char *name, const char *value);
    void setBool(const char *name, bool value);

    lua_Number getNumber(const char *name);
    bool getBool(const char *name);

    /* functions to evaluate lua code */
    int doFile(const char *fname);
    int doString(const char *code);

    /* functions that allow calling lua functions
     *
     * I decided to leave out the generic interface and only provide some
     * overloaded functions to call a lua function with specific parameters
     * the return value is encoded within the name, the othe parameters
     * should be selected by the ...
     */
    // void callV(const char * fname);
    // void callV(const char * fname, lua_Number p1);
    // lua_Number callN(const char * fname);
    // lua_Number callN(const char * fname, lua_Number p1);

};
