# Open Questions

Status as of 2026-10-08. Answered items keep their answer for the record.

1. Does the AE Steam build still use Scaleform GFx? **Answered: yes.** GFx 4.3, AS3, CLIK. Movies in INTERFACE.PAK as `CFX`.
2. Exact executable name, launcher? **Answered:** `KILLERINSTINCTX64_R.EXE`, launched directly by Steam (no launcher in the folder).
3. Which system DLLs does the exe import? **Answered:** dinput8, xinput9_1_0, d3d11, dxgi, winhttp, d3dcompiler_47 (plus CRT, steam, PlayFab, Bink). `dinput8.dll` proxy is the natural loader.
4. Any anti-cheat, anti-debug, or integrity check? **Answered: none found** statically. Runtime anti-debug still untested.
5. Where are localized UI strings, in what format? **Answered:** `gameinfo\strings\strings[_lang]` in GLOBAL.PAK, binary hash->UTF-8 table, decoded.
6. Does the game-side C++ keep a menu model, or is menu state purely inside ActionScript? **Partly answered:** AS3 owns `SelectionIndex`/`MenuEntries`; Lua (engine side) drives population via `Lua_*` invokes and receives `Navigate/Select` via ExternalInterface. So both sides see every change; the chokepoint is the GFx boundary.
7. Does KIUnpacker work on AE paks? Repacker? **Superseded:** our own parser works; no repacker written (asset mods are not on the critical path).
8. Shadow Lords text location? **Answered:** plain XML (`gameinfo\AssaultMode\*`), extracted to `data/xml/`.
9. Store (UWP) build layout? Still unknown; unsupported.
10. Will Microsoft/Iron Galaxy entertain an official narration request? Still open.
11. Is SightlessKombat willing to advise or test? Still open.
12. Xbox Live sign-in UI: separate window or Scaleform? Still open; observe at first launch.

New since 2026-10-08:
13. What cipher protects the type-58 Lua scripts, and where is the key in the exe? (Optional.)
14. Can the retail build's debug "Console" / "LUA Log Output" windows be enabled from `windows_pc.cfg` / `default_pc.cfg` (`minwarn`, demoMode hints)? If yes, UI traffic may be observable without hooks.
15. What is the hash function for string-table keys? (Only matters if we want key names; display text is enough for narration.)
16. Does `ExternalInterface.call` go through a single engine callback we can hook, and what is its signature in this build? (Phase 0 C.)
