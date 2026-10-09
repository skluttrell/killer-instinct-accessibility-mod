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
