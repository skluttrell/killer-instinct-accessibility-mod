# KI narrator v0.2 (Phase 1 prototype)

Python host + Frida agent + Prism speech. Speaks screen changes, the focused menu item with its position, toggle
values, popups (title, message, buttons), the character / stage / command-list / controller screens, toasts and the
loading screen. Read-only: the agent calls the game's own public ActionScript getters and reads display-object
properties through the engine's GFx entry points on the UI thread; nothing is patched.

## Run
1. Start the game through Steam and reach any menu (narrator can attach at any time).
2. `python narrator.py [--verbosity 0|1|2] [--events] [--no-speech]` (plain `python`, user-site frida; run only one
   Frida session on the game at a time).
3. Speech goes to the best Prism backend (NVDA here). Every utterance is logged to `data/narrator_speech.log`
   with the hook-to-speech latency in ms (`--events` also logs every engine event).

Hotkeys (global, RegisterHotKey, verified live 2026-10-08): Ctrl+Shift+R repeat, Ctrl+Shift+D description of the
focused item, Ctrl+Shift+T ticker / MOTD, Ctrl+Shift+V cycle verbosity (0 label only, 1 + position, 2 + description),
Ctrl+Shift+Q quit.

## How it works
- `agent.js` hooks the AS3 -> Lua dispatcher (0x5dfe50), `AS3::MovieRoot::Invoke` (0x1122ef0, every Lua -> AS3 call
  as `root.Invoke(swf, func, json)`) and the return of `GFx::MovieImpl::Advance` (0xe4a700, vtable 0x203dd38 slot 24,
  the per-frame tick of the UI thread).
- After a trigger (scroll/accept/back sounds, `ScreenShown`, `SetVariables`, `EntrySelected`, ... and the `Populate*`
  invokes) the agent takes a focus snapshot immediately, then again when the next frame's `Advance` returns, and once
  more 30 and 180 frames later (screens that move the cursor after the sound or animate in; frames are counted on
  one movie instance). The narrator de-duplicates.
- A snapshot resolves the screen with `root.GetSWFRefFromString(swf)` and then, per the `SCREENS` table:
  - index mode: the screen's public getter (`GetSelectionIndex`, `GetSelectedDisplayButton`, ...) and the text fields
    of item `{i}` (paths from the decompiled AS3);
  - scan mode (screens without a getter: pause menus, practice menu, trials, store, post-match, controller config):
    every button's `currentLabel`; the one in GainFocus/FocusedLoop/RemappingLoop/Step* is the cursor, counted among
    the visible, labelled buttons; hidden sub-menus (`when: <container>.visible`) are skipped;
  - `probe` fields: category / mode / difficulty / popup title and body, read straight from the screen.
- `narrator.py` resolves localization keys (`&literal`, `#decimal-crc32` incl. signed, CRC32(lowercase key) against
  `data/strings_en.tsv`), keeps state stacks for MainMenu and OptionsMenu from the Populate JSON (their button text
  tweens in late, so labels come from the JSON), tracks character-select stages per side (including Player 1 driving
  the Player 2 cursor), and speaks. Last-known `Populate` payloads are dumped to `data/populate_<swf>.json` and
  reloaded at start, so attaching after a screen loaded still works.

## Verified live (2026-10-08, sessions 4 and 5)
Landing page, main menu with all its states (Fight / Master / Multiplayer sub-menus, "n of m"), Options lists, states
and toggles, popups (title, body, buttons, also bodies with raw newlines), daily reward panel, Character Select
(fighter / costume / color for both sides, lock flags), Stage Select (name, locked / not installed, lock popup),
loading screen ("Loading. Jago versus Fulgore"), practice pause menu (all categories: Pause Menu, Dummy Options,
Practice Options with slider values, Theme), Command List (group, moves with notation "down down-forward forward plus
light punch", key moves, paging with Q/E), Controller config (rows with command and bound key, "Press a key",
assignment, toggles, Save/Default, quit confirmation), Dojo (mode, lesson number and title, locked), Trials
(difficulty, entries), versus pause menu (Resume ... Quit, 7 entries), match results (options; summary with winner and
win streak, metrics at verbosity 2), all five hotkeys. Latency hook -> speech call: 0-10 ms (scan screens up to ~10 ms).

## Known gaps / next
- Store (needs sign-in), lobbies, Shadow Lords (GA_*), Fight Archive, Stats, Leaderboards: no readers yet (fallback:
  screen name + JSON lists where present).
- Popup display-button lists (store style) are read through `GetSelectedDisplayButton` but untested.
- Inline icon tags in descriptions are converted to words; HTML entities are not yet decoded.
- Immediately after a screen change the first (immediate) snapshot can still show the previous cursor; the deferred
  snapshot corrects it within a frame, so the user may hear two items (e.g. "Command List" then "Resume").
