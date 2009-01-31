
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
    void callV(const char * fname);
    void callV(const char * fname, lua_Number p1);
    lua_Number callN(const char * fname);
    lua_Number callN(const char * fname, lua_Number p1);

};
