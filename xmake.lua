add_rules("mode.debug", "mode.release")

set_languages("c++23")

add_requires("minhook")

package("emmylua_debugger")
    set_kind("library")
    set_homepage("https://github.com/EmmyLua/EmmyLuaDebugger")
    set_description("EmmyLua Debugger")
 
    add_urls("https://github.com/EmmyLua/EmmyLuaDebugger/archive/refs/tags/$(version).tar.gz",
             "https://github.com/EmmyLua/EmmyLuaDebugger.git")
 
    add_versions("1.9.0", "a926312ecd0daebfbd85cdc935e4f3db2ad86837158749eeb8fad7e34ce2dba3")
    add_versions("1.8.7", "971684c7a344eedd3cc1a1c1faa52b6a8f3d2361acb2c39797dd7dc6e6453690")
    add_versions("1.8.6", "41e053856b4cf6641a22d43d64c78a25dfbbe8eaa4a8c90e87b20b76193f1af8")
    add_versions("1.8.5", "3262b90978ac3c4d825008e1658b03cb03db547d9bb05ff7f843b05c7092a668")
    add_versions("1.8.4", "e94590ae2ad6ad3ec6d238d6e5991b4d2a7f5942fc329f8f627e1d24315bdb88")
    add_versions("1.8.3", "a2803b4eec21400ca61691824e9e7689c1f14735470081a3ef0c5234aa4e590f")
    add_versions("1.8.2", "2ce5adbfad4055072d39302dccf794ec45800e84a5f3ba4784b373078a9dff8c")
    add_versions("1.8.1", "0dbbfefe798425323bd1f531463675460fce3418d73ef29b495e7369f8c76475")
    add_versions("1.8.0", "21e5ba1c82e4386cd8ad4f8c76511d70319b899b414d29ecdaba35649325d2ee")
    add_versions("1.7.1", "8757d372c146d9995b6e506d42f511422bcb1dc8bacbc3ea1a5868ebfb30015f")
    add_versions("1.6.3", "4e10cf1c729fc58f72880895e63618cb91d186ff3b55f270cdaa089a2f8b20bc")
 
    add_configs("luasrc", {description = "Use lua source.", default = true, type = "boolean"})
    add_configs("luaver", {description = "Set lua version.", default = "5.4", type = "string"})
 
    if is_plat("windows") then
        add_configs("emmy_tool", {description = "Build emmy_tool", default = false, type = "boolean"})
        add_configs("emmy_hook", {description = "Build emmy_hook", default = false, type = "boolean"})
    end
 
    add_deps("cmake")
 
    on_load(function (package)
        local suffix
        if package:is_plat("macosx") then
            suffix = ".dylib"
        elseif package:is_plat("windows") then
            suffix = ".dll"
        else
            suffix = ".so"
        end
        package:addenv("EMMYLUA_DEBUGGER", "bin/emmy_core" .. suffix)
        package:mark_as_pathenv("EMMYLUA_DEBUGGER")
    end)
 
    on_install("macosx", "linux", "windows", function (package)
        import("core.base.semver")
        -- Tarball releases ship empty submodule dirs (only README.txt). Clone what's needed.
        local luaver = package:config("luaver")
        local submodules = {}
        if luaver == "jit" then
            submodules["third-party/luajit"] = {url = "https://github.com/LuaJIT/LuaJIT.git", ref = "v2.1"}
        end
        for dir, info in pairs(submodules) do
            local hdr = path.join(dir, "src", "lua.h")
            if not os.isfile(hdr) then
                os.tryrm(dir)
                os.vrunv("git", {"clone", "--depth=1", "--branch", info.ref, info.url, dir})
            end
        end
        -- LuaJIT must be built with its own msvcbuild.bat to produce lua51.lib that emmy_debugger links.
        if luaver == "jit" and package:is_plat("windows") and not os.isfile("third-party/luajit/src/lua51.lib") then
            -- VS2026 preview's vswhere is broken, so pin to VS2022's vcvars directly.
            local vcvars = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
            assert(os.isfile(vcvars), "VS2022 vcvars64.bat not found at " .. vcvars)
            local jitsrc = path.absolute("third-party/luajit/src")
            -- Write a small bat file to avoid cmd /c arg-splitting/quoting issues with &&.
            local wrapper = path.absolute("luajit_build_wrapper.bat")
            io.writefile(wrapper, table.concat({
                "@echo off",
                "setlocal",
                "set NoDefaultCurrentDirectoryInExePath=",
                "call \"" .. vcvars .. "\" || exit /b 1",
                "cd /d \"" .. jitsrc .. "\" || exit /b 1",
                "call .\\msvcbuild.bat || exit /b 1",
            }, "\r\n"))
            os.vrunv("cmd.exe", {"/c", wrapper})
            os.tryrm(wrapper)
            assert(os.isfile("third-party/luajit/src/lua51.lib"), "LuaJIT msvcbuild.bat did not produce lua51.lib")
        end
        local configs = {}
        if package:config("luasrc") then
            table.insert(configs, "-DEMMY_USE_LUA_SOURCE=ON")
        end
        local luaver = package:config("luaver")
        if luaver then
            local version_str
            if luaver == "jit" then
                version_str = "jit"
            else
                local version = semver.new(luaver)
                version_str = version:major() .. version:minor()
            end
            table.insert(configs, "-DEMMY_LUA_VERSION=" .. version_str)
            if luaver == "jit" then
                -- LuaJIT has no LUA_NUMTAGS macro; emmy_debugger.h uses it for std::bitset size.
                table.insert(configs, "-DCMAKE_CXX_FLAGS=/DLUA_NUMTAGS=16")
                table.insert(configs, "-DCMAKE_C_FLAGS=/DLUA_NUMTAGS=16")
            end
        end
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
        io.replace("CMakeLists.txt", "set(CMAKE_INSTALL_PREFIX install)", "", {plain = true})
        if package:is_plat("windows") then
            if not (package:config("emmy_tool") or package:config("emmy_hook")) then
                io.replace("CMakeLists.txt", "add_subdirectory(shared)", "", {plain = true})
            end
            if not package:config("emmy_tool") then
                io.replace("CMakeLists.txt", "add_subdirectory(emmy_tool)", "", {plain = true})
            end
            if not package:config("emmy_hook") then
                io.replace("CMakeLists.txt", "add_subdirectory(emmy_hook)", "", {plain = true})
                io.replace("CMakeLists.txt", "add_subdirectory(third-party/EasyHook)", "", {plain = true})
            end
        end
        import("package.tools.cmake").install(package, configs)
    end)
 
    on_test(function (package)
        assert(os.isfile(os.getenv("EMMYLUA_DEBUGGER")))
    end)
package_end()

add_requires("emmylua_debugger", {
    configs = { luaver = "jit" }
})

target("SM-LuaDebugger")
    set_kind("shared")
    add_files("src/*.cpp")
    add_packages("emmylua_debugger", "minhook")
    add_syslinks("user32")
    set_symbols("debug")

    if is_mode("debug") then
        add_defines("_ITERATOR_DEBUG_LEVEL=0")
    end

    before_build(function (target)
        local pkg = target:pkg("emmylua_debugger")
        assert(pkg, "emmylua_debugger package not found")
        local dll = path.join(pkg:installdir(), "bin", "emmy_core.dll")
        assert(os.isfile(dll), "emmy_core.dll not found at " .. dll)
        local out = path.join(os.projectdir(), "src", "emmy.dll.h")

        -- Skip if up-to-date
        if os.isfile(out) and os.mtime(out) >= os.mtime(dll) then
            return
        end

        local data = io.readfile(dll, {encoding = "binary"})
        local lines = {
            "// Auto-generated by xmake. Do not edit.",
            "#pragma once",
            "#include <cstddef>",
            "#include <cstdint>",
            "",
            "inline constexpr std::uint8_t emmy_core_dll[] = {",
        }
        local buf = {}
        for i = 1, #data do
            buf[#buf + 1] = string.format("0x%02x,", data:byte(i))
            if i % 16 == 0 then
                lines[#lines + 1] = "    " .. table.concat(buf, "")
                buf = {}
            end
        end
        if #buf > 0 then
            lines[#lines + 1] = "    " .. table.concat(buf, "")
        end
        lines[#lines + 1] = "};"
        lines[#lines + 1] = string.format("inline constexpr std::size_t emmy_core_dll_size = %d;", #data)
        lines[#lines + 1] = ""
        io.writefile(out, table.concat(lines, "\n"))
        cprint("${cyan}[gen]${clear} src/emmy.dll.h (%d bytes)", #data)
    end)