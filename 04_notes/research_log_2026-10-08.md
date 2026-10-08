# Research Log - 2026-10-08

User installed the free Steam build (app 577940). Rule still in force: no third-party tools installed.
All inspection used Python 3.10 (already present) with `python -I`, read-only.

## Done today (Phase 0 A + B)
1. Located install, build id, size, exe hash/version/signature, imports, sections. No anti-cheat.
2. Confirmed Scaleform GFx 4.3 + AS3, Wwise, Havok, Bink, PlayFab, Lua from exe strings and config files.
3. Decoded the `PAK_` v4 container (262-byte records, uncompressed) and parsed GLOBAL.PAK and INTERFACE.PAK tables.
4. Mapped type codes: 21 XML, 26 string table, 58 encrypted Lua, 63 CFX movie, 64 DDS, 9 CSV.
5. Decoded the localization table format and dumped all 16,472 English strings.
6. Extracted and inflated the MainMenu / OptionsMenu / CharacterSelect / Popup / SharedLibrary movies; listed their AS3 classes, methods and instance names.
7. Extracted ScreensDefinition.xml (60 screens) and the Shadow Lords XML text files.
8. Established that Lua screen scripts are encrypted (entropy 8.0, 16-byte aligned, deterministic).
9. Saved scripts to `05_tools/`, derived data to `data/`, findings to `01_research/local_inspection_2026-10-08.md`, revised plan in `02_analysis/assessment_and_plan.md`.
10. Wrote `05_tools/memscan.py` (read-only live memory search) for Phase 0 C; not yet run.

## Corrections to the 2026-10-07 notes
- Localization is a binary hash table, not XML/CSV (the `.csv` is only for cinematic dialog).
- The unpacker on GitHub is unnecessary; our own parser handles the current paks and the table layout is documented.
- "Meld" XAML files are a legacy news overlay, not the game UI.

## Open for next session
- Phase 0 C: run the game to the main menu; use `memscan.py find "Multiplayer" "mcMenuBtn0"` and `watch` to find the cursor index.
- Decide whether to approve Cheat Engine / x64dbg / System Informer / JPEXS.
- Optional: identify the Lua cipher (candidates: AES-128/TEA/XTEA; key in exe) so screen scripts become readable.
- Optional: enable the retail "LUA Log Output"/console windows via config to see `CallUILuaScript` traffic without hooking.
