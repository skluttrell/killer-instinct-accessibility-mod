# 2026-10-18 — Performance Test results reader (online multiplayer)

## Report

The online performance test (required before Ranked/online play) ends on a results screen that read
nothing. The user sat on the screen; screenshot `Screenshots/2026-10-18_09-02-56-performance_test.jpg`
shows: "Performance Test Results", a Pass hexagon with "Score: 1002 /1000", a system info + current
settings block (CPU, GPU, display, reflections/AA/motion blur/bloom/AO/shadows/textures), the banner
"Best possible performance for Online Play / You may play Ranked Leagues Online.", legend Y Graphics,
A Accept, X Redo Test.

## Why it was silent

Today's speech.log: entering Lobby mode read the "Performance Test Required!" popup, then
"Loading. Cinder versus Cinder" (the test is a scripted fight), fight clock stopped 09:02:05 — and
nothing after. The screen is `PerfResult_Popup.swf` (decompile already on disk from the 10-08 session's
scratchpad). It never gets a `LoadDestination` of its own — the fight flow shows it and `HideScreen`
returns to the "MessageHandler" — so the screen-name announcement never fired, and no reader existed.

## Reader (Populate-driven)

`PerfResult_Popup.Populate(data)` carries the whole screen: `PerfGrade` (0 fail / 1 pass; picks
mcLow/mcHigh), `PointsValue`, `PointsOutOf` ("/1000"), `SystemInfo` (the multi-line CPU/settings
block), `HowToImprove` (the banner), `SettignsChanged` [game's typo] — Accept (A) is only offered when
it is false; X redoes the test, Y opens graphics options. Static texts are localization keys, all
verified present in strings_en.tsv (PERF_SUMMARY_SCREEN_TITLE / _RESULT_PASS / _RESULT_FAIL /
PERF_SUMMARY_SCORE_TITLE / _PROMPT_ACCEPT / _PROMPT_REDO, OPTION_GRAPHICS).

`say_perf_result` (narrator.cpp) speaks: title, Pass/Fail, "Score: 1002 out of 1000" (leading "/" of
PointsOutOf becomes "out of"), the SystemInfo block with newlines (both real and literal `\n`) turned
into sentence breaks so Ctrl+Shift+Down steps one setting per line, the banner, then the button legend
from `key_labels_`. Sets `current_swf_`/`last_focus_` so repeat works. `PerfResult_Popup.swf` added to
SCREEN_NAMES ("Performance test results") in case a LoadDestination path ever does name it.

Built clean; installed over the running game via the exit-watcher (copy fired on game close). Not yet
heard live — to verify: relaunch, Options → run/redo the performance test (or X on the results screen
if it comes back), listen for the full read.

# 2026-10-18 (later) — Multiplayer Lobby menu + Ranked syncing screen

## Reports

Two online screens were silent (user on the Lobby screen live): (1) the Multiplayer Lobby menu read
only "item 1" instead of "Create Lobby / Search Lobby"; (2) the Ranked Leagues syncing screen read
nothing. Screenshots: `2026-10-18_09-22-36-Lobby.jpg` (Create Lobby / Search Lobby),
`2026-10-18_09-17-45-Syncing.jpg` ("Ranked Leagues / Syncing ladder information with the server.").

## Lobby menu (MultiplayerLobby.swf) — the "item 1" bug

The SCREENS table already had `MultiplayerLobby.swf: {indexFn: GetSelectionIndex}` — index worked
(0) but there were no groups, so the generic reader had no label and fell back to "item N". The movie
is one SWF with four in-movie subscreens switched by `mcLobby.gotoAndPlay` (LobbyMain / LobbyCreate /
LobbySearching / LobbyInside); the active one owns the cursor (single `GetSelectionIndex` across all).
Decompile already on disk (`87fc2036-*/scratchpad/as3/MultiplayerLobby`). Replaced the entry with a
scan-mode table of three groups:
- main (LobbyMain): `mcLobby.mcLobbyMain.mcBtn{i}` ×2 = Create Lobby / Search Lobby; label
  `mcTxt.txtButton.text` (MenuTextLeft_Anim → MPLobbyMenuEntry.Populate sets mcTxt.txtButton).
- create (LobbyCreate): `mcLobby.mcLobbyCreate.mcToggle{i}` ×8; label `mcTxt.txt.text`, value
  `mcValue.txt.text` (GenericToggle.Populate). 7 or 8 toggles live (UseLargeVersion = crossService);
  toggles are lobby size / private type / min rank / max rank / min perf / crossplay / lobby name
  / cross-service, per MPLCreateScreen.Select's CreateLobby args.
- search (LobbySearching): `mcLobby.mcLobbySearching.mcToggle{i}` ×5 (4 or 5 live).
All entries use FocusedLoop/UnfocusedLoop labels (match `isFocusedLabel`); scan's per-group pos/count
keeps the inactive subscreens from inflating the active one, and pick_group returns whichever group
has the focused item. Also added `MultiplayerLobby.swf` to the PlayScrollLeftRight suppress list in
on_focus (GenericToggle plays the step sound *before* SetText, same as OptionsMenu — the deferred
snapshot delivers the new value; without this the old value would be spoken first). This one table
covers both the Lobby menu and the Lobby Creation screen the user wants next.

## Ranked syncing screen (MultiplayerRanked.swf)

Not a popup — the full-screen one in the screenshot loaded silently (no SAY line between the 09:17:19
"item 1" and the 09:17:45 screenshot). The movie has zero named DefineEditText fields (swf_inspect);
its "Ranked Leagues / Syncing ladder information with the server." text is injected by Lua, and
ButtonDown is empty (non-interactive) — nothing for the generic reader to probe. It only ever shows
this one message, so on its LoadDestination (dest == "MultiplayerRanked.swf") the narrator now speaks
a fixed line from the game's own strings, addressed by hash since the symbolic keys are unknown:
`#1002906451` = "Ranked Leagues", `#3889741323` = "Syncing ladder information with the server."
(hex 3bc72353 / e7d8be0b). Note: a *separate* spinner Popup.swf with the same wording ("Ranked
Leagues. Syncing ladder information with the server.") already reads fine on some online entries
(seen at 09:16:35) — that path is untouched; this adds the standalone-screen case.

Both built clean; SCREENS JSON re-validated (32 screens, Lobby groups main/create/search). Installed
by the exit-watcher with the perf-test build. Not yet heard live — to verify: Multiplayer → Lobby Mode,
Up/Down over Create Lobby / Search Lobby, Enter into Create Lobby and step the toggles with
Up/Down (value with Left/Right); and Multiplayer → Ranked Leagues to hear the syncing line.

# 2026-10-18 (later still) — Exhibition front (Search) screen

## Report / shape

Multiplayer → Exhibition shows a Search panel (screenshot `2026-10-18_09-43-27-exibition.jpg`):
Search button, then toggles Minimum Rank (QUALIFIERS) / Maximum Rank (KILLER) / Minimum Performance
(Pass) / Allow Console Crossplay (Yes) / Allow Cross-Network (Yes), then an Invite Player button —
all under one cursor. MultiplayerExhibition.swf is one movie with three subscreens (MPEFrontScreen /
MPELobbyScreen / MPELeaderboards) sharing GetSelectionIndex; decompile in
`87fc2036-*/scratchpad/as3/MultiplayerExhibition`. Front entries live at
`mcExhibitionMain.mcExhibition.{mcBtn0, mcToggle1..5, mcBtn2}` (MPEFrontScreen.SetupScreen), idx 0 =
Search, 1-5 = the toggles, 6 = Invite Player. Buttons (MPExhibitionButton) store text at
`mcTxt.txtButton.text`; toggles (GenericToggle) at `mcTxt.txt.text` + value `mcValue.txt.text` — two
different label paths interleaved under one cursor, so a single scan group can't read both.

## Reader (payload label + live value, dedicated handler)

Added `MultiplayerExhibition.swf` to SCREENS as indexFn GetSelectionIndex with a probe of each entry's
`currentLabel` (s0-s6) and each toggle's live value (v1-v5). New on_focus handler: the label comes from
the stored Populate payload `FrontScreen.Options[idx]` (`.key`, `.type` "Button"/"textlist"/"FillBar",
`.textValues`, `.value`); the live value for a toggle comes from probe v{idx} (payload value is only the
initial one), with a textValues[value] fallback; position is idx+1 of Options.size(). Gated on a front
entry actually holding focus (GainFocus/FocusedLoop, not Lose/Unfocused) so the lobby/leaderboard
subscreens — which deactivate these and have their own structure — don't get misread into FrontScreen's
option list. PlayScrollLeftRight (value change) suppressed on the immediate snapshot like the other
toggle screens; the deferred/SetVariables snapshot speaks the new value (dedup on the |item key stops
the double). Focus labels GainFocus/FocusedLoop confirmed via swf_inspect; toggle text paths confirmed
from the Exhibition movie's own GenericToggle.as.

Built clean; SCREENS re-validated (33 screens). Installed by the exit-watcher with the earlier builds.
Not heard live — to verify: Multiplayer → Exhibition, Up/Down over Search / the 5 toggles / Invite
Player (Left/Right to change a toggle value). Lobby (MPELobbyScreen) and Leaderboards subscreens left
unbuilt — reached only after matchmaking; add later if needed.

# 2026-10-18 (evening) — Ranked Leagues hub menu (MultiplayerLadder.swf)

## Report / what the log showed

Multiplayer → Ranked Leagues, after the syncing spinner, reaches the Ranked hub
(screenshot `2026-10-18_09-54-13-ranked_leagues-pop-up_message.jpg`): a welcome dialog, and behind it a
menu (Search For Match / Leaderboards / Stats / Requalify), a player card (top-left, X to expand), a
"Time Until Killer Tier Reset" countdown, projected placement bars. The installed log showed this screen
loaded as raw "MultiplayerLadder" (not in SCREEN_NAMES) and its menu read "item 1" (no reader entry).
The welcome dialog produced no SAY line → silent. This screen is MultiplayerLadder.swf; decompile in
`87fc2036-*/scratchpad/as3/MultiplayerLadder`.

## Menu reader (built)

MultiplayerLadder.swf is one movie with three subscreens (front menu / rank-up / leaderboards) sharing
GetSelectionIndex. Front menu class = MainScreen (extends MPLadderFrontScreen); its four entries are at
`mcMenuGroup.mcMenuGroup.mcMenuBtn{0,1,3,4}.mcBtn0` (note the gap at btn2), all MPLadderMenuEntry buttons
(label `mcTxt.txtButton.text`, desc `mcDescription.mcDescriptionContainer.txtDescription.text`, state
currentLabel = GainFocus/FocusedLoop). Added SCREEN_NAMES "Ranked Leagues" and a scan-mode SCREENS entry
with one group "menu" (explicit item paths, since btn2 is skipped) gated on `mcPlayerCardP1.visible` —
the player card is shown only on the front screen (EnterFrontScreen sets it visible; EnterLeaderboard/
EnterRankUp hide it), so the leaderboard/rankup subscreens (card hidden → group hidden → no focused
item) stay silent instead of being misread into the menu. Labels come from the payload at runtime;
descriptions read at verbosity 2. No narrator code beyond the SCREEN_NAMES line — the generic scan
handler consumes the group.

## Welcome tutorial (NOT built — needs a live capture)

The welcome dialog is the ladder tutorial (`mcTutotialScreen` = MPLadderTutorial), a multi-page overlay,
not a Popup.swf (hence silent). First-entry pages: "Welcome" (keys NOTIFICATION_LADDERSYS_FTU_TITLE =
"Welcome to the All-New RANKED LEAGUES…" / _FTU_BODY = "Every month the Top 32…") then "TimeToQualify"
(NOTIFICATION_LADDERSYS_QUALIFY_EXP_TITLE = "Time to Qualify!" / _BODY). Each page is a frame of
mcTutotialScreen (gotoAndPlay by name: Welcome / TimeToQualify / OnTrack / LadderRules_01-03 /
KillerCongrats / KillerRules / Reset / ProStatus / ProbationWarning / InProbation / MonthlyReset /
InProbationDetails), each with its own title/body movieclip (mcWelcomeTitle/Body, mcTimeToQualifyTitle/
Body, …). A reader would probe `mcTutotialScreen.currentLabel` + each page's `<clip>.txtItem.text` and
announce on change. BLOCKER: the first page is shown from an internal animation-completion callback
(MPLadderBars: ladder-bar progress animation → UnlockInput + TutorialInterface.ShowNextPage), with no
dispatcher/inv event the hook sees, and page advances play "TutorialAccept" (a uiSounds event, unverified
as a trigger). Needs a live `log_events=1` capture of entering Ranked to find the observable event that
coincides with the tutorial showing/advancing before it can be wired reliably. Deferred.

Built clean; SCREENS re-validated (34 screens). Installed by the exit-watcher with the earlier builds.
Not heard live — to verify: Multiplayer → Ranked Leagues, dismiss the welcome dialog (Enter), Up/Down
over Search For Match / Leaderboards / Stats / Requalify.

# 2026-10-18 (evening) — Shadow Lords MP = Exhibition with IsShadowLords

Screenshot `2026-10-18_10-11-57-shadow_lords_mp.jpg` shows the same Search panel as Exhibition (Minimum
Rank / Maximum Rank / Minimum Performance / Allow Console Crossplay / Allow Cross-Network / Invite
Player). Confirmed from the AS3 and the live log: selecting "Shadow Lords MP, 4 of 5" loads
MultiplayerExhibition.swf with `IsShadowLords=true` (MultiplayerExhibition.Populate reads
param1.IsShadowLords; EnterFrontScreen then sets the title to MP_MULTIPLAYER_MENU_TITLE = "Shadow Lords
MP" instead of MPMENU_EXHIBITION = "Exhibition"; the intro plays "SLBuildOn"). The log showed it
announcing "Exhibition" then reading "item 1". So the just-built Exhibition front-screen reader already
covers Shadow Lords MP — the entry paths (mcExhibitionMain.mcExhibition.{mcBtn0,mcToggle1..5,mcBtn2})
are identical; only the screen NAME was wrong.

Fix: the LoadDestination name for MultiplayerExhibition.swf is now deferred (its branch in on_ei says
nothing) and chosen in the Populate handler from IsShadowLords. Because the front-screen focus read
fires in the same Populate event with interrupt (and would cut the name off), the name is stashed in
`mp_screen_name_` and the Exhibition focus handler speaks it as a prefix on the first read
("Shadow Lords MP. Search, 1 of 7" / "Exhibition. Search, 1 of 7"), then clears it. Built clean, 34
screens. Installed by the exit-watcher with the rest.

# 2026-10-18 (late) — Lobby subscreen fix (pick_group picked the stale subscreen)

## Diagnosis from a log_events=1 capture (loot_probe=0 to unflood)

User report: Lobby Search toggles read their values but not the label/heading; Create Lobby re-said
"Create Lobby" on every option change. The focus records show the cause: MultiplayerLobby.swf is one
movie with three in-movie subscreens, and the INACTIVE subscreen's button stays in FocusedLoop
(selecting "Create Lobby"/"Search Lobby" never un-focuses that main button). So every snapshot has two
focused groups, e.g. on the search screen: main{index:1,"Search Lobby",FocusedLoop} AND search{the real
toggle}. pick_group walks groups in key order (create, main, search) and returns the first whose
state_of()=="focused" -- so it returned main ("Search Lobby"), deduped against the earlier main read ->
silence on navigation; only Left/Right values spoke (the shared-toggle OptionsMenu.SetVariables event
says the bare value). On Create the same stale main button won during a toggle's step: the scan marks
the stepping create toggle focused (isFocusedLabel accepts "StepRight"), but pick_group's state_of()
does NOT treat "Step*" as focused, so it fell through to main -> "Create Lobby, 1 of 2" every change.

## Fix

Dedicated MultiplayerLobby.swf handler in on_focus: pick the group for the ACTIVE subscreen from the
`screen` probe (mcLobby.currentLabel -> LobbyMain/LobbyCreate/LobbySearching -> main/create/search),
never pick_group. Read that group's focused item (label, value, pos/count). Dedup on item identity
(label+pos), NOT the value, so a Left/Right change keeps the same item and stays silent -- the bare
value is already spoken by the shared-toggle SetVariables event (on_ei ~line 1051, fires because the
narrator's current_swf_ stays MultiplayerLobby.swf; the dispatcher's s_currentSwf flaps to
OptionsMenu.swf but snapshots fall back to s_lastGood). Name the subscreen once on entry
("Create Lobby." / "Search Lobby.") as the heading, via `lobby_sub_`. Applied the same item-identity
dedup to the Exhibition/SL-MP handler so its value changes aren't double-read either. Net: navigate ->
"Minimum Performance, Pass, 5 of 8"; change value -> just "Fail"; enter a subscreen ->
"Create Lobby. Lobby Size, 8, 1 of 8".

Built clean, 34 screens. Re-armed the exit-watcher (baeypiryt). log_events stays 1, dump_populate back
to 0, loot_probe 0 for one verification pass -- revert to log_events=0/loot_probe=1 once confirmed.

## Lobby inside (player list) reader — LobbyInside subscreen

Verified live that the Lobby config fix works ("Search Lobby. Lobby Size, Any, 1 of 5" + toggles).
User then joined an actual lobby (LobbyInside, MPLLobbyScreen) and arrowed the player list — silent
(same stale-pick_group bug; my handler returned for LobbyInside). The screen is up to 8 player entries
mcGamerTag0-7 (LobbyEntry/MPLobbyEntry, FocusedLoop), name `mcTxt.txtButton.text`, win/loss
`mcWinsLosses.txtButton.text` ("12 / 2"), crown `mcCrown.visible` (front of the battle queue, queuePos
0). Live data streams via `inv MultiplayerLobby.swf.UpdateLobbyInfo` (~every 266 ms, Players[] with
name/ratio/Visible/queuePos/IsUserBlocked, Host, Slots, NumFreeSlots, isHost) — NOT a snapshot trigger,
so no spam; reads fire on navigation (PlayScroll) and on ScreenShown (fires repeatedly in the lobby, but
deduped). Captured one full payload to scratchpad/lobbyinfo.txt.

Built: added a 4th scan group "lobby" (mcLobby.mcLobbyInside.mcGamerTag{i}) and a LobbyInside branch in
the MultiplayerLobby handler: reads "<name>, <W> wins, <L> losses[, next to fight][, n of m]", names the
player count once on entry ("4 players in lobby. ..."), dedups on name+pos (list updates don't re-read).
Read from the display scan (not the payload), paths confirmed from MPLobbyEntry.Populate. Host / "you" /
blocked / muted not yet surfaced (would need the payload; add later). Built clean, Lobby groups now
main/create/search/lobby. Re-armed the exit-watcher (brpi7xswl). NOT verified live — a populated lobby is
hard to reach on demand (needs matchmaking to find one with players).

Not heard live — verify: Multiplayer → Shadow
Lords MP (hear "Shadow Lords MP" + the toggles) and Multiplayer → Exhibition (hear "Exhibition").

# 2026-10-18 (late) - Main menu stray "main" before "Fight" (landing to main menu)

User: selecting Single Player / Multiplayer on the landing page says "main" then "Fight". It is the
"Main menu" screen-name announcement (SCREEN_NAMES) fired on the MainMenu.swf LoadDestination, cut off
~64 ms later by the first item read ("Fight, 1 of N", both interrupt=true), heard as "mai-Fight". The
landing choice already named the destination, so the name is redundant on that path. Fix: in on_ei
LoadDestination, when dest=="MainMenu.swf" AND Current=="LandingPage.swf" (the payload carries
Current:"LandingPage.swf" from a landing choice, vs the sub-screen name when returning), skip the say.
Returning to the main menu from a sub-screen still announces "Main menu". narrator-only change, no
SCREENS change. Built clean; the pending lobby exit-watcher (brpi7xswl) installs this too on exit.
Not heard live yet - verify: landing to Single Player/Multiplayer reads just "Fight, 1 of N".

# 2026-10-18 (late) - Command List input-token stepping (beta item 3)

User wants to step through a move's individual button/direction tokens on the Command List. The Command
List uses Up/Down for moves, LB/RB for pages, A/B/X/Y for actions; Left/Right are free but plain arrows
can't be hooked (the narrator only registers Ctrl+Shift hotkeys, plain keys go to the game). So reused
the existing line-by-line review hotkeys (Ctrl+Shift+Down/Up): on the Command List they now step the
focused move's input tokens instead of sentences.

Implementation: split humanize_command into command_tokens() (returns the token list: "down",
"down-forward", "forward", "plus", "heavy punch") + humanize_command() = join of that. on_move_selected
stores cmd_tokens_ + cmd_tokens_src_ (= last_text_ of that move's announcement). review_step: when
last_text_ == cmd_tokens_src_ and cmd_tokens_ non-empty, review_is_command_=true and review_lines_ =
cmd_tokens_ (header "Command, N inputs." vs "Review, N lines."); otherwise unchanged (sentence review).
Ties the tokens to the current move, falls back cleanly off the Command List or for command-less moves.
Built clean, narrator-only. Pending lobby exit-watcher (brpi7xswl) installs it on exit.

Not heard live - verify: Command List (e.g. Dojo/pause -> Command List), focus a move, Ctrl+Shift+Down
steps "Command, N inputs. down" then "down-forward" ... Ctrl+Shift+Up goes back, "Top"/"Bottom" at ends.
NOTE: if the user wants PLAIN Left/Right instead of Ctrl+Shift, that needs a low-level keyboard hook
(bigger change) - offered as a follow-up.

# 2026-10-18 (late) - Ranked welcome tutorial read (found the trigger via live capture)

The deferred tutorial is now built. Live log (log_events=1) showed the welcome page appears via:
Populate (tutorialInfo.tutorialTypeTable:[{Index:0}] = WelcomeTutorial) -> EnterFrontScreen ->
BuildOnComplete -> ForceUpdateCurrentData -> UpdateCurrentScreen -> (ladder bar anim) ->
ei MultiplayerLadder.ProgressAnimationComplete AND ei uiSounds.TutorialPopup. The hookable trigger is
uiSounds.TutorialPopup (played by MPLadderTutorial.ShowTutorial every time a page is shown, incl. A
advances). Added "TutorialPopup" to hooks TRIGGERS.

The tutorial overlay (mcTutotialScreen = MPLadderTutorial) has no Popup.swf; each page is a frame
(gotoAndPlay by label) with its own title/body movieclip. All page texts live under mcTutotialScreen
(set by SetupTutorials upfront, only for the tutorials in the table). Added 29 probes to the
MultiplayerLadder.swf SCREENS entry: tutState (mcTutotialScreen.currentLabel) + <page>Title/Body
.txtItem.text for all 14 pages (Welcome/TimeToQualify/OnTrack/LadderRules_01-03/KillerCongrats/
KillerRules/Reset/ProStatus/ProbationWarning/InProbation/MonthlyReset/InProbationDetails). New
MultiplayerLadder handler in on_focus: map tutState -> probe keys, speak "title. body. <Enter> to
continue" (deduped on swf|tut); when tutState is not a page label ("Off"/menu), fall through to the
generic menu scan. narrator + snapshot + hooks change. Built clean, 34 screens, 29 ladder probes.
Re-armed the exit-watcher (b87eu57oq).

Welcome page keys (display text already resolved): NOTIFICATION_LADDERSYS_FTU_TITLE / _FTU_BODY, then
TimeToQualify = _QUALIFY_EXP_TITLE/_BODY (NumPagesWelcome=2). Verify: welcome is showing repeatedly on
Ranked entry (not yet dismissed), so exit (installs) -> relaunch -> Multiplayer -> Ranked Leagues; the
welcome should read, A advances to "Time to Qualify!" which should read too. Only the welcome flow is
live-verifiable now; the rank-change pages (promotion/probation/killer/reset) ship from the decompile
unverified (graceful: a wrong path reads empty title/body for that page only).

# 2026-10-18 (late) - Main menu reads wrong state after exiting a ranked match

User on the Multiplayer menu (after a ranked fight) heard the Single-Player items ("Fight, Store &
Collection, Fight Archive") instead of Local Play/Exhibition/Ranked/etc. Live log: the scan "main" group
read the CORRECT on-screen buttons (focus record: index1="Exhibition" desc "Play friendly matches
online", index2="Ranked Leagues"), but the narrator overrode the label with mm_entry (the JSON state
payload) which gave "Fight, 1 of 4" - the tracked mm_state_/payload was out of sync with the display on
the ranked return. Also: the return LoadDestination sends Destination="MainMenu" WITHOUT ".swf" (normal
is "MainMenu.swf"), which broke current_swf_ and the MainMenu PlayAccept/PlayBack state tracking.

Root reasoning: the old MainMenu reader used index mode + treated mm_entry as authoritative ("on-screen
text lags behind state changes"). That predated the BuildOff gate (beta item 1), which now drops reads
during a state switch - so the settled on-screen text is reliable and should win.

Fix (3 parts):
1. snapshot.cpp: MainMenu.swf -> scan mode, group "main" count 5 (mcMenuBtn0-4, each hidden until its
   state populates it) + visible field. The scan now gives label + pos + count straight from the display.
2. narrator.cpp on_focus: the mm_entry override is now a FALLBACK only (if (swf=="MainMenu.swf" &&
   isNone(label))) - used only when the scan couldn't read a button. The scan label wins otherwise.
3. narrator.cpp on_ei LoadDestination: normalize a dest with no ".swf" (append it), so "MainMenu" ->
   "MainMenu.swf" and current_swf_/screen name/PlayAccept-PlayBack all match.

Built clean, 34 screens, MainMenu scan=true. The pending exit-watcher (b87eu57oq, armed for the ranked
tutorial build which also never installed since the game hasn't exited) will install this latest build
(ranked tutorial + main-menu fix) on exit. Verify: exit -> relaunch -> Multiplayer menu reads Local
Play/Exhibition/Ranked/etc. correctly, incl. after returning from a ranked match; Single Player still
reads Fight/Store/Archive/Help; and the ranked welcome tutorial now reads too.
