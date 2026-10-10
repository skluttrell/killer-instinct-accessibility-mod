# Killer Instinct: Anniversary Edition - Blind Accessibility Mod (developer notes)

The user-facing readme (GitHub front page and the pack-in ReadMe for releases) is `README.md`, written to the FFVII-Access readme template. This file holds the development status and folder map.

Goal: add screen-reader access (menu narration, character select, practice/training text, Shadow Lords text,
online lobbies) to Killer Instinct on Steam. The free base app (577940) and the Anniversary Edition DLC (2625580)
share one executable and one set of paks, so work on the free build covers both.

Status 2026-10-08 (sixth session): **Phase 2 DLL working.** `07_dll/` builds `kiaccess.dll` (C++, MinHook + Prism),
loaded by the game as a `dinput8.dll` proxy; it is a direct port of the Phase 1 narrator and was verified live on the
same screens (main menu states, Dojo, Character Select, Stage Select, practice pause menu, Command List, Controller
config, popups, match results). Phase 1 (`06_narrator/`, Python + Frida) stays as the development harness. Not yet
read: store, lobbies, Shadow Lords. See `07_dll/README.md` and `04_notes/research_log_2026-10-08f.md`.

Status 2026-10-10: **Shadow Lords readers working.** Live capture of the mode and decompile of its 13 movies done
(`04_notes/research_log_2026-10-09b.md`); the narrator bugs it exposed are fixed (popup repeated every frame, payloads
with `\'` escapes rejected, popup key names, attack placeholders, screen tracking without `LoadDestination`). Readers
verified live from a saved playthrough: hub, versus screen and its dialogue, War Room missions and encounter popups,
loadout popup, match rewards, Emporium, Barracks, Spirit Lair, tutorial prompts (see `07_dll/README.md`, "Shadow
Lords"). Still to do: pack reveal cards, purchase / craft popups, Barracks artifact list, Spirit Lair pet details,
Archives, leaderboard, cinematic subtitles (subtitle XML per movie + a Bink hook).

## Folder map

| Folder | What is in it |
|---|---|
| `01_research/` | Web findings, **`local_inspection_2026-10-08.md`** (binary/asset facts) and **`live_session_2026-10-08.md`** (hook points, live traffic) |
| `02_analysis/` | Approach options, pitfalls, and the **revised plan** |
| `03_resources/` | Link index and tool shopping list |
| `04_notes/` | Open questions, verification checklist (A/B ticked), daily research logs |
| `05_tools/` | Our scripts: PAK/strings/SWF tools, Capstone xref/disasm/RTTI/Lua-binding finders, Frida discovery hooks, key sender, screenshot, Prism ctypes wrapper |
| `06_narrator/` | Phase 1 narrator prototype (`narrator.py` + `agent.js`), see its README |
| `07_dll/` | **Phase 2: `kiaccess.dll`** (C++ port, MinHook + Prism, dinput8 proxy), see its README |
| `data/` | Locally derived game data (string table, pak tables, screen list, Shadow Lords XML, AS3 listings). Do not redistribute. |

## Key facts (verified)
- Win32 x64, DirectX 11, unsigned exe, no anti-cheat, imports `dinput8.dll` (easy proxy-load).
- UI = Scaleform GFx 4.3 + ActionScript 3; 60 screens listed in `ScreensDefinition.xml`; each screen = CFX movie + encrypted Lua script.
- All 16,472 English UI strings decoded; table hash = CRC32(lowercase key); 2,246 keys resolved.
- Verified live: AS3 -> Lua dispatcher at RVA 0x5dfe50, Lua -> AS3 calls at `AS3::MovieRoot::Invoke` 0x1122ef0 (`root.Invoke(swf, func, json)` on ForegroundShell.swf), localization resolver at 0x1a9640. Hooking these three gives screen, navigation, payload text and every displayed string.
- Focus: `root.GetSWFRefFromString(swf)` then `GetSelectionIndex()` via the GFx object interface (0x1150a00), on the UI thread; screens without a getter are scanned by button `currentLabel`. Per-frame tick for deferred reads: `GFx::MovieImpl::Advance` 0xe4a700 (vtable 0x203dd38 slot 24). See `01_research/live_session_2026-10-08.md` section 7 and `06_narrator/README.md`.
- Shadow Lords dialogue, briefings and dossiers are plain XML.
- In-match audio is already blind-playable (hard-panned cues, HUD volume slider); menus are the gap.

## Start here
1. `02_analysis/assessment_and_plan.md` - the recommendation and the next-step decision.
2. `01_research/local_inspection_2026-10-08.md` - everything we know about the binary and assets.
3. `04_notes/verification_checklist.md` - section C is the next work.
