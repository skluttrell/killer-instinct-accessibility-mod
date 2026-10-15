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

## Open / not verified live

* Post-match flow (encounter choice cursor, fresh match-rewards summary, VS buffs) — needs a real
  mission played to the end. The loadout popup, consumable path and all menu readers were re-verified.
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
