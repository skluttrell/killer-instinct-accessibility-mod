# Research Log - 2026-10-08 (fourth session: Phase 1 narrator prototype)

User decision: begin Phase 1.

## Done
1. `06_narrator/agent.js` + `narrator.py`: dispatcher + `MovieRoot::Invoke` hooks, focus snapshots that read the focused
   button's text fields by display-object path (paths from the decompiled MainMenu/OptionsMenu AS3), Prism speech,
   localization resolver (`&literal`, `#decimal-hash`, CRC32 of lowercase key), Options state stack, global hotkeys, speech log.
2. Live test through landing page -> main menu -> Options -> Graphics toggles and back. Everything spoken correctly;
   latency hook -> speech call 1.6 ms average, 4 ms max.
3. Bugs found and fixed during the test: a rotating pool of GFx::Value scratch slots clobbered the live screen reference
   after 16 allocations (now one fresh Value per use); the toggle group was chosen by a state model that resets on restart
   (now decided by the list button's `currentLabel`: Unfocused while a toggle state is live); popup keys arrive as
   `#<decimal crc32>`; MOTD re-sent every 30 s (now spoken only when changed); "missing text" placeholder in text fields
   right after a screen/state shows (falls back to the Populate JSON entry).
4. Observed: the game asks "Keep Changes / Discard Changes" after any graphics toggle change; the narrator read the popup
   with both buttons. Escape discards.

## Lessons
- `ObjectInterface::GetMember` resolves display-list children by instance name and plain properties (`text`, `visible`,
  `currentLabel`, `currentFrame`) but not private AS3 vars (`ToggleMode`, `MenuMin`).
- Sound events fire before the text changes (`PlayScrollLeftRight` then `SetVariables`); speak on the latter.
- The engine's `Populate` JSON plus on-screen text together cover labels, descriptions, values and counts.

## Next
- Readers for CharacterSelect, Store, lobbies, pause menus; popup button focus; live hotkey test; user test with NVDA.
- Then Phase 2: C++ DLL with the same five RVAs (0x5dfe50, 0x1122ef0, 0x1150a00, 0x11502e0, 0x350bd0) behind AOB signatures.
