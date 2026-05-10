#pragma once

#include <cstddef>

typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *L);

typedef int (*luaopen_emmy_core_t)(lua_State *L);
extern luaopen_emmy_core_t p_luaopen_emmy_core;

// Lua 5.1.5 imports (resolved from lua51.dll at runtime by InitLuaImports)
#define LUA_REGISTRYINDEX (-10000)
#define LUA_GLOBALSINDEX (-10002)
#define LUA_TTABLE 5

typedef void (*lua_getfield_t)(lua_State *L, int idx, const char *k);
typedef void (*lua_setfield_t)(lua_State *L, int idx, const char *k);
typedef void (*lua_pushcclosure_t)(lua_State *L, lua_CFunction fn, int n);
typedef void (*lua_settop_t)(lua_State *L, int idx);
typedef void (*lua_createtable_t)(lua_State *L, int narr, int nrec);
typedef void (*lua_pushvalue_t)(lua_State *L, int idx);
typedef int (*lua_type_t)(lua_State *L, int idx);
typedef void (*lua_pushstring_t)(lua_State *L, const char *s);
typedef void (*lua_pushinteger_t)(lua_State *L, ptrdiff_t n);
typedef int (*lua_pcall_t)(lua_State *L, int nargs, int nresults, int errfunc);
typedef const char *(*lua_tolstring_t)(lua_State *L, int idx, size_t *len);
typedef void (*lua_pushboolean_t)(lua_State *L, int b);
typedef int (*lua_toboolean_t)(lua_State *L, int idx);

extern lua_getfield_t p_lua_getfield;
extern lua_setfield_t p_lua_setfield;
extern lua_pushcclosure_t p_lua_pushcclosure;
extern lua_settop_t p_lua_settop;
extern lua_createtable_t p_lua_createtable;
extern lua_pushvalue_t p_lua_pushvalue;
extern lua_type_t p_lua_type;
extern lua_pushstring_t p_lua_pushstring;
extern lua_pushinteger_t p_lua_pushinteger;
extern lua_pcall_t p_lua_pcall;
extern lua_tolstring_t p_lua_tolstring;
extern lua_pushboolean_t p_lua_pushboolean;
extern lua_toboolean_t p_lua_toboolean;

#define lua_getfield(L, i, k) p_lua_getfield((L), (i), (k))
#define lua_setfield(L, i, k) p_lua_setfield((L), (i), (k))
#define lua_pushcclosure(L, f, n) p_lua_pushcclosure((L), (f), (n))
#define lua_settop(L, i) p_lua_settop((L), (i))
#define lua_createtable(L, na, nr) p_lua_createtable((L), (na), (nr))
#define lua_pushvalue(L, i) p_lua_pushvalue((L), (i))
#define lua_type(L, i) p_lua_type((L), (i))
#define lua_getglobal(L, s) lua_getfield((L), LUA_GLOBALSINDEX, (s))
#define lua_setglobal(L, s) lua_setfield((L), LUA_GLOBALSINDEX, (s))
#define lua_pushcfunction(L, f) lua_pushcclosure((L), (f), 0)
#define lua_pop(L, n) lua_settop((L), -(n) - 1)
#define lua_pushstring(L, s) p_lua_pushstring((L), (s))
#define lua_pushinteger(L, n) p_lua_pushinteger((L), (n))
#define lua_pcall(L, na, nr, ef) p_lua_pcall((L), (na), (nr), (ef))
#define lua_tostring(L, i) p_lua_tolstring((L), (i), nullptr)
#define lua_pushboolean(L, b) p_lua_pushboolean((L), (b))
#define lua_toboolean(L, i) p_lua_toboolean((L), (i))

bool InitLuaImports();
