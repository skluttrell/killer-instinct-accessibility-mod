# 2026-10-21 — triage of the 2026-10-21 beta report (capture-driven)

User ran a Shadow Lords session + an Exhibition invite with log_events=1 + dump_populate=1 (fresh
speech.log, rotated the old one). Findings per reported issue:

## #4 SL mission rewards "regression" — NOT reproduced (works)

The capture shows GA_MatchRewards reads in full: "Mission complete. Jago health: 11%. Rewards: 4 Astral
Energy, 3 Nanotech Scrap, 13 Astral Gems, Fulgore Mk III Manual V...". The GA ScreenShown -> ga_summary
path is unchanged by v0.2.9/v0.2.10 anyway. Conclusion: jukesy is almost certainly on an older build
(or hit a rare mission type). No code change. Advise jukesy to update to v0.2.10 and recapture if still
seen.

## #3 post-mission encounter body + Ctrl+Shift+D — NOT reproduced (works)

Both a pre-mission and a post-mission War Room encounter read correctly: title + full body description
+ the choices, e.g. "South America. On your way to your mission in the Caribbean, a local woman
approaches you and offers to sell you a 'treasure map'... 4 choices. 1: Buy the map (Spend 100 Astral
Gems)...", and Ctrl+Shift+D re-read the body. The standard GA_WarRoom ShowEncountersPopup handler
(on_inv) reads the body and its focus reader gates on probe encounterShown. jukesy's "strange device"
(dismantle/activate/leave) likely a different/rare widget OR an old build. No code change yet; needs
jukesy's own capture on a current build if still broken.

## #1 daily reward not read entering Multiplayer — reads, content weak

The MainMenu daily-loot panel read once on entry: "Daily rewards panel. NEXT REWARD AVAILABLE IN. Day 1,
Day 2, ... Day 10. Escape to close" (Lua_PopulateDailyLootExpanded). It reads the reset text + day labels
but not which day is current or what today's reward actually is (previewRewardData[].name resolved to
"Day N", not the reward). Ambiguous vs the report ("did not read ... when I went to multiplayer"); the
panel shows once per session. Deferred: clarify with the user whether a different prompt appeared on the
Multiplayer entry, or whether they want today's specific reward named.

## #2 Exhibition invite -> Ready screen read wrong — FIXED

REPRODUCED. After invite, "Creating a lobby" -> the ready screen, but it read "Search, 1 of 7" (and the
toggles) instead of "Ready". Root cause confirmed from the capture: MultiplayerExhibition is one movie;
the ready/lobby subscreen (MPELobbyScreen) reuses the front screen's mcBtn0 (now "Ready",
MP_EXHIBITION_READY) and mcBtn2 (now "SearchAgain", MP_EXHIBITION_NEW_SEARCH) as its only 2 entries under
the shared GetSelectionIndex; options come from the Populate payload's `LobbyScreen.Options[]`, not
`FrontScreen.Options[]`. My Exhibition focus reader keyed on s0-s6 Focus and read FrontScreen.Options[idx]
-> idx0="Search", idx1="Minimum Rank QUALIFIERS" (exactly jukesy's "search and qualifiers").

Discriminator from the capture: `inv MultiplayerExhibition.swf.EnterLobbyScreen` fires right before the
misread (then PrepareForLobbyScreen, UpdateLobbyInfo, UpdatePlayerReadies). Fix: track exh_sub_
(front/lobby/lb) from EnterFrontScreen/EnterLobbyScreen/EnterLeaderboardScreen in on_inv (reset to "front"
on the Exhibition Populate). In on_focus, when exh_sub_=="lobby", read LobbyScreen.Options[GetSelectionIndex]
(0=Ready, 1=Search Again) instead of the front list; prefix the first read with "Match ready" so the new
screen is recognizable. Built clean (34 screens), installed 10:50. NOT re-verified live yet — logging left
on; verify: Multiplayer -> Exhibition -> Invite Player -> hear "Match ready. Ready, 1 of 2" and
"Search Again, 2 of 2". (The ready screen with a real opponent's player cards/names still needs a second
player.)
