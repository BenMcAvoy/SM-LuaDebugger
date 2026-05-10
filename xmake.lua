add_rules("mode.debug", "mode.release")

set_languages("c++23")

set_runtimes("MD")
add_defines("_ITERATOR_DEBUG_LEVEL=0", "_HAS_ITERATOR_DEBUGGING=0")

includes("xmake/packages/emmylua_debugger.lua")
includes("xmake/rules/embed_emmy_dll.lua")

add_requires("minhook")
add_requires("emmylua_debugger")

target("SM-LuaDebugger")
    set_kind("shared")
    add_files("src/*.cpp")
    add_packages("emmylua_debugger", "minhook")
    add_syslinks("user32")
    set_symbols("debug")
    add_rules("embed_emmy_dll")