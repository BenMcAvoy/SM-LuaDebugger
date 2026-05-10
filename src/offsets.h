#pragma once

// Offsets for SM build 0.7.4.778

// Imports -> `luaL_newstate` -> XREF (first) -> Look for function call after `lua_atpanic`, that should be `LuaVM_Initialize`
constexpr std::uintptr_t LuaVM_Initialize_RVA = 0x54A7F0;

// Strings -> "DirectoryManager.cpp" -> XREF (first) -> look for `qword_` refernce above this line, should be condition of an if satement (e.g. `if ( qword_14126770 != Block )`)
constexpr std::uintptr_t DirectoryManager_RVA = 0x1267770;

// Strings -> "..." -> XREF (last call, takes 3 opaquely typed arguments)
constexpr std::uintptr_t BuildChunkName_RVA = 0x54E8E0;