# Research log 2026-10-09 (evening): Shadow Lords live capture

Goal: step 1 of the Shadow Lords milestone. Enter the mode with the installed `kiaccess.dll` (`log_events=1`,
`dump_populate=1`), walk as far as the tutorial allows, and learn how each screen talks to the engine so the readers
(step 3) can be designed. Driven from the terminal with `sendkeys.py` and `screenshot.ps1`; the game is still open at
the Emporium (Turn 3 of the tutorial) at the end of this log. Both flags are still on in the installed `kiaccess.ini`.

## Route taken
Landing page > Shadow Lords > (MainMenu.swf in state `ShadowLord`: New Game / Multiplayer / Shadow Lords Leaderboards)
> New Game > Normal > tutorial note popup > GA_HUB (populated, immediately replaced) > prologue cinematic > GA_PreLoad
(Jago vs Omen, 6 lines of dialogue) > tutorial fight (lessons 1-2, "Need Help?" popups) > GA_WarRoom (populated, shown
for one frame) > GA_PreLoad (Jago vs Gargos) > tutorial fight (lesson 3, combo breakers, ultra) > GA_Emporium (video,
Kan-Ra dialogue popup, Craft tab with a guided prompt).

## How the mode talks to the engine (new, differs from the menus done so far)
- All GA screens are loaded in one burst (`LoadDestinationComplete` for HUB, FTUE, WarRoom, Emporium, PreLoad, Loadout
  within 300 ms) and then shown/hidden by the mode itself. There is **no `LoadDestination` per screen**, so the
  narrator never announces GA_WarRoom / GA_Emporium / GA_PreLoad and keeps `current_swf_` at the last loaded movie
  (it reported GA_FTUE.swf while the Emporium was on screen). The reliable signal is `ei <Screen>.ScreenShown`.
- Navigation inside a GA screen is handled in AS3 (`ButtonDown` switch in the decompiled classes), so there are no
  `Navigate`/`Select` dispatcher calls. What does reach the dispatcher: `uiSounds.PlaySoundString` with mode-specific
  names (`Play_SL_Global_Toggle`, `Play_SL_Global_Select`, `Play_SL_Emporium_Craft_Toggle`, `Play_SL_PreLoad_FlavorText_P1/P2`)
  and a few AS->Lua calls (`GA_WarRoom.AS_UpdateMissionSelection {"Index":n}`, `AS_SelectMission`, `AS_DeployFighter`,
  `GA_PreLoad.AS_BeginLoading`). Readers must trigger on these and read the display objects on the next frame.
- Screen state lives in the Populate payloads: `GA_HUB.Populate {navigationTabData[], initialTab, charactersInfo[],
  threatData[], currency, timerData, gargosDebuffsData}`; `GA_WarRoom.Populate {threatData[{name, corruptionLevel}],
  hubNotifications}` + `PopulateCharacterGroups [{health, healthDesc, status, isPrimary}]`;
  `GA_Emporium.Populate/Refresh {navigationTabData[{key, data[{name, description, tokenCost, kiGoldCost}]}]}`;
  `GA_PreLoad.Populate {enemyData{combatants[]}, flavorText, dialog[{player, text}], ...}`.
- Hub layout (decompiled `GA_HUB_NavigationTabs`): a horizontal tab bar of 7 slots, the focused tab is always
  `mcTab3` (`FOCUS_INDEX = 3`); Left/Right scroll the data under it, A selects the tab's destination, B exits the mode,
  Y expands the daily rewards. Tab text: `<tabs>.mcTab3.mcTabTitleText.txtItem.text` and `.mcTabDetailsText.txtItem.text`,
  notification count `.mcTabNotification.mcNumText.txt.text`. Position = index of the focused title in `navigationTabData`.
- Pre-fight dialogue (`GA_PreLoad_Dialog`): `DialogData[DialogIndex]` with `player` 0/1, written into
  `mcDialogOverlay.mcPlayerDialog.mcSpeechText.txtItem` or `mcDialogOverlay.mcEnemyDialog.mcSpeechText.txtItem`, then the
  P1/P2 flavor sound plays. Enter advances; after the last line `AS_BeginLoading`. Mission text:
  `mcMissionInfo.mcMissionText.txtItem`, names `mcEnemyName`, `mcPlayerName`, `mcArenaName`.
- War Room mission list: `mcMissionList.mcMission{0..6}`, selected entry always slot 2; every move calls
  `AS_UpdateMissionSelection {"Index"}` and Lua answers with `Lua_UpdateEnemyDetails` / `UpdateMissionText`. Encounter
  (dialogue-choice) popups are `GA_WarRoom_EncountersPopup` with `mcTitle`, `mcBodyText`, `mcSelections.mcEntry{0..4}`
  and a `GetSelectionIndex()` getter on the selections module; confirmation goes to Lua via
  `AS_ConfirmEncounterPopupSelection`.
- Getters found in the other movies: `GA_Emporium.GetSelectedPackIndex/GetSelectedPackData`,
  `GA_SpiritLair.GetSelectedCardIndex/GetSelectedPetData`, `GA_Barracks.GetSelectedData`,
  `GA_Loadout_Popup.GetSelectionIndex/GetSelectedData/GetSelectedLoadoutFighterName`,
  `GA_Archives.GetSelectedEntryData`, `GA_Leaderboard.GetSelectionIndex/GetCurrentIndex`. Several return objects, not
  ints, so the GFx bridge must be able to serialise an object (or we read the text fields instead).
- Cinematics are Bink movies (`MOVIES4K\SLMode\S3_*.bik`); their subtitles are **not** UI traffic. Each movie has a
  `Movies\SLMode\<name>_sub` XML in GLOBAL.PAK (64 such files for all movies) with `StartFrame/EndFrame` and a string
  key (`SL_INTRO1_1` = "Meanwhile, in San Francisco's Chinatown..."); the keys resolve through the existing CRC32 lookup.
  Narrating them needs the movie name and start time, which a hook on `bink2w64.dll` `BinkOpen` would give.

## Bugs in the current narrator exposed by the mode
1. **Popup flood**: the in-fight lesson popups ("Lesson 1: Basic Attacks") are re-populated every frame for about two
   seconds; the narrator spoke the same text 121 times. Needs de-duplication of identical popup text within a short window.
2. **JSON parse failures** on `GA_PreLoad.Populate`, `GA_Emporium.Populate` and the Kan-Ra `Popup.Populate` (the
   fallback read title and body from the screen but lost the button, "Enter: Give Kan-Ra Ingredients"). The event log
   truncates payloads at 300 chars, so the cause is unknown; the DLL must dump the raw payload of a failed parse to a file.
3. **Button names**: the narrator says "X: Skip Lesson" but the keyboard key is Tab (the game renders `[Tab]`); Y is
   also wrong for keyboard users. The game's own rendering of the popup legend is the source of truth.
4. **Input glyph tokens**: popup bodies contain `LIGHT_PUNCH`, `HEAVY_KICK`, `PUNCH_X_3` placeholders that the game
   replaces with the bound key (`[J]`, `[3] + [1]`); the narrator reads the raw token. Map them to attack names, and
   to the player's binds once those are known (the Controller config reader already sees them).
5. **Screen tracking**: see above, `ScreenShown` must drive `current_swf_` for GA screens.
6. HUD hint during the tutorial fight ("[LCtrl] Show Help Message") is not announced; source not yet identified.

## Artifacts
- `data/as3_listings/GA_*.txt` (13 movies) from `swf_inspect.py`; full FFDec decompile in the scratchpad `as3_src/`
  (regenerate: extract `swfs\ga_` type-63 entries from INTERFACE.PAK, rename to `.gfx`, `ffdec-cli -export script`).
- `data/xml/AssaultMode_Cinematics.xml`, `data/xml/slmode_subs/*_sub.xml`.
- Full session in the game's `kiaccess\speech.log` (23:19 to 23:45).

## Next
Step 2 (decompile) is effectively done. Step 3 in this order: fix bugs 1-5 (small, in `narrator.cpp`), then readers
for GA_HUB (tab bar), GA_PreLoad (dialogue + versus line), GA_WarRoom (mission list, encounter popup), GA_Emporium,
GA_Loadout_Popup, GA_MatchRewards/WrapUp, Archives, Barracks, SpiritLair, Leaderboard; then cinematic subtitles via a
BinkOpen hook. The tutorial cannot be left before Turn 3 ends, and a new run restarts it, so each live test of the hub
costs two scripted fights (about 8 minutes with lesson skipping: Enter closes a lesson popup, Tab skips it once it has
settled for 3 s; Enter outside a popup pauses the match).

## Fixes (2026-10-10 morning)
All in `07_dll/src/narrator.cpp` (+ a popup probe in `snapshot.cpp`), rebuilt and installed:
1. Popup de-duplication: identical popup text within 3 s is spoken once. Verified: "Lesson 1: Basic Attacks" spoken
   once (was 121 times), and the start-up sync popup once (was twice).
2. JSON repair before giving up: inside strings, invalid escapes lose their backslash (the real cause: Lua writes
   `\'`, as in "I\'ve come for you, brother"), raw control characters are escaped, invalid UTF-8 bytes are transcoded
   from Windows-1252. With `dump_populate=1` a payload that still fails is written to `kiaccess\data\badjson_*.txt`.
   The repair was checked standalone against the dumped GA_PreLoad payload and synthetic cases (scratchpad
   `repair_test/test.cpp`).
3. Popup button names come from the popup's own legend (`ABUTTON.mcIcon.txt.text` etc., learned at the first popup,
   defaults before that). Verified: "Buttons: Enter: Continue, Tab: Skip Lesson".
4. Attack placeholders (`LIGHT_PUNCH`, `PUNCH_X_3`, ...) are spoken as attack names. Verified: "pressing light punch".
   The bound key is not added yet (would need the controller binds; the game shows "[J]").
5. Shadow Lords screens are announced from `ei GA_*.ScreenShown` (not GA_FTUE). Verified: "Shadow Lords hub",
   "Versus screen". Side effect: the War Room flashes for one frame between fights and is announced; acceptable.
6. Not fixed: the "[LCtrl] Show Help Message" HUD hint during tutorial fights (no UI event carries it).

## Readers, step 3 (2026-10-10, in progress)
Built and verified live in one tutorial run (narrator.cpp `ga_summary` / `on_ga_focus` / `on_preload_dialogue` / `say_mission`):
- Screen summaries spoken after the screen name on `ScreenShown` (or alone when LoadDestination already named the
  screen): hub (day, KI gold, tokens), versus screen ("JAGO versus OMEN. San Francisco. Health: 100%", post-mission
  "Mission won/lost"), War Room (turn, wins/losses, mission count), loadout popup (fighters with health), match rewards
  (mission complete, fighter health, reward list).
- Versus-screen dialogue: each line spoken with the speaker name on the `Play_SL_PreLoad_FlavorText_P1/P2` sound
  (verified: "JAGO: Omen. What are you doing here?" ... four lines, then the post-mission line).
- Hub tab bar: probe of `mcNavigationElements.mcTab3` title/details/notification, spoken on change; position from
  `navigationTabData`. Not yet heard live: the tutorial never shows the hub for more than a frame.
- War Room missions: `ei GA_WarRoom.AS_UpdateMissionSelection {"Index"}` -> MissionData[idx] (name, location,
  difficulty, n of m, turns remaining, enemies, rewards; summary on Ctrl+Shift+D). Not yet heard live (build after the run).
- Shadow Lords `Refresh` payloads are merged into the stored Populate state (the Emporium's item lists arrive that way).
Tutorial route facts for test runs: the Turn 3 mission (Jago vs Mimic Jago, power level 1) must be won or the outcome
screen offers only "Rematch"; `05_tools/fightbot.py` (block, two Endokukens, forward heavy kick, alternating sides) won
it on the third try. The guided prompts arrive as `inv GA_FTUE.swf.Populate {promptData:{state, inputs, target}}` and
`inv <screen>.ActivateGuidedPrompt`; the prompt texts are string keys chosen by state inside the FTUE classes (table
extracted to the scratchpad `sl_prompts.json`, 44 states). Pack reveal: Right reveals the next card; a tutorial video
(`AS_PlayTutorialVid`) follows the guardian reveal.
Still to do: loadout popup slot navigation (`mcWarRoom_Loadout.mcLoadout.mcAddFighter_{0..2}` + `mcLaunchMissionButton`,
focus labels GainFocus / GainFocus_Filled, sub-buttons fighter/consumable/guardian via `mcFilledBacking` frame labels
HLCharacter/HLConsumable/HLSpiritPet), Emporium (tabs `mcTabs`, craft `mcCraftElements.mcItemTitle/mcBuyPackAmt/
mcCraftItemDetails(2)`, packs/KI gold/storage similar, purchase popup, pack reveal), Barracks, Spirit Lair, Archives,
encounter popups, guided-prompt texts, cinematic subtitles.

## Readers verified live (2026-10-10, late morning, saved playthrough at Turn 3)
The tutorial run was completed (two more fights won with `fightbot.py`, the deploy mission, Kan-Ra's second bargain,
the guardian pack) and the game left through its own menus, so the Shadow Lords menu now offers Continue and every
screen can be tested from a cold start in under a minute. Spoken, from the log:
- Hub: "Shadow Lords hub. Day 3. 0 KI gold. 80 tokens" then "War Room, 3 of 5"; tabs with notification counts from the
  payload (the on-screen badge keeps a stale number); the "Shadow Signatures Detected" alert (fixed string keys).
- War Room: "Turn 3. 5 wins, 0 losses. 3 missions" then "Relic Hunt, Egypt, Easy, 1 of 3, (0) Turns Remaining"; Up/Down
  read the other missions; Ctrl+Shift+D reads the summary and reward list. Encounter popups and "TURN n" are read from
  their invokes (tested only through the log of the tutorial's deployment report).
- Loadout popup: summary with the fighters' health; slots "Add Fighter" / "consumable: empty" / "guardian: empty";
  the fighter picker "Jago, health 100%, 1 of 3".
- Emporium: "Packs tab, 1 of 4", "Healing Pack, 1 of 12, 700 gems or 200 KI gold"; tab changes with Q/E; the craft
  screen uses its own "1/6" counter because `GetSelectedPackIndex` there returns a different index.
- Barracks: "Jago, health: 100%, 1 of 3, captain", rotation read from `AS_ClearAllNotificationsForFighter {rosterIndex}`
  (the centre module instance does not change when the roster rotates, so the probe approach was dropped).
- Spirit Lair: "6 astral energy. 1 guardian owned" then "Vampire Bat, damage type, none owned, 1 of 10"; Left/Right move.
Pitfall: `.visible` of the mode's popups and sub-screens is always true (they hide through frame labels), so it must not
be used as a gate; the tab strip's "Active" label and the focus labels are the reliable signals.
Still open: pack reveal cards, Emporium purchase/craft popups, Barracks artifact list and inventory popup, Spirit Lair
pet details / recharge popups, Archives, leaderboard, daily rewards panel, cinematic subtitles, guided prompt
verification with speech (the state table is in place), `fightbot.py` is a test tool only.

## Beta report 1 (Sightless Kombat, 2026-10-10) and the fixes
Report in `beta_tester_reports/sightless_kombat-beta_testing_report_1-sat-oct-10-2026.txt`. Order approved by the user:
radar, post-match stats, then the Character Select / Stage Select items, then Command List descriptions, then colours.
1. Radar irregular in normal fights: the gate "clock words unchanged for 400 ms = paused" flapped because the round
   timer only ticks about every two real seconds in a match (Practice and the Dojo have a float clock that moves every
   frame). Measured 12:08: running 0.3-0.7 s, stopped 1.6 s. Fix: 3000 ms stall and fighter movement counts as activity
   (`radar.cpp`). Verified 12:24: gate open continuously from the round's first tick to the KO (77 s); the pre-round
   intro still toggles it, which is harmless.
2. Post-match stats: all five groups (Hero, Offense, Defense, Combos, Style) with their metrics are spoken at verbosity
   1 and 2, kept for Ctrl+Shift+D at every verbosity; XP earned added. Verified ("Match results. Player 2 wins. 32 XP.
   Hero: Average Combo Damage ... Style: ... Most Stylish Combo 0").
3. Game settings popup (Y on Character Select, key R on this keyboard): `GameSettingsPopUp.swf`, three buttons
   `mcGSPopUpText.mcBtn{i}` with `mcTxt.TxtItem` / `mcValue.TxtItem`, FocusedLoop label; scan reader. Verified "Time
   Limit, 99, 1 of 2", "Difficulty, Beginner, 2 of 2", value changes "Infinite" / "99". The popup has no LoadDestination
   of its own (only LoadDestinationComplete), and its movie stays loaded after it closes, so it is announced from
   LoadDestinationComplete and ignored when it is not the current screen.
4. Music menu (Y on Stage Select): `mcMusicSelect.mcTrackText{i}` (15 slots, FocusedLoop), options from
   `Lua_PopulateMusicOptions {MusicOptions:[{name, id, isSelected, isEnabled}]}` whose names are string keys
   (`STAGE_DEFAULT_THEME`); position from the option list. Nothing fires when the menu opens (pure AS3), so the first
   track is heard on the first Up/Down. Verified the tracks; the position fix is in the build after 12:24.
5. Fighter level on Character Select: from the player-card payload's `Expanded.Entries[].Title` ("Jago - Lvl 1") and
   `NextUnlockLevel`, learned next to the fighter names. Verified "Jago, level 1, next unlock at level 5, 2 of 30".
6. Command List descriptions: the RefreshPage payload has a `Description` hash per move; it was only spoken at
   verbosity 2, now at 1 as well. Live check pending (the CPU ends a versus match in about 75 s; pause first).
7. Random stage: nothing in the UI traffic names the stage once Random is chosen (the loading screen payload has only
   the two fighters' thumbnails). Would need the stage id from the match state; not done.
8. Colour variants in the appearance texts: content work, awaiting the user's decision (announce the colour number and
   add per-colour notes gradually, or write all variants).

## Hotkeys released when the game is not in focus (user request, 2026-10-10 afternoon)
`RegisterHotKey` makes Ctrl+Shift+R/D/A/T/V/Q/P/U system-wide, which took them away from every other program while the
game ran. The hotkey thread now waits with `MsgWaitForMultipleObjects` (200 ms) and registers the keys only while
`GetForegroundWindow()` belongs to the game process, unregistering as soon as another window is in front
(`dllmain.cpp`). Verified with `05_tools/hotkey_probe.py`: another process can register Ctrl+Shift+R after an Alt+Tab
away from the game and cannot once the game is back in front; the in-game repeat key still works. Testing note:
`SetForegroundWindow` to another process and minimizing the game do not move the focus away from the game, Alt+Tab does.

## Stage announcement (2026-10-10 afternoon)
`GetLevelId` (RVA 0x68b020, now the `levelState` signature) returns `[*[0x27d5318] + 0xbfc]`, which is CRC32 of the
level path exactly as the Stage Select payload spells it (`levels\stage_11_maya\stage_11_maya` -> -476706460; verified on
City of Dawn, then Random -> -120101589 = Shadow Tiger's Lair). The radar thread logs it when the match goes live and
calls the narrator, which maps it through the Stage Select payload or the built-in table and says "Stage: X" unless
Stage Select already named that stage. Verified: "Loading. Jago versus Fulgore" then "Stage: SHADOW TIGER'S LAIR" 1.5 s
later on a Random pick.
Notes: attaching Frida to the game during a match (level_probe.py) made the process vanish without a crash dump; use
the DLL's own log for in-match reads. The crash dumps in %LOCALAPPDATA%\CrashDumps are all from 2026-10-09 (one per
game exit that morning; the exit-crash fix was added later that day). `sendkeys.py` and `fightbot.py` now refuse to
send when the game is not the foreground window (a key chain had typed into the user's Notepad).
Colours: the game has no colour names or swatches; each colour is a baked texture (`characters\<f>\<f>_cm_variationN`
in SPLIT_CHAR_<F>.PAK, a custom container, not DDS, format undecoded). Per-colour descriptions therefore cannot be
derived from data without decoding that container; awaiting the user's choice.
