# SM-LuaDebugger

A Lua debugger for Scrap Mechanic. Hooks the game's Lua VM, embeds [EmmyLuaDebugger](https://github.com/EmmyLua/EmmyLuaDebugger), and exposes it to the in-game environment so you can attach a debugger from your editor and step through Scrap Mechanic's Lua scripts (and your own mod code).

## Building

You'll need [xmake](https://xmake.io/).

```sh
git clone https://github.com/BenMcAvoy/SM-LuaDebugger.git
cd SM-LuaDebugger
xmake f -m release
xmake
```

The output DLL ends up under `build/`.

## Usage

You need to inject the DLL into `ScrapMechanic.exe` **before a world is loaded** — ideally right at game startup, before you hit the main menu. Once a world is up, the Lua VM is already initialized and the hook won't take.

How you inject it is up to you. Anything that does a `CreateRemoteThread` + `LoadLibrary` into the game process is fine.

Once injected, the debugger listens on `127.0.0.1:9966`. A message box pops up to confirm it's ready — at that point, attach from your editor.

### What gets exposed to Lua

Scrap Mechanic ships with a sandboxed Lua environment — the standard `debug` library isn't normally reachable from mod code. SM-LuaDebugger fixes that:

- The `debug` table is made available inside `unsafe_env` (so `unsafe_env.debug` works from any context that already has access to `unsafe_env`).
- The Emmy debugger API is installed onto the `debug` table as `debug.emmy`, which is the entry point you'll use for anything beyond just attaching.

In practice the listener is already started for you, so you usually don't need to touch `debug.emmy` directly — just attach from your editor. But if you want to drive it manually, the relevant calls are:

```lua
-- already done automatically on startup, but for reference:
debug.emmy.tcpListen("127.0.0.1", 9966)

-- force a breakpoint from code:
debug.emmy.breakHere()
```

See the [EmmyLuaDebugger docs](https://github.com/EmmyLua/EmmyLuaDebugger) for the full API.

### VSCode / EmmyLua setup

Install the **EmmyLua** extension, then create a debug configuration that attaches to `127.0.0.1:9966`.

You'll also want an `.emmyrc.json` at the root of whatever Lua project / mod folder you're working in, so the analyzer knows it's Lua 5.1 and can find your library stubs:

```json
{
  "workspace": {
    "$schema": "https://raw.githubusercontent.com/EmmyLuaLs/emmylua-analyzer-rust/refs/heads/main/crates/emmylua_code_analysis/resources/schema.json",
    "runtime": {
      "version": "Lua5.1"
    },
    "library": ["C:/Users/Ben/LuaLibs/sm.lua"]
  }
}
```

Swap the `library` path for wherever you keep your Scrap Mechanic Lua stubs.

## How it works (briefly)

- Hooks `LuaVM::Initialize` via MinHook to grab the `lua_State*` the moment a new VM is created.
- Drops `emmy_core.dll` (embedded in the binary) to a temp file with `FILE_FLAG_DELETE_ON_CLOSE`, `LoadLibrary`s it, and immediately closes the handle so the file is unlinked from disk — the loaded image stays mapped.
- Calls `luaopen_emmy_core` against the game's Lua state, starts the TCP listener, and exposes `emmy` on `debug.emmy` (and `unsafe_env.debug.emmy` if that table exists).
- Also hooks `BuildChunkName` so chunk names come through as full paths instead of Lua's truncated `"..."` form, which otherwise confuses EmmyLua's source resolution.

## Acknowledgements

- [EmmyLuaDebugger](https://github.com/EmmyLua/EmmyLuaDebugger) — the actual debugger engine doing all the heavy lifting. This project is essentially glue to get it talking to Scrap Mechanic's Lua VM.
- [EmmyLua analyzer / VSCode extension](https://github.com/EmmyLuaLs) — for the editor-side experience.
- [MinHook](https://github.com/TsudaKageyu/minhook) — function hooking.
- [xmake](https://xmake.io/) — build system.

## Disclaimer

This is a tool for poking at Scrap Mechanic's Lua scripts for modding and debugging purposes. Don't use it on multiplayer servers you don't own, don't be a jerk about it, etc.
