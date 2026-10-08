# Assessment and Recommended Plan

Revised 2026-10-08 (evening) after the live Frida session. Earlier revisions are summarized in `04_notes/research_log_*.md`.

## Verdict
Feasible with low risk. Everything a narrator needs flows through three engine functions that are cheap to hook and verified live at 60 FPS:
1. the AS3 -> Lua dispatcher 0x5dfe50 (screen name, function, JSON) for navigation and screen transitions,
2. `AS3::MovieRoot::Invoke` 0x1122ef0, through which every Lua -> AS3 call passes as `root.Invoke(swf, func, JSON)` (including the Lua-table payloads, already serialized),
3. the localization resolver 0x1a9640 (every display string as it is resolved, in on-screen order).
The focus question (C2) is solved: on the UI thread we call the host shell's `GetSWFRefFromString(swf)` and then the screen's public `GetSelectionIndex()` through the GFx object interface; it works on MainMenu, OptionsMenu and LandingPage, costs under 1 ms, and needs no patching. Details in `01_research/live_session_2026-10-08.md` section 7.

Speech output: **Prism** (user decision), C ABI from the mod DLL; `prism.dll` already verified to pick NVDA on this machine.

## Architecture (target)
```
+--------------------------------------------+      +-------------------------------------------+
| KILLERINSTINCTX64_R.EXE                    |      | kiaccess (in-process DLL, C++)            |
|  dinput8.dll proxy -> kiaccess.dll         |      |  MinHook on:                              |
|  Scaleform GFx 4.3 (AS3 screens)           |      |   0x5dfe50 ExternalInterface dispatcher   |
|  Lua screen scripts (encrypted, untouched) |----->|   0x1122ef0 MovieRoot::Invoke (root.Invoke)|
|  LocalizationManager                       |      |   0x1a9640 GetString(hash)                |
|  ForegroundShell.swf (host, ShellBase)     |      |  screen model from Populate JSON + AS3    |
|                                            |      |  focus: root.GetSWFRefFromString, then    |
|                                            |      |   ObjectInterface::Invoke GetSelectionIndex|
|                                            |      |  speech thread -> prism.dll -> NVDA/JAWS/ |
|                                            |      |          SAPI; OCR fallback (Windows OCR) |
+--------------------------------------------+      +-------------------------------------------+
```
Prototype first in Python + Frida (`05_tools/frida_focus_invoke.py` already has the three hooks and the focus query) with `prism_ctypes.py` for speech; port to a C++ DLL once the behaviour is right.

## Phases
### Phase 0: Reconnaissance - DONE (2026-10-08)
- [x] A binary facts, [x] B assets and AS3 decompile, [x] C live hooks on dispatcher / invoke queue / localization; keyboard driving; screenshots.
- [x] C2 focus index: `GetSelectionIndex()` read in-process through the host shell (MainMenu, OptionsMenu, LandingPage verified).
- [x] C3 Lua-table payloads: they reach AS3 as JSON through `root.Invoke`; hook 0x1122ef0.

### Phase 1: Python prototype (v0.1, Frida + Prism) - RUNNING since 2026-10-08 (`06_narrator/`)
- [x] Screen changes, popups (title/message/buttons), focused item read from the screen's own text fields at `GetSelectionIndex()`, position "n of m", Options states and toggle values, daily loot panel, MOTD. Latency about 2 ms.
- [x] Hotkeys (repeat, description, ticker, verbosity) and speech log implemented and exercised live (2026-10-08).
- [x] Readers for Character Select, Stage Select, practice pause menu, Command List, Controller config, Dojo, Trials, toasts, loading screen (verified live 2026-10-08); [ ] versus pause menu and match results (configured, not yet seen live); [ ] Store, lobbies, Shadow Lords.
- [x] Per-frame UI-thread tick for deferred reads: `MovieImpl::Advance` 0xe4a700 (vtable 0x203dd38 slot 24).
- [ ] Test with the user (NVDA) on this machine; tune verbosity and wording.

### Phase 2: C++ DLL + Prism, proxy-loaded via dinput8.dll - WORKING (2026-10-08, `07_dll/`)
- [x] MinHook detours, GFx::Value API, SCREENS table and narrator ported; exe hash gate + AOB signatures; Prism speech thread; hotkeys; verified live by injection.
- [x] Installed as the dinput8.dll proxy in the game folder and verified from a cold Steam start (2026-10-08 16:19).
- [ ] Packaging / first-run generation of `strings_en.tsv` from the PAK (currently shipped from `data/`).
- Same hooks with MinHook (dispatcher 0x5dfe50, MovieRoot::Invoke 0x1122ef0, MovieImpl::Advance 0xe4a700) plus the GFx::Value API entry points (0x1150a00, 0x11502e0, 0x350bd0); AOB signatures + exe hash gate; no Frida dependency for users.

### Phase 3: Depth
- CommandList, Dojo prompts, Trials, online lobbies/rank, Shadow Lords (`data/xml/AssaultMode_*.xml`), store.

Dropped 2026-10-08 (user decision): no outreach to Microsoft / Iron Galaxy and no recruiting of outside testers or reviewers. This is a self-contained mod effort.

## Resources
Installed today (approved): Frida + frida-tools, Capstone (already present), JPEXS FFDec 26.3, Temurin JRE 21, prismatoid wheel (for `prism.dll`). Not installed: Cheat Engine (GUI-only, not usable by either of us), x64dbg, System Informer (not needed; RTTI/xref scripts replaced them).
Still needed later: a C++ toolchain (MSVC Build Tools or clang) for the DLL, MinHook, Prism via vcpkg.

## Go / no-go
1. C2 unsolved after ~2 weeks -> ship v0.1 with MainMenu/popups/transitions narration plus OCR for options, and keep working on focus.
2. Any online-play rejection of the proxy DLL -> external read-only mode (ReadProcessMemory polling of the same data).
