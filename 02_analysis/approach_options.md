# Candidate Approaches, Ranked

Scope: narrate menus, option values, character select, practice/training text, Shadow Lords text, online lobby/rank info, plus optional round/health callouts in match.

## Option 1: External memory reader + screen-state model (RECOMMENDED first phase)
How: A companion process (Python/pymem or C#) opens `KillerInstinctX64_r.exe` with read access, pattern-scans for the UI/menu structures, polls ~20x/sec, diffs "current screen + focused item + item label", and speaks changes through Tolk.
Where the text comes from: Scaleform keeps TextField strings in memory; once we locate the menu movie's object tree (or the game-side menu model that feeds it), labels are readable as plain UTF-16. Supplement with string tables extracted from the paks.
Pros: no injection, no file changes, no anti-cheat exposure, survives game restarts, same model as CFC-Access which shipped for a fighting-game collection.
Cons: finding stable pointers to Scaleform objects from outside is real RE work; heap-allocated trees need pointer chains or heuristics; addresses change per patch (mitigated by AOB scans and a near-frozen game).
Effort: medium-high RE, low engineering.

## Option 2: In-process proxy DLL hooking Scaleform (second phase or alternative)
How: Ship a `dinput8.dll`/`dxgi.dll` proxy that loads our DLL into the game. Hook the GFx functions that set TextField text or that handle focus/selection (`GFx::Value::SetText`, `Movie::Invoke`, the ExternalInterface callback). Every string the UI draws passes through us; emit it to Tolk.
Pros: catches *all* UI text including Shadow Lords dialogs, store, lobbies, without per-screen RE. Accurate focus tracking if the AS code fires selection events via ExternalInterface.
Cons: injection into a live online game; signature hunting in a stripped binary; must handle the game's threading; higher chance of crashes; any integrity check (none known) would catch it.
Effort: high RE, medium engineering.

## Option 3: D3D11 Present hook + Windows OCR
How: Capture the backbuffer each frame (or on a hotkey), OCR fixed regions, speak diffs.
Pros: engine-agnostic; works on day one for any screen; blind players already use OCR so the UX is familiar.
Cons: no focus awareness (cannot tell which item is highlighted without color heuristics), slow (100-300 ms), noisy on stylized fonts and animated backgrounds, CPU cost during matches.
Role: fallback layer and bootstrapping tool while Options 1/2 mature. Also useful to *label* screens during RE.

## Option 4: Asset mod - edit the Scaleform SWF/ActionScript to self-voice
How: Decompile menu `.gfx` with JPEXS, add ActionScript that, on focus change, writes the label somewhere observable (e.g. a known variable, or fscommand/ExternalInterface call we can hook), repack into the pak.
Pros: exact focus events straight from the UI logic; could even call `fscommand` with the text.
Cons: requires a working pak *repacker* (none documented), Scaleform's `.gfx` uses proprietary compression and FFDec export is not guaranteed to re-import; modified assets may fail online if any hash check exists; the official forum bans mod tutorials, so support channels are Discord only.
Role: possible later refinement; blocked until a repacker is confirmed.

## Option 5: Lobby Microsoft/Iron Galaxy for official narration
How: Use the 2019 forum request, the postmortem's own admission, Xbox Accessibility Guidelines, and Microsoft's Gaming Accessibility team as leverage.
Pros: only route that reaches Xbox and Microsoft Store users.
Cons: game is in maintenance mode after the 2024-04 bonus update; no feature patches since. Low probability, but costs nothing to run in parallel.

## Decision (revised 2026-10-08)
Local inspection showed the UI is Scaleform GFx 4.3 / AS3 with explicit per-screen focus state and a single engine boundary (`ExternalInterface` handler and `Movie::Invoke`) through which every navigation and population event passes. That makes Option 2 (in-process hook on the GFx boundary) the strongest product candidate: hook points are generic API calls, not per-screen reverse engineering, and the labels arrive as arguments.
Plan: Phase 0 C decides. Prototype with Option 1 (external read-only, `05_tools/memscan.py`) because it needs no installs; if the AS3 state is reachable and stable from outside, ship that for v0.1; otherwise build Option 2. Option 3 (OCR) stays as a fallback layer. Option 4 is unnecessary (no repacker needed; the string table and XML are readable without modifying assets). Option 5 in parallel.
