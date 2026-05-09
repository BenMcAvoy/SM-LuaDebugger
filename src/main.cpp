#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <cstdio>
#include <MinHook.h>

#include <unordered_map>
#include <string>

#include "emmy.dll.h"

typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *L);

typedef int (*luaopen_emmy_core_t)(lua_State *L);
static luaopen_emmy_core_t p_luaopen_emmy_core = nullptr;

// Lua 5.1.5 imports (resolved from lua51.dll at runtime in MainThread)
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

static lua_getfield_t p_lua_getfield = nullptr;
static lua_setfield_t p_lua_setfield = nullptr;
static lua_pushcclosure_t p_lua_pushcclosure = nullptr;
static lua_settop_t p_lua_settop = nullptr;
static lua_createtable_t p_lua_createtable = nullptr;
static lua_pushvalue_t p_lua_pushvalue = nullptr;
static lua_type_t p_lua_type = nullptr;
static lua_pushstring_t p_lua_pushstring = nullptr;
static lua_pushinteger_t p_lua_pushinteger = nullptr;
static lua_pcall_t p_lua_pcall = nullptr;
static lua_tolstring_t p_lua_tolstring = nullptr;

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

constexpr std::uintptr_t LuaVM_Initialize_RVA = 0x54A7F0;
constexpr std::uintptr_t DirectoryManager_RVA = 0x1267770;
constexpr std::uintptr_t BuildChunkName_RVA = 0x54E8E0;  // sub_14054E8E0
constexpr std::uintptr_t StdStringAssign_RVA = 0x24CCD0; // sub_14024CCD0 (std::string assign)

struct StringHash
{
    using is_transparent = void;

    inline std::size_t operator()(const std::string &str) const noexcept
    {
        return std::hash<std::string>{}(str);
    }

    inline std::size_t operator()(const std::string_view &str) const noexcept
    {
        return std::hash<std::string_view>{}(str);
    }
};

// https://github.com/Scrap-Mods/SmSdk/blob/main/include/SmSdk/DirectoryManager.hpp
class DirectoryManager
{
    /* 0x000 */ char pad_0x0[0x8];
    /* 0x008 */ std::unordered_map<std::string, std::string, StringHash, std::equal_to<>> pathMap;

public:
    bool getReplacement(const std::string_view &key, std::string_view &replacement)
    {
        auto it = pathMap.find(key);
        if (it == pathMap.end())
            return false;
        replacement = it->second;
        return true;
    }

    bool replacePathR(std::string &path)
    {
        if (path.empty() || path[0] != L'$')
            return false;

        const char *begin = path.data();
        const char *key = std::strchr(begin, L'/');
        if (key == nullptr)
            return false;

        const size_t keyIdx = key - begin;

        const std::string keyChunk = path.substr(0, keyIdx);
        const auto it = pathMap.find(keyChunk);
        if (it == pathMap.end())
            return false;

        path = (it->second + path.substr(keyIdx));
        return true;
    }

    static bool ReplacePathR(std::string &path)
    {
        DirectoryManager *dirMgr = *(DirectoryManager **)((std::uintptr_t)GetModuleHandleA(nullptr) + DirectoryManager_RVA);
        if (!dirMgr)
            return false;
        return dirMgr->replacePathR(path);
    }
};

struct LuaVM
{
    /* 0x000 */ lua_State *L;
};

using LuaVM_Initialize_t = void *(*)(LuaVM * thisPtr, void **modOpenerLists, int envMode);
LuaVM_Initialize_t original_LuaVM_Initialize = nullptr;

// Chunkname builder: writes full source path into the std::string at a2 then truncates if >46 chars.
using BuildChunkName_t = void *(*)(void *a1, void *a2, const void *a3);
BuildChunkName_t original_BuildChunkName = nullptr;
using StdStringAssign_t = void *(*)(std::string * dst, std::string *src);
static StdStringAssign_t p_StdStringAssign = nullptr;

// Used to resolve full path as chunk name
void *hk_BuildChunkName(void *unk, std::string *dst, const char **src)
{
    std::string path{*src};
    DirectoryManager::ReplacePathR(path);
    p_StdStringAssign(dst, &path);

    return dst;
}

void *hk_LuaVM_Initialize(LuaVM *thisPtr, void **modOpenerLists, int envMode)
{
    // Run original first — it creates the Lua state / env
    void *ret = original_LuaVM_Initialize(thisPtr, modOpenerLists, envMode);

    // If not game env early exit
    if (envMode != 0)
        return ret;

    lua_State *L = thisPtr->L;

    // Resolve emmy_core.dll once.
    if (!p_luaopen_emmy_core)
    {
        char exeDir[MAX_PATH];
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        if (char *slash = strrchr(exeDir, '\\'))
            slash[1] = '\0';
        char dllPath[MAX_PATH];
        snprintf(dllPath, sizeof(dllPath), "%semmy_core.dll", exeDir);

        if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES)
        {
            HANDLE h = CreateFileA(dllPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                DWORD written = 0;
                WriteFile(h, emmy_core_dll, (DWORD)emmy_core_dll_size, &written, nullptr);
                CloseHandle(h);
            }
        }

        HMODULE emmy = LoadLibraryA(dllPath);
        if (emmy)
            p_luaopen_emmy_core = (luaopen_emmy_core_t)GetProcAddress(emmy, "luaopen_emmy_core");
    }
    if (!p_luaopen_emmy_core)
        return ret;

    // Open emmy_core directly (no require/package — those don't live in unsafe_env).
    // luaopen_emmy_core leaves the emmy module table on the stack.
    lua_pushcfunction(L, p_luaopen_emmy_core);
    if (lua_pcall(L, 0, 1, 0) != 0)
    {
        const char *err = lua_tostring(L, -1);
        MessageBoxA(nullptr, err ? err : "luaopen_emmy_core failed", "SM Lua Debugger", MB_OK | MB_ICONERROR);
        lua_pop(L, 1);
        return ret;
    }
    // stack: emmy

    // Expose emmy + debug inside unsafe_env (the original Initialize already created it in _G).
    // debug exists in _G but isn't in unsafe_env — emmy needs it to resolve source info.
    lua_getglobal(L, "unsafe_env");
    if (lua_type(L, -1) == LUA_TTABLE)
    {
        lua_pushvalue(L, -2);        // copy emmy table
        lua_setfield(L, -2, "emmy"); // unsafe_env.emmy = emmy

        lua_getglobal(L, "debug");
        if (lua_type(L, -1) == LUA_TTABLE)
            lua_setfield(L, -2, "debug"); // unsafe_env.debug = _G.debug
        else
            lua_pop(L, 1);
    }
    lua_pop(L, 1); // pop unsafe_env (or non-table sentinel)

    // tcpListen("127.0.0.1", 9966) — only the first env to get here actually opens the port.
    static bool s_listening = false;
    if (!s_listening)
    {
        lua_getfield(L, -1, "tcpListen");
        lua_pushstring(L, "127.0.0.1");
        lua_pushinteger(L, 9966);
        if (lua_pcall(L, 2, 0, 0) != 0)
        {
            const char *err = lua_tostring(L, -1);
            MessageBoxA(nullptr, err ? err : "emmy.tcpListen failed", "SM Lua Debugger", MB_OK | MB_ICONERROR);
            lua_pop(L, 1);
        }
        else
        {
            s_listening = true;
            MessageBoxA(nullptr, "Emmy debugger listening on 127.0.0.1:9966.\nAttach from VSCode now.", "SM Lua Debugger", MB_OK);
        }
    }
    lua_pop(L, 1); // pop emmy

    return ret;
}

DWORD MainThread(HMODULE hModule)
{
    if (MH_Initialize() != MH_OK)
        return 1;

    HMODULE lua51 = GetModuleHandleA("lua51.dll");
    if (!lua51)
        lua51 = LoadLibraryA("lua51.dll");
    if (!lua51)
        return 1;
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

    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    uintptr_t p_LuaVM_Initialize = base + LuaVM_Initialize_RVA;
    if (MH_CreateHook((LPVOID)p_LuaVM_Initialize, (LPVOID)hk_LuaVM_Initialize, (LPVOID *)&original_LuaVM_Initialize) != MH_OK)
        return 1;

    if (MH_EnableHook((LPVOID)p_LuaVM_Initialize) != MH_OK)
        return 1;

    // Hook the chunkname builder so emmy sees full source paths instead of "...tail" form.
    p_StdStringAssign = (StdStringAssign_t)(base + StdStringAssign_RVA);
    uintptr_t p_BuildChunkName = base + BuildChunkName_RVA;
    if (MH_CreateHook((LPVOID)p_BuildChunkName, (LPVOID)hk_BuildChunkName, (LPVOID *)&original_BuildChunkName) != MH_OK)
        return 1;
    if (MH_EnableHook((LPVOID)p_BuildChunkName) != MH_OK)
        return 1;

    while (!GetAsyncKeyState(VK_END))
    {
        // Keep code loaded
        Sleep(100);
    }

    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr);
    }

    return TRUE;
}