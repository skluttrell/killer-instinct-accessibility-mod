# Research Log - 2026-10-09 (seventh session: retro texts, first-run string table, start screen)

Resumed from the 2026-10-08 "Next" list; the user was not present, so everything was verified from the terminal.

## Done
1. **Retro costume texts** for the eleven fighters that had none (`07_dll/data/fighter_appearance.json`): Shadow Jago, Aganos,
   ARIA, Kilgore, Arbiter (the string table itself calls it the "Halo 2 Arbiter retro costume"), Rash, Shin Hisako, Mira,
   General RAAM, Eagle. The game's own strings (`Retro <name>` / `Purchase Retro <name>`) show that every fighter except
   Omen has a retro costume, so Omen's `retro` entry now says exactly that. The texts are written from memory of the game,
   like the others, and still need a sighted check. Installed into the game folder (data only, the DLL reads it at start).
2. **First-run generation of `strings_en.tsv`** (`07_dll/src/pak.h`, `pak.cpp`, wired in `dllmain.cpp`): a C++ port of
   `pak_table.py` + `strings_decode.py`. The DLL looks for `PAK\DX11\GLOBAL.PAK` next to the game exe (the *process*
   directory, `exeDir()`, so it also works when injected from `build\`), scans for the record `gameinfo\strings\strings`
   (NUL-terminated name inside the 240-byte field, so `strings_ita` cannot match; the record table is in the first ~7 MB
   of the 1.7 GB file, found in 10-15 ms; a full scan for a missing record takes ~600 ms), decodes the type-26 table and
   writes the same TSV `strings_decode.py` produced (byte-identical apart from LF vs CRLF). It runs when the TSV is missing
   or older than the PAK (game update). `build.ps1` no longer copies `data/strings_en.tsv`, so the build output contains
   none of the game's text. Verified by an offline console harness against the real PAK and by three cold starts
   (log: `generated data\strings_en.tsv from PAK\DX11\GLOBAL.PAK: 16472 strings in 10 ms`, then `strings: 16456`;
   second start: no regeneration).
3. **Start screen announced on cold start.** With `log_events=1` the first screen arrives as
   `ei StartScreen.ScreenShown` + `inv StartScreen.swf.Populate {"Season":"COMMON_SEASON_3","PressStartText":"START_SCREEN_PRESS_START"}`
   with no `uiScreenUtil.LoadDestination`, so the narrator said nothing between "narrator ready" and the landing page.
   `Populate` of `StartScreen.swf` now sets the current screen and says "Start screen. Press Menu or Space" (the on-screen
   text resolves to `PRESS MENU / <KEY1>COMMAND_UI_START</KEY1>`, Space on the keyboard). Verified live: announced 14 s
   after launch.

## How to exit the game from the terminal
Landing page -> Down x4 to "Exit, 5 of 5" -> Enter -> popup "Confirm Exit. Are you sure you want to exit Killer Instinct?
Buttons: Enter: Yes, Escape: No" -> Enter. The log then shows `=== game exiting ===`. Wait ~10 s after the landing page
appears before sending keys: the sync popup and the "Free Rotating Fighter" toast swallow the first presses (one lost
Down sent the session into the Store once; Escape twice returns to the landing page with the focus kept). Escape on the
landing page itself does nothing.

## Gotchas
- The Bash tool in this environment collapses every `\\` to `\` in the command text, even inside quoted heredocs and
  single quotes (not only through Python as noted yesterday). C++ string escapes (`"\\t"`, `L"PAK\\DX11"`) and sed
  patterns written that way arrive broken. Write source files containing backslash escapes with the Write/Edit tools,
  or compile and check `grep -n '\\\\'`.
- `MSVC` warning C4129 ("unrecognized character escape sequence") is the symptom of the collapse: a `\s` in a string.

## Next
- Packaging: release zip with `dinput8.dll`, `kiaccess\kiaccess.ini`, `kiaccess\prism.dll`, `kiaccess\data\fighter_names.json`,
  `kiaccess\data\fighter_appearance.json` and an install note (no game data inside).
- Store / lobbies / Shadow Lords readers (the Store narrated "Store, bundles, item 1" today: the item labels still need a reader).
- User test with NVDA; sighted review of the appearance texts.

## Addendum: opponent radar (user request, same day)
User asked for a radar pulse: pans to the opponent's side (relative to the player), faster when closer, higher when the
opponent is airborne, tone synthesized like the FFVII-Access `tones.cpp` (waveOut, per-call 16-bit sine buffer with
fades). Decisions: pan = opponent relative to Player 1; Player 1 vs CPU only (no second radar for Player 2).
- Data: the Lua bindings `Opponent_GetPosX/Y/Z` (0x6e3ac0/0x6e3b30/0x6e3bb0) read `match = *[0x27de140]` (getter
  0x9d7240, valid when `[match+0x48] & 0x8000`), slot `i*0xd0`, 3x4 transform at +0x3e0, translation X +0x3ec, Y +0x3fc,
  Z +0x40c. `GetRoundElapsedTime` (0x6ef910) reads `round = *[0x27d85f0]` (getter 0x74d520, same flag): elapsed =
  `[+0x370]-[+0x368]` frames or the float `[+0x938]` when `[+0x374]` is set (practice). Both getters got AOB signatures
  (`matchState`, `roundState`); the DLL decodes the RIP-relative global from the getter's first instruction.
- Measured in Practice (Jago vs Fulgore): round start X +1.5 (P1, left of screen) / -1.5 (P2, right); walking back (A)
  raises X to 4.56 at the corner, so screen-right is -X; Y is up (dummy set to Jump peaks at 1.96); Z = 1.0 lane. Both
  transforms are zero in the front end (the match flag is set there anyway), garbage for the first second of the loading
  screen (opp Z 30.85) and the intro places fighters below the floor (Y -0.63), so: zero gate for menus, a 3 s sliding
  floor for the height, and the pulse runs only while one of the three clock words changed in the last 400 ms (`+0x370`
  is static during a fight; `+0x938` advances ~0.52/s in practice). Verified: stopped through loading, running in the
  fight, stopped 0.7 s after pause, running on resume.
- Sound: 90 ms stereo pulse, sine + 0.35 x 2nd harmonic, 4 ms attack, 30 ms exponential release, constant-power pan
  `0.35 + 0.65 * d` toward the opponent, interval 120-650 ms over 0-7 units, pitch 440 Hz x 2^(height/2.0). Volume squared
  like FFVII-Access. Own thread; blocking waveOut play then sleep. Hotkey Ctrl+Shift+P; ini `radar*` keys.
- The user's keyboard binds (read from the Controller screen): up W, down S, left A, right D, LP 2, MP 3, HP 7, LK J,
  MK 1, HK E, 3P 6, 3K W (W is bound twice: up and HK+MK+LK; W did not jump in the test), taunt 9. Movement keys are
  needed for in-match tests; arrows only work in menus.
- Practice menu: Escape pauses into a tabbed menu (Q/E switch tabs: Pause Menu, Dummy Options, Practice Options, Theme;
  the tab is remembered). Dummy Options -> Action cycles Stand/Crouch/Jump/CPU/Human/Record with Left/Right. Restored
  to Stand and Health Max to 100% after the test.
- **Exit crash fixed.** The game wrote a crash dump at every exit (`%LOCALAPPDATA%\CrashDumps`, also on 2026-10-08)
  right after the DLL logged `=== game exiting ===`. Parsed with the `minidump` Python package + a linker map
  (`build.ps1` now passes `/MAP`): the fault is `abort` <- `terminate` <- the atexit destructor of `speech.cpp`'s static
  `std::thread s_worker`, run by our CRT during `DLL_PROCESS_DETACH` at process exit while the thread was still
  joinable (we deliberately do not join at process exit). Fix: `speech::abandon()` detaches it in that branch.
