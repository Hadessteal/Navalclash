#pragma once

#include <cstddef>

extern "C" {
struct lua_State;
using lua_Number = double;
using lua_Integer = long long;
using lua_CFunction = int (*)(lua_State *);

int lua_gettop(lua_State *L);
void lua_getfield(lua_State *L, int index, const char *name);
int lua_isnumber(lua_State *L, int index);
lua_Number lua_tonumber(lua_State *L, int index);
void lua_settop(lua_State *L, int index);
int lua_isstring(lua_State *L, int index);
const char *lua_tostring(lua_State *L, int index);
void lua_createtable(lua_State *L, int array_count, int record_count);
void lua_pushnumber(lua_State *L, lua_Number value);
void lua_setfield(lua_State *L, int index, const char *name);
void lua_pushlstring(lua_State *L, const char *value, std::size_t length);
void lua_pushinteger(lua_State *L, lua_Integer value);
void lua_rawseti(lua_State *L, int index, int array_index);
int lua_isnil(lua_State *L, int index);
int lua_toboolean(lua_State *L, int index);
void lua_pushnil(lua_State *L);
void lua_pushboolean(lua_State *L, int value);
void lua_rawgeti(lua_State *L, int index, int array_index);
std::size_t lua_objlen(lua_State *L, int index);
void luaL_checktype(lua_State *L, int index, int type);
lua_Number luaL_checknumber(lua_State *L, int index);
lua_Number luaL_optnumber(lua_State *L, int index, lua_Number fallback);
}

#define LUA_TTABLE 5
#define lua_pop(L, count) lua_settop((L), -(count)-1)
#define lua_istable(L, index) 1

class Server;

class ModApiBase {
protected:
    static Server *getServer(lua_State *L);
public:
    static bool registerFunction(lua_State *L, const char *name, lua_CFunction function, int top);
};
