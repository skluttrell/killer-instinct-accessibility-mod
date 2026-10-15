# 2026-10-15 — beta reports 3: character select silence, Story mode, Store numbering, Shadow Lords fixes

Worked through the four unprocessed beta reports (my own 2026-10-11 and 2026-10-13, anomaly420 and
Sightless Kombat 2026-10-13). Everything below is in v0.2.4.

## Character select silence (anomaly420 + Sightless Kombat, verified fixed live)

Three strands, all in `on_charselect_ei`:

1. **One Back press from the costume stage** emits only `PlayBackPn` + `AS_SetSelections` with
   `Confirmed:false` — no `AS_ResetPlayerSelection` (that only comes on a *second* Back, when the
   fighter card clears). The handler used neither, so `cs_stage_` stayed `"costume"` and
   `on_charselect_focus` gated every fighter-cursor announcement off. Fix: an unconfirmed
   `AS_SetSelections` resets the side's stage to `"fighter"` and clears the dedup key; a confirmed one
   derives the stage from `isColorConfirmed` / `isCostumeConfirmed` (screen re-entry restores state
   this way).
2. **`AS_ResetPlayerSelection` carries `{"Index":n}`, not `{"PlayerIndex":n}`** — the handler parsed
   PlayerIndex (= 0), so a Player 2 reset cleared Player 1's stage and left P2 stuck. Side now falls
   back to `Index`.
3. The `Confirmed:false` re-sends of `AS_PlayerPickedCostume` / `AS_PlayerPickedColor` while stepping
   back, and `AS_PlayerUnready`, now re-sync the stage too.

The **Shadow Lords team select** silence (Sightless) is the same bug: after teammate 1 is "Ready" the
stage stuck at `"waiting"`; the game announces the next team slot with an unconfirmed
`AS_SetSelections`, which fix 1 turns into a reset. Verified live in the Assemble New Team flow
(backed out before confirming a team; the Turn 3 save is untouched — "Continue" still offered).

Also: "Now choosing Player 2's fighter" is gated on `cs_active_` now (every P2 cursor move re-sent
`AS_SetSelections` and repeated the phrase).

## Story mode reader (anomaly420's "item 1 2 or 3", verified live)

`Story.swf` reports only a row index; the labels live on screen. Decompiled the movie (INTERFACE.PAK →
FFDec): the bottom-right detail panel `mcStoryMain.mcUnlockDetails` always holds the resolved
selection — `txtItem` ("SABREWULF - ENDING 1"), `txtDescription` (unlock requirement), `txtDifficulty`.
Left/Right ending moves fire `PlaySound("PlayScroll")` from `StoryEntry.Navigate`, so probing these
fields on sound events covers rows, endings and season switches. Lock state comes from the Populate
(`Season{n}[row].EndingData.Ending{e}Unlocked`, ending digit parsed from the panel text). Season name +
"% complete" are spoken once per season change. New probe config `Story.swf` in snapshot.cpp +
`on_story_focus`.

## Store "2 of 8" (my 2026-10-13 note, verified live)

`Options[0]` ("What's New") has `canBeHighlighted:false` — the cursor can never land on it, but the
scan counted it, so the first reachable tile said "2 of 8". The narrator now numbers StoreMain tiles
among the Populate entries with `canBeHighlighted && !hidden`: "Bundles, 1 of 7".

## Shadow Lords fixes

* **"(0) Turns Remaining"** (Sightless): the game sends `days: "&(0) Turns Remaining"` in every
  mission, but *no script in GA_WarRoom.swf ever displays it* (checked the full decompile). Dropped
  from the mission line.
* **Match rewards named the wrong fighter** (Sightless: "tracking Kim Wu instead of Jago"): the
  GA_MatchRewards summary ran at `ScreenShown`, but the fresh Populate lands ~20 ms *later* — the
  summary read the previous match's stored payload (the stale dump on disk proved it). ScreenShown now
  drops the stored payload and the summary is spoken when the new Populate arrives
  (`ga_summary_pending_`). Team missions read every fighter's health.
* **Encounter choices silent after a mission fight** (both testers): the post-match encounter popup
  lives in GA_WarRoom.swf but appears while GA_Loadout_Popup.swf is the probing target (hooks pins it).
  `hookDispatcher` now also snapshots GA_WarRoom.swf on `PlaySoundString` while the loadout popup is
  current; choice moves fire `Play_SL_Global_Toggle`, so the existing encounter-cursor reader speaks.
  (Not yet heard live — needs a real mission; the encounter text itself was already read.)
* **Barracks artifact inventory** (my dead-fighter screenshot — that screen was fully silent):
  decompiled GA_Barracks.swf; the popup is `mcInventoryPopup` with the same title/description/toggle
  layout as the loadout consumable picker. New probes + `on_ga_focus` branch: opening announces
  "Artifacts: Jago, dead, Scroll of Restore, Killer, 1 of 1, \<effect\>", moves and Q/E tier switches
  speak. Roster rotation now says "dead / deployed / captured / in a match / unavailable" from the
  `status` code (0–5, mapped in GA_Barracks_Inventory_CharacterModule.as) instead of "health: 0%".
* **Ctrl+Shift+I status hotkey** (both testers): wallet (kiGoldOwned / tokensOwned = astral gems),
  astral energy (Spirit Lair payload), turn + record, roster with health/state. Values come from each
  screen's last Populate; the SL mode re-populates everything on entry, so they are current. Verified
  live from the hub.
* **Versus-screen buffs** (my vs-screen screenshot): GA_PreLoad combatants carry `modifiers[].key`
  (guardian, consumables, enemy perks) and a `stats` string ("+10% DMG +10% DEF") — appended to the
  summary at verbosity ≥ 1.
* **Consumable / guardian / artifact effects** now read at verbosity 1 (Sightless's request), not 2.
* Hub summary says "astral gems" instead of "tokens".

## Colour brief (my request)

`AS_PlayerPickedColor` now appends a hint derived from `costume_colors.json`: the first two distinct
"becomes" targets of the colour's line — "Color 2, 2 of 17, mainly dark red". Full line stays on
Ctrl+Shift+A. Skins (Mimic/Shadow/Gold) and Colour 1 get no hint.

## Midday additions (committed 9de8d91)

* **Store content pages** (StoreContentOverview.swf — Skins, Extras, Profile Card, Currency, Boosters,
  What's New): `ei PreviewItem {Id}` fires on every cursor move; items come from `inv PopulateItems`
  (a 6-item window, `MinIndex` from `GetItemsInCategory` keeps the numbering continuous). Verified live
  on Skins (paging) and Boosters. The Bundles and Fighters tiles open different movies (StoreBundles /
  StoreFighters), still unread.
* **Hub daily rewards overlay**: `PopulateDailyRewardExpanded.dailyRewardData` with
  `currentRewardData{displayName, day, quantity, completed}`, `previewRewardData[10]`,
  `multiplayerRewardData[4]` / `shadowMissionRewardData[3]` `{title, reward, completed}` (fields from
  the GA_HUB decompile, GA_Daily_Rewards_Expanded.Populate). It auto-opens from
  `GA_HUB.AS_BuildOnComplete` only right after a daily reward is earned, so the expanded announcement
  could not be re-triggered the same day; the close path and the quiet no-open path were verified live.

## Post-match flow verified on a real mission (2026-10-15 midday, user-approved burn)

Played Cleanup Protocol (Turn 3, Fulgore vs a buffed clone) end to end with fightbot. All of the
v0.2.4 code-only fixes heard live:

* Mission-start **target-selection encounter** read in full, and the choice cursor spoke on every move
  ("Target 704-H311_51ng, 2 of 5") — the GA_WarRoom-over-loadout probe works.
* The **resolution dossier** after choosing read in full ("Name: von Sabrewulf, Konrad ... Rewards:
  Sabrewulf appeared!") — the choice changed the opponent from Riptor to Sabrewulf.
* **Versus screen buffs**: "FULGORE versus SABREWULF. Ravensburg. your health: 100%. enemy health:
  110%. enemy buffs: Power Level II, Overdrive, Shadow Bond, +10% DMG".
* A **"New Enemy Power" popup** opens over the fight start and pauses it until Enter — it was read
  with its Continue button (fightbot does not press Enter; a human must, or the fight never starts).
* **Match rewards** announced the right fighter with fresh data: "Mission failed. Fulgore health: 0%"
  (the fight was lost — the clone's 110% health + three powers beat the bot). Turn advance ("TURN 4"),
  the new mission list ("Relic Hunt (2/6) ... versus Mimic Mira") and the War Room summary followed.

Save state after the burn: mission failed, Fulgore at 0% (unavailable), Kim Wu available again at 43%,
turn advanced to 4. Backed out cleanly; game exited.

## Ladder ending captured (user's Sabrewulf run, afternoon)

The user played a Season 1 ladder to the end with Sabrewulf (log with the raised 2000/4000-char caps).
Flow after the last win: Match results -> Advance -> ~21 s with no UI traffic = the **ending cinematic**
(fully voiced, no subtitles pass through the UI layer — nothing for the narrator to add) ->
`StoryMuralPopup.swf.Populate` -> `PlayPuzzlePieceAdded` -> Enter -> out. The popup's payload is pure
state: `MuralIndex` (season - 1), `NewEndings[]` and `EndingsUnlockState[27]` (9 rows x 3; row 9 is the
hidden fighter, "char_ending_hidden"), index = row * 3 + ending, rows in the Story screen's Season
array order (index 3 = Sabrewulf ending 1 ✓). No text on the screen beyond flair.

New readers from the capture (commit pending):
* **StoryMuralPopup**: "New ending unlocked: SABREWULF, ending 1. A piece of the season 1 mural is
  revealed. 1 ending unlocked so far. Enter: Continue" (names from the stored Story.swf payload).
* **PopulateLevelUpNotification** (match outcome): "Sabrewulf Level Up: Sabrewulf Background 1" — was
  silent before.
* Suppressed: after some matches the game runs a **hidden shadow-AI calibration match** ("Glacius
  versus Kilgore", stage DEBUG VOID) that triggered the loading-screen and stage announcements while
  the player sits in a menu. The stage line is now skipped for DEBUG VOID (the loading line remains —
  rare, and hard to tell apart from a real load).

## Pack reveal verified with a real purchase (user's own session, ~13:00)

The user bought a Healing Pack for 700 astral gems. Everything spoke on the first live run of the
v0.2.2 reveal reader: the buy popup ("Buy Healing Pack?, Pay 700 astral gems, 2 of 2"), the opening
line ("Pack opened, 4 cards. Right turns the next card, Escape leaves the pack") with the first card,
every card turned forward AND backward with tier and quantity, the return to the pack list, and the
updated wallet in the hub. Pack-reveal caveat removed from the README.

## Open / not verified live

* Hub daily rewards overlay's expanded announcement (needs the next daily reward).
* StoryMuralPopup announcement (built from the captured payload; re-heard on the next finished ladder).
* The hub's own Daily Rewards overlay (opens over the hub, swallowed input, says nothing — the main
  menu one is read). Noted in README known issues.
* Pack reveal still untested (needs 500 gems; save has 307).
* StoreContentOverview pages (Fighters/Skins/…) still say "item N".

## Session notes

* Dev loop this time: game closed → copy `build\dinput8.dll` over the installed one → launch via
  Steam → drive with sendkeys. No injection, no double-hook risk. Old 29 MB speech.log rotated to
  `kiaccess\speech_until_2026-10-13.log`.
* In the costume stage the two costume hexes sit vertically: Right does nothing there (a dead key press
  looks like a hang in the log). Backspace is NOT back in Character Select; Escape is.
* "Assemble New Team" (SL menu, 2 of 4) → difficulty → team select does NOT touch the existing save
  until the team is confirmed; Escape chain backs out cleanly.
