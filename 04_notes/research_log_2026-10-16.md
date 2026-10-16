# Research log 2026-10-16: Fight Archive readers

Trigger: beta report `beta_tester_reports/2026-10-16-FRI_Test_report_and_suggestions.txt`, item
"Fighter archive still needs to be read out", with screenshots of every sub-page.

## Decompiles

Extracted the seven archive movies from `INTERFACE.PAK` (type-63 records under
`interface\flash_season3\swfs\`): `FightArchiveMain`, `Stats`, `FightTitles`, `Leaderboards`,
`ShadowLeaderboards`, `Replays`, `ReplayBrowser` (plus `FightTitles`' own `ScrollList`). Workflow as
before: `extract_entry.py` → `swf_inspect.py` (.gfx) → FFDec `-export script` (scratchpad `as3_src/`).
Naming surprise: the on-screen **"Fight Challenges" page is `FightTitles.swf`** — the keys
`FAMENU_TITLES_TITLE` / `FAMENU_TOTAL_TITLES` resolve to "Fight Challenges" / "Total Fight Challenges"
(the feature was renamed after the code was written). There is no FightChallenges movie.

## What each screen exposes

* **FightArchiveMain**: plain state menu (`States.Main.Entries`, 6 rows) — already readable through
  `label_from_json`; `GetSelectionIndex` exists. `CheckIsDestInstalled` fires on every move (already a
  trigger). Selecting plays the menu off (`StateExit` resets `SelectionIndex` to 0 before the screen
  unloads) — the deferred PlayAccept snapshot read "Fight Stats, 1 of 6" after choosing something else.
  Fixed with probes `currentLabel == "BuildOff"` / `transitioning` (a `BaseScreen` public var) that
  suppress reads during the build-off. This is one instance of beta-report item 1 (stale menu reads).
* **Stats**: landing Populate carries everything (KP/TotTime/OLTime pairs; Ranked/Unranked/Offline
  panels `{title, stats[8], chars[3], values[12]}` — values are **column-major** for the fighter table:
  6 pair values, then 3 usage rates, then 3 win rates). The All Stats view (Y) requests
  `GetStatsByCategoryAndSubIndex {CategoryIndex, SubIndex}` (captured in `on_ei` for the spoken prefix)
  and Lua answers with `PopulateOverall/Breaker/Combo/Combat/Characters`; the value order is hard-coded
  from `Stats.as`, labels come from `CategoryStatLabels[cat]` (some contain literal `\t` — stripped).
  `Populate` arrives with empty text probes, so the reader is payload-only (`custom: none`).
* **FightTitles**: `Populate.TitleCategoryNames[{Title, Count, CharName}]` + `TotalTitles`; rows via
  `GetSelectionIndex`. Enter → `GetTitleDetailsForCategory {Category}` → `PopulateTitleDetails`
  (array of `{Title, Count, Previous, Next, FillAmount}`): the whole list is spoken at once; the real
  progress number is `Previous + FillAmount * (Next - Previous)` (verified against the screenshots:
  Aerial Attacks 92 of 100). Detail scrolling is AS3-only and intentionally silent.
* **Leaderboards**: `Populate.CategoryInfo` (categories/subcategories as keys),
  `ReceiveLeaderboardRecords {topIndex, data[8 {rank, gamertag, column1..3}]}`,
  `PopulateColumnHeaders {Column1..3}`, `PopulatePlayerData {PlayerData {Rank, Gamertag, Column1..3}}`
  (footer; all "-" = no entry), `ReceiveColumnUpdate {row, column, text, forPlayer}` patches async
  cells (time/score arrive late). Focused row via `GetSelectionIndex`; category/sub/filter/loading all
  probed from the screen text. `ReceiveLeaderboardRecords` added to `INV_TRIGGERS` so a page flip
  re-reads the focused row.
* **ShadowLeaderboards**: records also arrive per column, so the reader scans the screen instead:
  `mcListItems.mcListItem{0..4}` (entry classes `ShadowLBEntryBase`; instance names are the public
  vars `RankPosition/GamerTag/CharacterName/WinsValue/LossesValue/RankingValue`, values under
  `…txtItem.text`, Wins/Losses/Ranking under `…mcText.txtItem.text`), focus = `GainFocus`/`FocusedLoop`
  timeline label (`HighlightEntry`). Column headers differ per category and are probed from
  `mcColumnHeaders`. Row 5 is the user's own footer row (not scanned).
* **Replays**: `Populate.ReplayList[{ReplayName, Date, Time, IsAFavorite, LegacyId}]`,
  `GetSelectionIndex` over the *current view's* list. X swaps to a favorites-only list built in AS3 —
  mirrored by filtering `IsAFavorite`, toggled on the `PlayMetal_HitLarge` sound (added to TRIGGERS
  with `PlayMetalSlide`). Y → `ToggleFavorite {IsFavorite, ReplayId}` (patch the mirror by `LegacyId`).
  Enter → `LoadReplayById`.
* **Achievements** menu entry calls `CallAchievements` → Steam overlay; announced with a Shift+Tab hint.
* **ReplayBrowser.swf** exists (likely the online replay browser) — not built, never seen in a log.

## Gotchas hit

* `PlaySound` events arrive as `uiSounds.<name>` — gating `fn == "PlayBack"` on `script == "Stats"`
  never fires; gate on `current_swf_` instead. (`CallUILuaScript` events *do* carry the screen's
  script name, e.g. `Stats.GetStatsByCategoryAndSubIndex`.)
* The announce prefix ("Survival Mode, Wins. ") must stay out of the dedup key, or the settle
  snapshot repeats the row without it.
* `snapshot.cpp`'s `SCREENS_JSON` outgrew MSVC's 16 KB literal limit (C2026): split into two
  concatenated raw strings.
* Keyboard equivalents confirmed: Y = **R**, X = **Tab**, LB/RB = Q/E (sendkeys `r`, `tab`, `q`, `e`).

## Verified live (two builds, game relaunched once)

Fight Stats summary (values match the screenshot), all five All Stats pages incl. empty-page "No data"
and "Summary" on back; Fight Challenges list + Jago's 22-challenge details with computed progress;
Leaderboards rows/scroll/page categories/subcategories/filter footer; Shadow Leaderboards both
categories with per-category columns; Replays rows, favorite add/remove (restored afterwards),
favorites view on/off with mirrored counts; Achievements overlay announcement; no stale archive-menu
reads after the gate.

## Beta report backlog (items 1-5, 16 — not built today)

1. Stale "last selection" on menu entry (Master menu example) — the archive case is fixed by the
   BuildOff gate; the MainMenu state-switch case needs the same idea inside one movie.
2. Practice narrator mode announcing pressed buttons/specials (opt-in).
3. Command list: step through a move's inputs token by token.
4. F1 audio accessibility menu with persistent ini writes.
5. Line-by-line review mode for long dumps (post-fight report etc.) — the archive readers read
   everything at once by design ("fine to read out all the text", report item 5).
6. Shadow Lords ground loot after a fight (items to pick up; no capture yet).

# 2026-10-16 (later) — beta items 1 and 5, v0.2.7 packaged

## Main menu stale reads (beta item 1)

The tester's example ("Practice" just before "Dojo" when entering the Master menu) is the in-movie state
switch: `MainMenu.Select()` calls `StateExit` (plays `mcMenuGroup` "BuildOff") *before* the PlayAccept
sound, and only when that animation finishes does `MainMenuBuildOffDone` → `StateEnter` →
`ResetMenuButtons` reset `SelectionIndex` to 0 and repopulate the buttons. Every snapshot in between
(immediate PlayAccept + the settle reads) sees the *new* state (`mm_state_` was already pushed) at the
*old* index. Same shape as the archive fix: MainMenu.swf now probes `mcMenuGroup.currentLabel` (state
switch) and the root `currentLabel` (leaving the screen) and `on_focus` returns while either is
"BuildOff". The correct read still arrives: `ResetMenuButtons` fires `CheckIsDestInstalled` (a trigger),
whose +1-frame settle snapshot runs after `mcMenuGroup` goes "BuildOn" with the cursor at 0 and the new
texts in place. `Back()` takes the same path (Exit → PopState → PlayBack), so it is covered too. Source:
the MainMenu decompile from a previous session's scratchpad (still on disk).

## Line-by-line review (beta item 5)

Ctrl+Shift+Down / Ctrl+Shift+Up step through the last announcement one line at a time; the first press
interrupts the running mass read ("silence the mass reading" in the report) and says "Review, n lines"
before line 1. Lines are the announcement's sentences (split on ". ", "; ", "! ", "? "; the archive
dumps, match stats and dossiers all join their rows with ". " so they split naturally). Stepping past
the ends says "Top" / "Bottom" and stays. The review lines are spoken outside `say()` so `last_text_`
(and with it Ctrl+Shift+R and the review source itself) is not disturbed; any new announcement rebuilds
the cursor on the next step. Hotkeys registered like the others (game-foreground only); plain arrows
still reach the game.

Both features build clean; not yet heard live (next session: enter Master from Single Player and listen
for a single "Dojo..." read; dump a post-fight report and step through it).
