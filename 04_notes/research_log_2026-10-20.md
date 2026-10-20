# 2026-10-20 — Solo-testable multiplayer items from the 10-19 beta reports

Three online-menu gaps from `beta_tester_reports/2026-10-19-Mon_Test_report_and_suggestions.txt` that
can be reached without a second player. All built against the MultiplayerLadder / MultiplayerLobby
decompiles already on disk (`87fc2036-*/scratchpad/as3/`) and the lobbyinfo.txt capture from 10-18.

## 1. Ranked Leagues → Leaderboards (user: "Multiplayer leader boards does not read")

The hub's Leaderboards item enters MPLadderLeaderboards (`mcLeaderboards`, symbol406) — deliberately
left silent when the hub menu was built on 10-18 (the menu scan group is gated on the player card,
which this subscreen hides). It works exactly like the Fight Archive's Leaderboards.swf: Lua streams
`ReceiveLeaderboardRecords` (15-row pages, `topIndex` + `data` rows with rank/gamertag/column1..6),
`PopulatePlayerData` (your footer row), `ReceiveColumnUpdate` (async column fills). Column headers are
NOT a PopulateColumnHeaders event here — they come per category from the hub's own Populate payload
(`LeaderboardsPopulateObject.CategoryInfo[i].CategoryLabel/ColumnLabels`). LB/RB toggles category
(`mcMenuGroup.mcToggle.txtCategory`), X toggles the All/Friends filter (`mcFilterLabel.txtItem`,
"Current Filter: All"), row cursor = `mcLeaderboards.GetSelectionIndex()`.

Built: snapshot probes lbState/lbLoading/lbCat/lbFilter + call lbIdx on MultiplayerLadder.swf; on_inv
ReceiveLeaderboardRecords/ReceiveColumnUpdate extended to this movie (shared lb_rows_/lb_top_, per-swf
dedup keys); a dedicated MultiplayerLadder PopulatePlayerData branch (lower-case column1..6, headers
from `ladder_lb_cols()` matched by the on-screen category text); an on_focus branch gated on
lbState BuildOn/Loop that reads "Leaderboards." once on entry, the category on change, the filter on
change, and the focused row ("rank N, tag, <hdr> <val>..., in the top 32"). Falls back to
"rank topIndex+idx+1" when the row's own rank field is empty.

## 2. Rank on the Ranked hub (Sightless: "doesn't read your rank when you go to search")

The player card shows the tier text at `mcPlayerCardP1.mcProLadderElements.mcProLadderTextAmt.txtItem`
(LADDER_TITLE_QUALIFIER="Qualifier" … LADDER_TITLE_KILLER="Killer"; 1..15 are named tiers like
"Master"). New probe cardRank; the ladder on_focus branch stashes a one-shot `ladder_prefix_`
("Rank Master. ") when the front menu is active and the rank hasn't been said this visit
(`ladder_rank_said_`, reset on the hub's Populate), and the generic scan reader prepends it to the next
menu read: "Rank Master. Search For Match, 1 of 4". Prefix is gated on swf==MultiplayerLadder.swf so it
can't leak onto a popup.

## 3. Lobby inside (Sightless: "once you create a lobby, the screen does not correctly read the new
options presented — which I believe could be ready and search")

There are no Ready/Search cursor items in LobbyInside — navigation is only the player list; the "new
options" are the legend buttons (MPLLobbyScreen.UpdateLegend): A Gamercard, X Remap Controls,
Y Queue Battle / Spectate (MPLOBBY_SPECTATOR_NAV_BATTLE/SPECTATE_SEASON_2), B Back, LB Block, RB Mute,
host only: Back=Kick, Start=Invite. Plus header texts never spoken: txtLobbyTitle, txtGamerTag (host),
txtSlotsPublicPrivate ("4 / 0"), txtButton (rank limit).

Built: four header probes (lobTitle/lobHost/lobSlots/lobRank); the LobbyInside entry announcement now
reads "Lobby <title>. host <name>. slots <n/m>. rank limit <x>. N players. <Y>: queue battle or
spectate, <X>: remap controls, <B>: leave." and fires even before a player row takes focus. Key names
from key_labels_ (learned from the popup legend).

Also from the UpdateLobbyInfo stream (~every 266 ms, on_inv, gated on current_swf_): announce CHANGES
only, with a baseline captured on the first update of each lobby (lobby_seen_/lobby_toggle_/
lobby_names_, reset on leaving the lobby subscreen and on the movie's Populate):
- LocalPlayerToggled flip → "Queued to battle" / "Spectating" (the Y toggle gives no other feedback;
  capture confirms toggled=true while in the battle queue).
- Visible player set diff → "<name> joined. <name> left. N players in lobby" (jukesy: "When someone
  joins a lobby, it doesn't tell you"). Join/leave needs a partner to verify; everything else above is
  solo-testable.

## State

Built clean (34 screens, both SCREENS_JSON segments < 16 KB), installed over the game copy 16:31 with
the game closed. NOT verified live yet. Verify: Multiplayer → Ranked Leagues (hear "Rank <tier>." before
the menu), Leaderboards item (rows read, LB/RB categories, X filter, footer "You: rank ..."), and
Multiplayer → Lobby → Create Lobby → create it (hear the lobby header + options, then yourself in the
player list; press Y twice to hear "Spectating" / "Queued to battle").

Left for a partner session: join/leave announcements, P1/P2 name reads in matches, lobby player list
with real opponents, Exhibition's in-movie lobby/leaderboards subscreens (reached only via matchmaking),
rank-change tutorial pages.

# 2026-10-20 (later) — Rivals pre-fight story text + Season 1 ending text

Sightless Kombat's two arcade-mode items. Both texts are ENGINE-drawn — no Scaleform events at all —
which is why they were silent.

## Season 2 Rivals pre-fight text (table-driven, no new hook)

`gameinfo\StoryModeS2Definition` (extracted from GLOBAL.PAK with pak_table.py) scripts the whole mode:
8 characters (TJCombo, Maya, Gauze=Kan-Ra, Aganos, Riptor, Hisako, Cinder, Aria) × 8 matches, each with
`<LoadingScreen File=...><Text String="S2_<Char>_LOADSCREEN_<n>">` — the story paragraph is drawn by the
engine over the loading screen, from the normal string table (all keys resolve; a few are present but
EMPTY — TJ 1, Maya 8, Gauze 8, Cinder 7 — the game itself shows no text there). The loading flow still
fires `inv LoadingScreen.swf.Populate` (verified from the 10-15 Sabrewulf ladder log), so the reader
needs no new hook: shipped the mapping as `data\s2_story.json` (fighter code -> [{vs, key}], generated
from the XML; codes = thumb-filename codes, Kan-Ra=gauze, Omen=nox), and the LoadingScreen Populate
handler now appends the chapter text + "Press a button to continue" when `story_mode_` is on. The match
is picked by the tower's rung (`Challengers.Level`) when it agrees on the opponent (TJ fights Fulgore
twice), else by first opponent match. `story_mode_` is set on a SinglePlayerLadder LoadDestination and
survives only {LoadingScreen, MatchOutcome, StoryMuralPopup, Popup, Advance}; anything else clears it,
so a casual match with an S2 pairing can't false-positive.

## Season 1 ending text (localization hook, known build only)

The Sabrewulf-run log shows the ending as ~19 s of ZERO UI traffic between the tower and
StoryMuralPopup — engine-rendered slides (in-engine cinematics under animation\cinematic\characters\
<char>\ending_1..3). Their texts ARE in the string table (found by content: Jago 036c444f, Sabrewulf
179d76c5, Thunder 0fbb4d4c...) but no data file maps fighter+ending to keys (the exe's UI_<CHAR>_ENDING_n
strings do NOT hash into the table — they're something else), so the texts are caught at fetch time
instead: a MinHook detour on the game's own localization lookup (LocalizationManager::GetString,
documented in 01_research/live_session_2026-10-08.md; known-build fixed offset only, same policy as the
loot global — off on unrecognised builds, non-fatal if the hook fails). The detour is gated on an atomic
`g_locArmed` and forwards only 100+-char results under g_lock; the narrator arms it ONLY for the ending
window — LoadDestination "Advance" while story_mode_ && last MatchOutcome was a P1 win && the stored
tower payload says the final rung (`ladder_final_fight()`): Level == Opponents.size()-1 — and disarms on
the mural's Populate or any other LoadDestination. `on_locstring` dedups per window, caps at 4 texts,
speaks queued (non-interrupt). Known noise in the window (the game repopulates MainMenu behind the
ending) is filtered by the 100-char floor.

## State

Built clean, installed 17:02 (game closed), s2_story.json copied to build and game data folders. NOT
verified live. Verify: Story mode → Season 2 → any S2 fighter (TJ's FIRST fight is silent by the game's
own data — start text appears from fight 2) → hear the chapter text after "Loading. X versus Y"; and
finish any S1 ladder (Beginner) → the ending slides should read before the mural popup. S2 LOADSCREEN
texts spot-checked against the table; ending texts unverified against real endings (hook reads whatever
the engine fetches, so a mismatch is unlikely).
