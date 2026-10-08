# Reverse Engineering Notes: Killer Instinct (PC)

Written 2026-10-07 from public sources. **Superseded on 2026-10-08 by `local_inspection_2026-10-08.md`**, which
verified the binary and assets directly. Keep this file for the public-source trail; where the two disagree, the
local inspection wins (notably: localization is a binary hash table, not XML/CSV; Lua scripts exist but are encrypted;
the UI is confirmed Scaleform GFx 4.3 with AS3).

## Binary and platform
- Executable: `KillerInstinctX64_r.exe` (from the 2024 Cheat Engine thread). VERIFY on AE.
- Win32, x64. Steam build since 2017; the Microsoft Store build is UWP and far harder to inject into. Target Steam only.
- Graphics: DirectX 11 on PC (store page minimum). Xbox AE moved to DX12; PC DX12 status unknown. VERIFY which `d3d11.dll`/`d3d12.dll`/`dxgi.dll` the exe imports.
- Runs on Steam Deck under Proton (needed Proton Experimental / GE at launch). A game that works under Proton almost certainly has no kernel-mode anti-cheat.
- Known trait: process sometimes stays alive in Task Manager after closing (Win10). Matters for a companion app watching the process.

## Anti-cheat / integrity
- No search result, forum thread, or trainer page mentions Easy Anti-Cheat, BattlEye, or any anti-cheat for the Steam build. SteamDB tech page was not fetchable (403); VERIFY there manually.
- Trainers and CE tables have been published for years with no ban reports found.
- Online play goes through PlayFab (since 2023 migration) with Xbox Live sign-in. Server-side validation of match results is plausible; client-side file integrity checks are not documented.
- The official forum's historical "K.I. Probation" system was a disconnect-penalty system, not anti-cheat.

## Engine and middleware
- Engine: Double Helix in-house engine (2013), maintained by Iron Galaxy since 2014. Not Unreal, not Unity. No script extender, no Lua layer, no public SDK.
- UI: Scaleform GFx (reported by the "List of games using Scaleform" wiki and the MobyGames credits search summary). VERIFY by looking for `.gfx`/`.swf` inside paks and `Scaleform`/`GFx` strings in the exe.
  - Scaleform renders Flash movies; menu labels are ActionScript TextField objects with real UTF-16/UTF-8 strings in memory, not pre-rasterized textures. That is the single most useful fact for narration.
  - Scaleform exposes `GFx::Movie::GetVariable`, `Invoke`, `SetExternalInterfaceHandler`; games call `Movie::Advance` every frame. Hooking is feasible but engine-specific.
  - JPEXS FFDec can decompile `.gfx` to recover ActionScript, TextField instance names, and layout, which tells us which variables to read.
- Audio: Audiokinetic Wwise (confirmed by the Game Developer postmortem). ~112 buses; HUD sounds are hard-panned P1 left / P2 right.
- Also credited: Havok Physics, Bink Video.

## Data formats
- `.pak` archives: container format, unpacker exists (thethiny). Compression not documented.
- `.EVF`: character/move/hitbox data (Hitbox Editor Tool). Not needed for narration but shows the data layer is decoded.
- Localization: unknown file format. VERIFY. Scaleform games usually ship string tables (XML/CSV/binary) referenced by ID from the SWF; if found, we can map IDs to display text without OCR.

## Memory layout knowledge (public)
- Known CE pointers: player health, shadow meter, instinct meter, AI health, match timer, camera position/angle, game speed.
- No published pointers for: menu state, selected menu item, cursor index, character select cursor, text box contents, screen/scene ID. We would have to find these.
- Addresses shift between builds; the 2024 table broke across versions. Use pattern (AOB) scans, not static offsets.

## Techniques that should apply
1. External process reading (ReadProcessMemory via pymem or C#). Zero injection, zero file changes. This is the CFC-Access model.
2. Proxy DLL (`dinput8.dll`, `dxgi.dll`, `version.dll`) to auto-load in-process code. CFC-Access and Voice of the Old Republic use `dinput8.dll`; Special K uses `dxgi.dll` on DX11. VERIFY which system DLLs the exe imports.
3. In-process hooks (MinHook/Detours) on Scaleform text-setting functions, found by scanning for the GFx vtable or by breakpointing on a known string's memory.
4. D3D11 `Present` hook to timestamp frames for OCR capture, if needed.
5. Windows OCR (Windows.Media.Ocr) on screen regions as a fallback; CFC-Access shows pre-processing (threshold, thicken) is needed for stylized fonts.

## Useful reference repos (not KI-specific)
- https://github.com/techiew/DirectX11Hook and https://github.com/guided-hacking/GH_D3D11_Hook - minimal D3D11 Present hooks.
- https://github.com/thethiny/Modding-Database - KI pak unpacker.
- JPEXS FFDec - `.gfx` decompiler.
- ReClass.NET, x64dbg (x64dbg gained initial screen-reader accessibility support in its April 2026 release, useful if a blind collaborator will do RE work).
