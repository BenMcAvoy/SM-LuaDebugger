package("emmylua_debugger")
    set_kind("library")
    set_homepage("https://github.com/EmmyLua/EmmyLuaDebugger")
    set_description("EmmyLua Debugger")

    add_urls("https://github.com/EmmyLua/EmmyLuaDebugger/archive/refs/tags/$(version).tar.gz",
             "https://github.com/EmmyLua/EmmyLuaDebugger.git")
    add_versions("1.9.1", "3e0df942c4bc42cd2414f008a0fdf10a5d5c863758f891d916966dd5da107a1d")

    -- LuaJIT v2.1 has no proper releases; pin to a known-good commit.
    add_resources("1.9.1", "luajit",
        "https://github.com/LuaJIT/LuaJIT/archive/18b087cd2cd4ddc4a79782bf155383a689d5093d.tar.gz",
        "88a592afa9907d6b0c6e1e7ac9b39982622e3ca086f0646d4ea89b0e4e81f093")

    add_deps("cmake")

    on_load(function (package)
        package:addenv("EMMYLUA_DEBUGGER", "bin/emmy_core.dll")
        package:mark_as_pathenv("EMMYLUA_DEBUGGER")
    end)

    on_install("windows", function (package)
        -- Tarball releases ship empty submodule dirs; copy LuaJIT from our pinned resource.
        if not os.isfile("third-party/luajit/src/lua.h") then
            os.tryrm("third-party/luajit")
            os.cp(path.join(package:resourcedir("luajit"), "*"), "third-party/luajit")
        end

        -- LuaJIT must be built with its own msvcbuild.bat to produce lua51.lib that emmy_debugger links.
        if not os.isfile("third-party/luajit/src/lua51.lib") then
            local msvc = package:toolchain("msvc") or import("core.tool.toolchain").load("msvc", {plat = "windows", arch = "x64"})
            os.vrunv("cmd.exe", {"/c", "msvcbuild.bat"}, {curdir = "third-party/luajit/src", envs = msvc:runenvs()})
            assert(os.isfile("third-party/luajit/src/lua51.lib"), "LuaJIT msvcbuild.bat did not produce lua51.lib")
        end

        -- Strip optional subprojects we don't need.
        io.replace("CMakeLists.txt", "set(CMAKE_INSTALL_PREFIX install)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(shared)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(emmy_tool)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(emmy_hook)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(third-party/EasyHook)", "", {plain = true})

        local configs = {
            "-DEMMY_USE_LUA_SOURCE=ON",
            "-DEMMY_LUA_VERSION=jit",
            "-DCMAKE_CXX_FLAGS=/DLUA_NUMTAGS=16",
            "-DCMAKE_C_FLAGS=/DLUA_NUMTAGS=16",
            "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"),
        }

        import("package.tools.cmake").install(package, configs)
    end)

    on_test(function (package)
        assert(os.isfile(os.getenv("EMMYLUA_DEBUGGER")))
    end)
package_end()
