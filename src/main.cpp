#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "emmy.dll.h"
#include "lua.h"
#include "offsets.h"
#include "DirectoryManager.h"

#include <MinHook.h>
#include <string>

static HMODULE LoadEmbeddedDll(const std::uint8_t *data, std::size_t size)
{
    wchar_t tempDir[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempDir))
        return nullptr;
    if (!GetTempFileNameW(tempDir, L"sml", 0, tempFile))
        return nullptr;

    HANDLE h = CreateFileW(tempFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return nullptr;

    DWORD written = 0;
    if (!WriteFile(h, data, (DWORD)size, &written, nullptr) || written != size)
    {
        CloseHandle(h);
        return nullptr;
    }
    FlushFileBuffers(h);

    HMODULE mod = LoadLibraryExW(tempFile, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    CloseHandle(h); // unlinks the file; the mapped image stays loaded
    return mod;
}

using LuaVM_Initialize_t = void *(*)(lua_State * *thisPtr, void **modOpenerLists, int envMode);
LuaVM_Initialize_t original_LuaVM_Initialize = nullptr;

using BuildChunkName_t = void *(*)(void *a1, void *a2, const void *a3);
BuildChunkName_t original_BuildChunkName = nullptr;

// Used to resolve full path as chunk name instead of truncated "..." form, which confuses emmy.
void *hk_BuildChunkName(void *unk, std::string *dst, const char **src)
{
    std::string path{*src};
    DirectoryManager::ReplacePathR(path);
    *dst = path;

    return dst;
}

void LoadEmmyCore(lua_State *L)
{
    // Resolve emmy_core.dll once.
    if (!p_luaopen_emmy_core)
    {
        HMODULE emmy = LoadEmbeddedDll(emmy_core_dll, emmy_core_dll_size);
        if (emmy)
            p_luaopen_emmy_core = (luaopen_emmy_core_t)GetProcAddress(emmy, "luaopen_emmy_core");
    }

    if (p_luaopen_emmy_core)
    {
        lua_pushcfunction(L, p_luaopen_emmy_core);
        if (lua_pcall(L, 0, 1, 0) != 0)
        {
            const char *err = lua_tostring(L, -1);
            MessageBoxA(nullptr, err ? err : "luaopen_emmy_core failed", "SM Lua Debugger", MB_OK | MB_ICONERROR);
            lua_pop(L, 1);
        }
    }
}

static void InjectIntoUnsafeEnv(lua_State *L)
{
    lua_getglobal(L, "debug");   // Get debug table
    lua_pushvalue(L, -2);        // Push `emmy` into it
    lua_setfield(L, -2, "emmy"); // debug.emmy = emmy

    lua_getglobal(L, "unsafe_env");
    if (lua_type(L, -1) == LUA_TTABLE)
    {
        lua_pushvalue(L, -2);         // debug
        lua_setfield(L, -2, "debug"); // unsafe_env.debug = debug
    }

    lua_pop(L, 1); // unsafe_env (or non-table)
    lua_pop(L, 1); // debug
    lua_pop(L, 1); // emmy
}

static void StartEmmyListenerOnce(lua_State *L, const char *host, int port)
{
    static const char kListenKey[] = "SM_LuaDebugger.emmy_listening";

    lua_getfield(L, LUA_REGISTRYINDEX, kListenKey);
    bool already = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    if (already)
        return;

    lua_getfield(L, -1, "tcpListen");
    lua_pushstring(L, host);
    lua_pushinteger(L, port);
    if (lua_pcall(L, 2, 0, 0) != 0)
    {
        const char *err = lua_tostring(L, -1);
        MessageBoxA(nullptr, err ? err : "emmy.tcpListen failed", "SM Lua Debugger", MB_OK | MB_ICONERROR);
        lua_pop(L, 1);
        return;
    }

    lua_pushboolean(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, kListenKey);

    char msg[128];
    snprintf(msg, sizeof(msg), "Emmy debugger listening on %s:%d.\nAttach from VSCode now.", host, port);
    MessageBoxA(nullptr, msg, "SM Lua Debugger", MB_OK);
}

void *hk_LuaVM_Initialize(lua_State **pL, void **modOpenerLists, int envMode)
{
    void *ret = original_LuaVM_Initialize(pL, modOpenerLists, envMode);

    // If not game env early exit
    if (envMode != 0)
        return ret;

    LoadEmmyCore(*pL);                             // pushes emmy
    StartEmmyListenerOnce(*pL, "127.0.0.1", 9966); // reads emmy at top, stack-neutral
    InjectIntoUnsafeEnv(*pL);                      // consumes emmy

    return ret;
}

DWORD MainThread(HMODULE hModule)
{
    if (MH_Initialize() != MH_OK)
        return 1;

    if (!InitLuaImports())
        return 1;

    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    uintptr_t p_LuaVM_Initialize = base + LuaVM_Initialize_RVA;
    if (MH_CreateHook((LPVOID)p_LuaVM_Initialize, (LPVOID)hk_LuaVM_Initialize, (LPVOID *)&original_LuaVM_Initialize) != MH_OK)
        return 1;

    if (MH_EnableHook((LPVOID)p_LuaVM_Initialize) != MH_OK)
        return 1;

    uintptr_t p_BuildChunkName = base + BuildChunkName_RVA;
    if (MH_CreateHook((LPVOID)p_BuildChunkName, (LPVOID)hk_BuildChunkName, (LPVOID *)&original_BuildChunkName) != MH_OK)
        return 1;
    if (MH_EnableHook((LPVOID)p_BuildChunkName) != MH_OK)
        return 1;

    return 0;
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