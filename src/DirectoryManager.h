#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#include <string_view>
#include <unordered_map>

#include "offsets.h"

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
