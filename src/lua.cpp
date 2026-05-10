#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "lua.h"

luaopen_emmy_core_t p_luaopen_emmy_core = nullptr;

lua_getfield_t p_lua_getfield = nullptr;
lua_setfield_t p_lua_setfield = nullptr;
lua_pushcclosure_t p_lua_pushcclosure = nullptr;
lua_settop_t p_lua_settop = nullptr;
lua_createtable_t p_lua_createtable = nullptr;
lua_pushvalue_t p_lua_pushvalue = nullptr;
lua_type_t p_lua_type = nullptr;
lua_pushstring_t p_lua_pushstring = nullptr;
lua_pushinteger_t p_lua_pushinteger = nullptr;
lua_pcall_t p_lua_pcall = nullptr;
lua_tolstring_t p_lua_tolstring = nullptr;
lua_pushboolean_t p_lua_pushboolean = nullptr;
lua_toboolean_t p_lua_toboolean = nullptr;

bool InitLuaImports()
{
    HMODULE lua51 = GetModuleHandleA("lua51.dll");
    if (!lua51)
        lua51 = LoadLibraryA("lua51.dll");
    if (!lua51)
        return false;

    p_lua_getfield = (lua_getfield_t)GetProcAddress(lua51, "lua_getfield");
    p_lua_setfield = (lua_setfield_t)GetProcAddress(lua51, "lua_setfield");
    p_lua_pushcclosure = (lua_pushcclosure_t)GetProcAddress(lua51, "lua_pushcclosure");
    p_lua_settop = (lua_settop_t)GetProcAddress(lua51, "lua_settop");
    p_lua_createtable = (lua_createtable_t)GetProcAddress(lua51, "lua_createtable");
    p_lua_pushvalue = (lua_pushvalue_t)GetProcAddress(lua51, "lua_pushvalue");
    p_lua_type = (lua_type_t)GetProcAddress(lua51, "lua_type");
    p_lua_pushstring = (lua_pushstring_t)GetProcAddress(lua51, "lua_pushstring");
    p_lua_pushinteger = (lua_pushinteger_t)GetProcAddress(lua51, "lua_pushinteger");
    p_lua_pcall = (lua_pcall_t)GetProcAddress(lua51, "lua_pcall");
    p_lua_tolstring = (lua_tolstring_t)GetProcAddress(lua51, "lua_tolstring");
    p_lua_pushboolean = (lua_pushboolean_t)GetProcAddress(lua51, "lua_pushboolean");
    p_lua_toboolean = (lua_toboolean_t)GetProcAddress(lua51, "lua_toboolean");

    return p_lua_getfield && p_lua_setfield && p_lua_pushcclosure && p_lua_settop &&
           p_lua_createtable && p_lua_pushvalue && p_lua_type && p_lua_pushstring &&
           p_lua_pushinteger && p_lua_pcall && p_lua_tolstring &&
           p_lua_pushboolean && p_lua_toboolean;
}
