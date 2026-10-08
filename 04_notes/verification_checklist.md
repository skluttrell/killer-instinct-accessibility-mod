# Verification Checklist

Keep everything read-only. Extracted files go in the session scratchpad or `data/`, never back into the game folder.

## A. Binary facts (done 2026-10-08, see 01_research/local_inspection_2026-10-08.md)
- [x] Exe name, size, SHA-256, version: `KILLERINSTINCTX64_R.EXE`, 43,511,576 B, `33bbd291...edf6`, v1.0.0.0, PE date 2024-05-08, unsigned.
- [x] Imports: dinput8 (DirectInput8Create), xinput9_1_0, d3d11, dxgi, winhttp, steam_api64, PlayFabMultiplayerWin, bink2w64. No version.dll.
- [x] Anti-cheat: none in folder, imports, or strings. "AntiCheat*" strings are disconnect-penalty config.
- [x] Middleware strings: Scaleform 4.3 / AS3, Wwise, Havok 2014, Bink 2, PlayFab, Lua.
- [x] Frida attached and detached repeatedly with no anti-debug reaction.

## B. Assets (done 2026-10-08)
- [x] PAK format decoded; own parser works on all tested paks.
- [x] Type codes mapped (21 XML, 26 strings, 58 Lua enc, 63 CFX, 64 DDS, 9 CSV, EIFF binaries).
- [x] Frontend movies inflated; AS3 symbol/method/instance names listed (`data/as3_listings/`).
- [x] Localization table decoded (`data/strings_en.tsv`, 16,472 strings).
- [x] ScreensDefinition.xml and Shadow Lords XML extracted (`data/xml/`).
- [x] Full AS3 decompile of MainMenu/OptionsMenu/CharacterSelect with JPEXS FFDec (CLI).
- [ ] Identify the Lua cipher (optional).

## C. Live (done 2026-10-08 with Frida, see 01_research/live_session_2026-10-08.md)
- [x] Launched the game, reached the main menu, drove it with `sendkeys.py`, screenshots for orientation.
- [x] ExternalInterface dispatcher located (0x5dfe50) and decoded: (script, function, JSON) on every AS3 -> Lua call.
- [x] Lua -> AS3 invoke queue located (0x5d9680 / 0x5d97d0): (swf, function, JSON) with display text.
- [x] Localization resolver located (0x1a9640): every resolved string in on-screen order.
- [x] MainMenu focus observable via `CheckIsDestInstalled(dest)`.
- [x] C2: generic focus index. `GetSelectionIndex()` called in-process via `root.GetSWFRefFromString` + `ObjectInterface::Invoke` (live-session notes section 7); verified on MainMenu, OptionsMenu, LandingPage, under 1 ms per query.
- [x] C3: Lua-table invoke payloads arrive as JSON at `MovieRoot::Invoke` 0x1122ef0 (`MainMenu.Populate`, `OptionsMenu.Populate` captured in `data/frida_focus_invoke5.log`).
- [x] C4: per-frame UI-thread tick found: `MovieImpl::Advance` 0xe4a700 (vtable 0x203dd38 slot 24), 120 calls/s, encloses the dispatcher (`05_tools/frida_advance_probe.py`).
- [x] AOB signatures for the hooks (0x5dfe50, 0x1122ef0, 0xe4a700, 0x1150a00, 0x11502e0, 0x350bd0): `07_dll/sigs.json` / `src/sigs.h`, all unique in .text (`05_tools/aob_sigs.py`).

## D. Speech smoke test
- [x] `prism_ctypes.py` loads prism.dll and selects NVDA. [x] Wired to the Frida events (`06_narrator/`).
- [x] Hook-to-speech-call latency: average 1.6 ms, max 4 ms over 58 utterances (`data/narrator_speech.log`); scan-mode screens (pause/practice/controller) 4-10 ms. Target under 100 ms met; NVDA's own output delay not included.
- [x] Readers verified live for 12 screens (session 5, see `04_notes/research_log_2026-10-08e.md`).

## E. Online safety
- [x] In-process DLL reads only (MinHook trampolines on three functions are the only code changes); `05_tools/inject.py` is a development tool, not part of the mod.
- [ ] Document behavior in online modes.
