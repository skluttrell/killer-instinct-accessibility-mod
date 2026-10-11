# Research log 2026-10-11: Shadow Lords mission start (loadout confirmation, pickers)

Trigger: beta report 2 (Sightless Kombat, 2026-10-10): "The missions aren't spoken at all, nor is the corruption level or
the match rewards"; beta report 1 (matrheine): "When choosing a mission, the consumables aren't spoken". The user then
narrowed it down: the War Room list and the loadout popup read fine, it is the stage after *starting* the mission that has
unread selectable things.

## What the logs showed
- The installed v0.2.2 did read the mission list in the user's own session this morning (`kiaccess\speech.log`:
  "Relic Hunt, Egypt, Easy, 1 of 3, (0) Turns Remaining" on every Up/Down). The `inv` log lines are cut at 300 chars,
  so `MissionData` never appears in a logged `GA_WarRoom.swf.Populate` even though it is in the payload (the dumped
  `populate_GA_WarRoom.json` has it).
- 5 of 22 logged `AS_UpdateMissionSelection` events were not followed by speech, all in the tutorial run: the War Room
  sends a second `Populate {"PlayThreatMeterSounds":true,"threatData":[...]}` (threat-meter refresh after a mission,
  no `MissionData`) which replaced the stored payload. Fixed by merging a War Room `Populate` without `MissionData`.
- After `Launch Mission`: `GA_WarRoom.AS_ValidateMissionLoadout` -> (warning popup when nothing is equipped) ->
  `GA_Loadout_Popup.swf.Lua_ConfirmLoadoutSelection` -> `Play_SL_WarRoom_Launch_Mission` -> the popup enters
  `NavState_Launch` and builds `mcWarRoom_Loadout.mcConfirmMission` (class `GA_Loadout_Popup_ConfirmationSelect`):
  two buttons `mcFight` / `mcDeploy` (`GA_Loadout_Popup_ConfirmationButton`: `mcTitle`, `mcDesc`, `mcPercentage`),
  Left/Right (`Play_SL_WarRoom_FightDeploy_Toggle`), Enter -> `AS_StartMission {"deploy":bool}`. A play-only mission
  (`missionSummary.canPlayOnly`) puts Deploy on its `Disabled` frame and Right only plays `Play_SL_Global_Negative`;
  a deploy-only mission does the same to Fight. Escape pops back to the slots.
- The hooks set the probed movie from every non-helper script event (`script + ".swf"`), so the popup's
  `GA_WarRoom.AS_ValidateMissionLoadout` switched probing to the War Room while the popup stayed on top; the launch
  sound's snapshot then read `GA_WarRoom.swf` (`encounterShown`) and nothing was said. Exception added in `hooks.cpp`.
- Consumable picker (`GA_ConsumableSelect`): 2 x 4 visible cells of `mcItem_0..9` (DEFAULT_INDEX 1, data alternates
  top/bottom row, so the second item is reached with Down, not Right), tiers via LB/RB (Q/E) with
  `mcToggle.mcCategoryName`, the focused item's name/description in `mcConsumableTitle` / `mcConsumableDescription`,
  "n/m" in `mcToggle.mcToggleQuantity`. Guardian picker (`GA_SpiritPetSelect`): `mcPetDeck_1.mcPetDeckName.txtItem`
  (name), `mcPetDeck_1.mcSpiritPetInfo.txtItem` (description, bare `\r` line breaks), energy only as a meter
  (`SetAstralEnergyLevel`) so it is taken from `spiritPetData[tier].pets[]` by name; the recharge popup is
  `mcAstralEnergyPopup` (`mcAstralEnergyWallet.txtItem`). The old probe paths `petName` (`mcSpiritPetInfo.mcPetName`)
  and `consumableShown` / `petShown` (`.visible`, always true) were wrong or useless and never read by the narrator.
- Sub-popup open state = timeline label of `mcConsumablePopup` / `mcSpiritPetPopup` / `mcConfirmMission`: `Off`
  (gotoAndStop at build), `BuildOn` / `ScrollLeft` / `ScrollRight` while open, `BuildOff` while closing.
- The loadout popup was announced only on its first open per War Room visit: `LoadDestination` kept `ga_announced_`
  for GA destinations, and the War Room's return from the popup has no `ScreenShown`. `ga_announced_` is cleared on
  every `LoadDestination` now.

## Verified live (dev build injected over the installed proxy, keys from `sendkeys.py`)
- "Turn 3. 5 wins, 0 losses. 3 missions. No corruption"; "Relic Hunt, Egypt, Easy, 1 of 3, (0) Turns Remaining, versus
  Kan-Ra"; "Mind Over Body, ..., versus Shadow Jago" (enemy from `opponents[].imagePath`, `*_mimic_icon` -> "Mimic X").
- Consumable picker: "Gibraltar Shield Mk III, 1 owned, Rare, 1 of 2"; back on the slot "consumable: empty".
- Guardian picker: "Ram, energy 4 of 5, Common, 1 of 1"; after Enter the slot reads "guardian: Ram".
- Confirmation: "FIGHT, Deploy unavailable" (Relic Hunt is play-only), Ctrl+Shift+D "Earn Astral Gems Maximum Rewards",
  Escape -> "Launch Mission", Escape -> "War Room" + the mission under the cursor.
- The loadout popup is announced on every open; "Kim Wu health 88% (unavailable)" on Jago's story mission.
- Not seen live: a Deploy-capable mission (none in this save), the recharge popup, the pack reveal, match rewards after
  a real (non-tutorial) mission (beta report 2 says they are not spoken; the tutorial's were, nothing in the logs
  explains the difference yet).

## Test-drive notes
- Build while the injected DLL is loaded fails at link time (`LNK1104 kiaccess.dll`): Ctrl+Shift+U first, then build,
  then inject. `prism.dll` stays mapped in the game after an unload, so `build.ps1` now tolerates that copy failing.
- The injected build starts with no War Room `MissionData` unless `build\kiaccess\data\populate_GA_WarRoom.json`
  exists (dump_populate=1 in the dev ini writes it); leave and re-enter the War Room after injecting.
- The game folder's `dinput8.dll` cannot be replaced while the game runs ("Device or resource busy"): install the new
  build after the game exits.
- Assigning a fighter / guardian in the loadout popup for a test does not change the playthrough; only Enter on FIGHT
  or DEPLOY does. Escape backs out of the confirmation.
