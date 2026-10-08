# Research Log - 2026-10-08 (third session: C2 focus index and C3 payloads)

User decisions this session: all outreach steps dropped (no Microsoft / Iron Galaxy request, no outside testers); start with the recommended next step, C2.

## Done
1. Static: `uiInvokeEvent` is passive data (vtable 0x17d0ac8 = type getters, dtor, clone); the queue lives at uiManager+0xbd0 and is drained by a job on the UI thread. New tools `vtable_calls.py` (vtable slot call summary with .pdata bounds) and `disp_scan.py` (who uses a struct offset).
2. Live backtraces (`frida_bt_probe.py`) from the dispatcher and the event destructor exposed the chain handler 0x998910 -> 0x1d6b30 -> 0x3c4fe0 -> thunk 0xe47040 -> `AS3::MovieRoot::Invoke` 0x1122ef0, which calls the host movie's `root.Invoke(swf, func, json)`. First guess (MovieImpl slot 57) was wrong; `frida_invoke_path.py` resolved the real vtable (MovieRoot 0x20a2928).
3. Host movie identified and decompiled: `ForegroundShell.swf` / `ShellBase` (`Invoke`, `GetSWFRefFromString`, `GetActiveSWF`, `LoadSWF`...). Listing saved to `data/as3_listings/ForegroundShell.txt`. SHELL.PAK was a red herring (level data; table in `data/SHELL.PAK.table.tsv`).
4. `frida_focus_invoke.py`: two-step in-process query (screen ref via the shell, then `GetSelectionIndex` through the GFx object interface slot 6 = 0x1150a00). Verified on MainMenu (indices agree with `CheckIsDestInstalled`), OptionsMenu (2 -> 3 -> 2 on down/down/up) and LandingPage. Under 1 ms. Logs: `data/frida_focus_invoke3.log` (MainMenu), `frida_focus_invoke5.log` (Options, transitions, Populate JSON).
5. C3 closed as a by-product: every Lua -> AS3 payload, including the table variant, is visible as JSON at 0x1122ef0.
6. Game launched via Steam, driven with `sendkeys.py`, closed at the end.

## Lessons
- Key presses need about 200 ms hold; 60 ms presses were dropped by the Combo Assist popups. `sendkeys.py` default changed.
- Frida 17: when a second session on the same process detaches, the first session's hooks go silent. One probe at a time.
- The game's own UI contract is the best API: public AS3 getters (`GetSelectionIndex`, `GetActiveSWF`, `GetFocusedFighterIndex`) called through the engine's entry points, on the UI thread, no patching.
- Re-entrant `MovieRoot::Invoke` from inside the ExternalInterface callback is fine in practice (10+ minutes, many screens).

## Next
- Phase 1 prototype: merge the three hooks + focus query with `prism_ctypes.py`; build the label map from `Populate` JSON (resolve `key`/`descKey` via `data/strings_en_by_key.tsv`); speak "label, index of count" on every `PlayScroll`; popups, transitions, toggles (`textValues[value]`).
- AOB signatures for 0x5dfe50, 0x1122ef0, 0x1a9640, 0x1150a00, 0x350bd0.
