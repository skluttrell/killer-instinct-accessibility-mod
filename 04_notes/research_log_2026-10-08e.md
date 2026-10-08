# Research Log - 2026-10-08 (fifth session: narrator readers for the remaining offline screens)

## Done
1. Extracted and decompiled all 66 UI movies from INTERFACE.PAK with FFDec (scratchpad `as3/<Movie>/scripts`), and read
   the screen classes for PauseMenu, PracticePauseMenu, CommandList, ControllerConfig, StageSelect, Dojo, Trials, Popup,
   StoreMain, MatchOutcome, Toast, LoadingScreen, MultiplayerLobby. Display-object paths and focus mechanics are now in
   the `SCREENS` table of `06_narrator/agent.js`.
2. Agent: generic reader engine with index mode (public getter) and scan mode (focus by `currentLabel`), probe fields,
   per-screen `when` visibility gates, and deferred snapshots on the return of `GFx::MovieImpl::Advance`
   (RVA 0xe4a700 = vtable 0x203dd38 slot 24; found with `05_tools/frida_advance_probe.py`: 120 calls/s on the UI
   thread and it encloses the AS3 -> Lua dispatcher). Snapshots run immediately, +1, +30 and +180 frames.
3. Narrator: readers for Stage Select (`StageSelectionChanged` + `Lua_Populate.Stages`), Command List (`RefreshPage` +
   `EntrySelected`, notation tokens `<TT>DOWN</TT>` -> words), Controller config (JSON `Commands[i].Entries/Button`
   + scan), practice menu categories and toggles, Dojo, Trials, Toast, LoadingScreen; MainMenu state stack from the
   Populate JSON (its button text lags the state change); popup fallback that reads Title/Body from the screen when the
   JSON has raw newlines (`json.loads(strict=False)` now); signed `#crc` keys; Player 1 driving the Player 2 cursor.
4. Live test (Practice with all its categories, Command List, Controller config, Dojo, Trials, CPU vs CPU through Stage Select
   and Match results, Player vs CPU with the versus pause menu) all spoken correctly; hotkeys verified.

## Gotchas found today
- `Legend`, `GenericAssets`, `PauseBackground`, `Toast`, `LoadingScreen` scripts call Lua too; they must not become
  the "current screen" (agent `HELPERS` set), and `Popup.Close` restores the screen below.
- Shared components call their own Lua script: the practice menu's toggles call `OptionsMenu.SetVariables` while
  `OptionsMenu.swf` is not loaded. The agent falls back to the last readable screen; the narrator speaks the value from
  the event itself.
- A stepping toggle's `currentLabel` is `StepLeft`/`StepRight` (not a Focus label) until the next focus change.
- PauseMenu/Trials play the scroll sound before moving the cursor; ControllerConfig gains focus only after its build-on
  animation; both need the deferred snapshots.
- `GetString` (0x1a9640) is not a usable tick: re-setting unchanged legend text does not call it.
- `AS_SetSelections` wraps its payload in `selectionData`; the Combo Assist popups' bodies contain raw newlines.
- Escape = back in menus and pauses a match, but the in-match input layer only sees scan-code key events
  (`KEYEVENTF_SCANCODE`); `sendkeys.py` now sends those. A CPU vs CPU match cannot be paused at all.
- The user's keyboard binds are customised (LP=2, MP=3, HP=7, LK=J, MK=1, HK=E, ...); "Default" on the controller
  screen would overwrite them: always leave with Escape -> Quit (discard).

## Next
- Store / lobbies / Shadow Lords readers (need sign-in); FightArchive, Stats, Leaderboards.
- User test with NVDA; then Phase 2 (C++ DLL with the same RVAs: 0x5dfe50, 0x1122ef0, 0x1150a00, 0x11502e0, 0x350bd0,
  0xe4a700 behind AOB signatures).
