Killer Instinct Accessibility Mod
v.0.2.7

Purpose:

Adds screen reader output to the menus of Killer Instinct (Steam version, free base game and Anniversary Edition) including the Shadow Lords mode, spoken descriptions of each fighter's appearance, and an opponent radar sound during matches.

Known Issues:

* This is a TEST BUILD. The mod writes kiaccess\speech.log in your Killer Instinct folder each session with every announcement. If anything goes wrong, please attach that file to your bug report.
* Shadow Lords: the leaderboard, the items dropped on the arena floor after some fights, and the Spirit Lair's excess-guardian popup are not read yet. The hub's daily rewards overlay reader has not been heard live yet (it only appears right after a daily reward is earned); if it stays silent for you, please attach kiaccess\speech.log. The subtitles of the story cinematics are not spoken either; the cinematics are voiced.
* The mural screen's announcement after finishing a ladder is built from a captured run but has not been re-heard live yet; if it stays silent after your next ladder, please attach kiaccess\speech.log.
* When a popup opens, the first thing spoken can be a button of the previous popup (for example "Purchase 300 KI Gold" just before "Exit Shadow Lords?"). The popup itself follows at once.
* The whole Store is read; only the online lobbies are not read yet beyond their basic layout.
* The fighter appearance descriptions were written from memory of the game and have not yet been reviewed by a sighted person.
* The opponent radar assumes you are Player 1 playing against the CPU. There is no radar for Player 2.
* The opponent radar pauses during the "Ready, Fight" intro of a round and for up to 3 seconds after a round ends. Please keep reporting whether the pulse, its panning and its pitch are useful.
* The colour descriptions (Ctrl+Shift+A on the colour stage) are generated from the game's own textures by comparing each colour with the default: they name the main areas that change ("dark blue becomes gold") but not which garment that is, and skin, hair and small details may be missed. The default look of each costume is still a hand-written text that no sighted person has reviewed.
* Announcements are always in English. The mod reads the game's English text table regardless of the language set in the game.
* For about 10 seconds after the landing page appears, the game shows a sync popup and a "Free Rotating Fighter" toast that swallow key presses. Wait for them to be announced before navigating.
* Only the Steam version of Killer Instinct is supported. The Microsoft Store / Xbox app version is not.
* If a future game update changes the executable so that the mod can no longer find its hook points, the narrator stays silent and writes the reason to kiaccess\speech.log. The game itself still runs normally.

New in this version:

* The whole Fight Archive is read (requested in the 2026-10-16 beta report):
  - Fight Stats: the summary page reads your total XP and play time, then the Ranked, Non-Ranked and Offline panels with their matches, win rates and your top three fighters ("no matches" when a panel is empty). Y (R on the default keyboard binds) opens All Stats; each page (Overall, Breaker, Combo, Combat, Character) is read in full, Q/E and Left/Right switch pages, Escape returns to the summary.
  - Fight Challenges: the total, then each category as you move (fighter, trophies, position). Enter reads the category's whole challenge list with the actual progress of each bar ("Aerial Attacks, 92 of 100").
  - Leaderboards: each row with its named columns as you move and page, the category and subcategory on every switch, the All/Friends filter, and your own entry at the bottom ("You: rank 1292..." or "You have no entry on this board").
  - Shadow Leaderboards: each row with rank, player, fighter and the columns of the chosen category.
  - Replays: the count on entry, then each replay with its date, time and favorite state. Y adds or removes a favorite, X switches to the favorites-only list ("Favorites only, 2 replays"), Enter loads the replay.
  - The Achievements entry says that the Steam overlay has opened and that Shift+Tab returns to the game.
* Line-by-line review (requested in the 2026-10-16 beta report): Ctrl+Shift+Down and Ctrl+Shift+Up step through the last announcement one line at a time, for long dumps like the post-fight report. The first press silences the running announcement and says how many lines there are; the ends say "Top" and "Bottom". Ctrl+Shift+R still repeats the whole announcement.
* Fixed (reported in the 2026-10-16 beta report): entering a menu section could first read the previously selected entry before the real one ("Practice" just before "Dojo" when entering Master). The menu is now silent while it rebuilds and only the real selection is read.

New in v0.2.6:

* The story dossier before an arcade ladder (the Ultratech databank page after picking your fighter in Story mode) is read in full: name, height, weight, age, threat level, last known location, the character's story text, and the continue prompt with your bound key.
* The arcade ladder tower is read: your difficulty, total wins and score, how many fights the ladder has, and the opponent you are at ("Next opponent: Riptor, 2 of 7"), spoken again with fresh numbers each time you return to the tower after a match.
* The end of an arcade ladder is covered: the ending itself is a fully voiced cinematic (nothing to read), and the mural screen after it announces which ending you unlocked by fighter and number, that a piece of the season's mural is revealed, how many endings you have so far, and the continue key.
* Level-up banners on the match results screen are read ("Sabrewulf Level Up: Sabrewulf Background 1").
* The game silently runs a hidden calibration match in a void stage after some fights; the mod no longer announces it as if a real fight were loading.
* The Emporium pack reveal has now been heard with a real purchase: all cards read as you turn them, forward and backward, with tier and quantity ("Phoenix Relic, killer, quantity 2, 1 of 4").
* Shadow Lords Archives are read: the four category tiles, the fighter grid ("Jago, 1 of 29"), every entry with its position and unlock state ("Guardians: Origins I, 11 of 37. Dossier Unlocked", or the hint telling you how to unlock it), and the dossier reading view itself — opening an unlocked dossier reads its full lore text, with Up and Down turning the pages and Escape closing.
* The Store's Bundles page reads each bundle with its price ("Killer Instinct: Anniversary Edition, 1 of 1"; a price that Steam has not delivered yet is left out instead of read as dashes), and the Fighters page reads each fighter with their state as you move through the grid ("Jago, owned" / "available" / "free" / "coming soon").
* Shadow Lords Spirit Lair: the guardian details popup is read (energy, abilities with their descriptions; Up and Down move through your cards of that guardian), the recharge popup reads its cost and your remaining energy as Left and Right change the amount, the dismantle confirmation is read, and trying to recharge a full guardian or one you cannot afford says why nothing happened.

New in v0.2.5:

* The Store's content pages (Skins, Extras, Profile Card, Currency, Boosters and What's New) are read: the page's name on entry, then each item as you move with its price and your level where shown ("Jago Mimic Skin, Your Current Jago Level is 3, price 600, item 19"), plus "on sale", "equipped" or "locked" when they apply. The item's description is on Ctrl+Shift+D. Paging through long lists keeps the numbering.
* The daily rewards overlay inside the Shadow Lords hub is read. It opens by itself when you have just earned a daily reward and it swallows the hub's keys while open; it now announces today's reward with its day and whether it is collected, how many of the ten days you have collected, the Multiplayer Gauntlet and Shadow Mission Loot objectives with their rewards, and that Escape closes it. Closing it says "Daily rewards closed" so you know the hub is listening again.
* The whole Shadow Lords mission flow of v0.2.4 has now been heard on a real mission: the target-selection encounter with its choices as you move, the dossier that resolves the choice, the versus screen with the enemy's buffs, the "New Enemy Power" popup at the start of the fight, and the match rewards naming the right fighter with fresh health, win or lose.

New in v0.2.4:

* Fixed: Character Select went silent after backing out of a picked fighter (reported by anomaly420 and Sightless Kombat). Backing out of the costume or colour stage, un-readying a player, and moving to the next Shadow Lords team slot all bring the fighter cursor back to life now, on both sides.
* Story mode (Single Player, Fight, Story) is read: each fighter's row with the selected ending, its difficulty, its unlock requirement and whether it is unlocked ("SABREWULF - ENDING 1, locked. Difficulty beginner. Unlock: Complete Story Mode. Row 2 of 8"), Left and Right move through the endings, and the season with its completion is announced on entry (reported by anomaly420 as "it just says item 1 2 or 3").
* Store: the tiles are numbered among the ones you can reach, so the first is "Bundles, 1 of 7" instead of "2 of 8" (the unreachable "What's New" banner was counted before).
* Ctrl+Shift+I reads your Shadow Lords status on demand: KI gold, astral gems, astral energy, the turn with your record, and every fighter's health or state (requested by two testers).
* Shadow Lords Barracks: the artifact inventory of a fighter is read (it was silent): opening it announces the fighter with their condition, then each artifact with its tier, position and effect as you move; Q and E switch tiers. A dead fighter is announced as "dead" in the roster instead of "health: 0%" (reported with the dead-fighter screenshot).
* Shadow Lords War Room: the mission line no longer ends in the meaningless "(0) Turns Remaining" (the game never shows that text; reported by Sightless Kombat).
* Shadow Lords match rewards: the screen was announced from the previous match's data, so it could name the wrong fighter with the wrong health and rewards (reported by Sightless Kombat); it now waits for the fresh payload. On a team mission every fighter's health is read.
* Shadow Lords: the choices of an encounter that opens right after a mission fight are read as you move through them (they were silent when the encounter appeared on top of the loadout screen; reported with the Relic Hunt screenshot).
* Shadow Lords: the consumable picker, the guardian picker and the Barracks artifacts read the item's effect at normal verbosity, not only at the highest one (requested by Sightless Kombat).
* Versus screen of a Shadow Lords mission: both sides' buffs are read ("your buffs: Ram, Odorous Wolf Heart, +10% DMG +10% DEF. enemy buffs: Power Level II, Vampirism, +10% DMG"; requested with the vs-screen screenshot).
* Colour stage of Character Select: each colour now comes with a brief hint of its look ("Color 2, 2 of 17, mainly dark red"), derived from the game's textures; the full description stays on Ctrl+Shift+A (requested in beta feedback).

New in v0.2.3:

* Shadow Lords War Room: the mission start is now fully spoken. After Launch, the Fight / Deploy confirmation reads the focused button ("FIGHT", or "DEPLOY, Success Chance 99%") with Left and Right to switch and Ctrl+Shift+D for the button's description; on a mission that must be fought it says "Deploy unavailable" instead of a position. Enter on FIGHT starts the match.
* Shadow Lords loadout: the consumable picker reads the focused item with how many you own, its tier and position ("Gibraltar Shield Mk III, 1 owned, Rare, 1 of 2"; Q and E switch tiers, Ctrl+Shift+D reads the effect), and the guardian picker reads the guardian with its energy ("Ram, energy 4 of 5, Common, 1 of 1") and the recharge popup when a drained guardian is chosen.
* Shadow Lords War Room: each mission now names the enemy ("Relic Hunt, Egypt, Easy, 1 of 3, (0) Turns Remaining, versus Kan-Ra"), the screen summary includes the corruption level of each region (or "No corruption"), the mission under the cursor is repeated when you close the loadout popup, and the loadout popup is announced every time it opens, not only the first time.
* Fixed: the mission list could go silent after a fight (the War Room refreshes its threat meters with a payload that has no mission data; it is merged now instead of replacing the list), and Shadow Lords payloads containing a path with "\u" were rejected by the JSON parser.

New in v0.2.1 and v0.2.2:

* The hotkeys are no longer held system-wide: they are active only while the game window is in front, so Ctrl+Shift+R and the others are free for other programs when you switch away.
* Fixes from the first beta report (thank you, Sightless Kombat).
* Opponent radar: the pulse no longer drops out in normal fights. The match timer only ticks every two seconds, and the radar mistook the gaps for a pause; it now watches the fighters' movement too.
* Match results read all the stats: Hero, Offense, Defense, Combos and Style with every metric, and the XP earned. Ctrl+Shift+D repeats the stats.
* The game settings menu on Character Select (the Y button, R on a keyboard with the default binds) is read: Time Limit and Difficulty with their values as you change them.
* The music menu on Stage Select (also the Y button) is read: track name, whether it is the current one, and its position. Press Down once after opening it to hear the list.
* Character Select announces each fighter's level and the level of the next unlock.
* The Command List reads each move's description (ender types, Instinct effects, Combo Assist explanations) at normal verbosity, not only at the highest one.
* The stage is announced as a fight loads ("Stage: Shadow Tiger's Lair") whenever it was not picked by name on Stage Select, so a Random pick, Shadow Lords missions and ladder fights tell you where you are.
* Costume colours: on the colour stage of Character Select, Ctrl+Shift+A now adds a line for the colour under the cursor, for the default and the retro costume, for example "Colour 3: much of the costume changes; dark blue becomes dark grey, brown becomes grey". These lines are generated from the game's textures (see Known Issues) and live in kiaccess\data\costume_colors.json, so they can be edited.
* Shadow Lords Emporium: the buy popup is read ("Buy Healing Pack? Pay 700 astral gems, 2 of 2"; Left and Right switch between KI gold and astral gems), as are the "Not enough" popup with its store options, the KI gold purchase popup, and the "Insufficient Materials" popup on the Craft tab with its Yes and No choices. Crafting an item announces "Crafted" with its name. The KI Gold tab reads each bundle's amount and price.
* Shadow Lords Emporium: when you buy a pack, the cards are read as you turn them ("Pack opened, 7 cards. Gibraltar Shield Mk III, rare, 1 of 7"), with the card's description on Ctrl+Shift+D. See Known Issues: not yet tested with a real purchase.
* Shadow Lords item names and descriptions no longer contain the game's colour-tag markup.

New in v0.2.0:

* Shadow Lords. The hub announces the day and your currencies, then the focused tab as you move Left and Right (Guardians, Barracks, War Room, Emporium, Archives) with its notification count. The War Room reads the turn, your record and each mission as you move through the list (name, region, difficulty, turns remaining); Ctrl+Shift+D reads the briefing and the rewards. The loadout popup reads the fighter slot with its consumable and guardian sub-slots, the fighter picker and the launch button. The versus screen reads both fighters, the arena and their health, then every line of the pre-fight dialogue with the speaker's name as you press Enter. Match rewards read the result and the loot. The Emporium reads the active tab and the focused pack, item or recipe with its price or crafting state. The Barracks reads the fighter in the centre as the roster rotates. The Spirit Lair reads each guardian type and how many you own. Encounter popups read their title, text and the choices; turn changes are announced. The tutorial's guided prompts are spoken.
* Popups that the game repopulates every frame (the fighting lessons of the Shadow Lords tutorial) are spoken once instead of dozens of times.
* Popup button names follow your keyboard binds: the mod reads the key shown in the popup's own legend, so a button that used to be announced as "X" is now announced as "Tab" when that is the key.
* Attack placeholders in popup text (which the game shows as your bound key) are spoken as the attack name, for example "light punch".
* Some popups and screens whose text failed to parse (dialogue with apostrophes, the Emporium) are read in full now, buttons included.

New in v0.1.0 (first release):

* Menu narration for: start screen, landing page, main menu and all of its states, Options, popups and toasts, Character Select (both sides, including the costume stage), Stage Select, loading screen, practice and versus pause menus, Command List (paging and move notation), Controller configuration (rows, rebinding flow, quit and discard popups), Dojo, Trials, match results and the exit confirmation.
* Fighter appearance: Ctrl+Shift+A on Character Select describes the physical appearance of the fighter under the cursor, with separate text for the retro costume. The texts live in kiaccess\data\fighter_appearance.json and can be edited freely.
* Opponent radar: a soft stereo pulse during a fight tells you where the opponent is. It is panned to the side the opponent is on, repeats faster the closer the opponent is, and rises in pitch when the opponent leaves the ground. Ctrl+Shift+P turns it on or off.
* The mod ships none of the game's text. On the first start it builds its text table from the game's own files (about 10 milliseconds) and rebuilds it automatically after a game update.
* Verbosity levels: label only, label and position ("3 of 5"), or label, position and description.
* Speech goes through a separate thread, so the game never waits on the screen reader.

Install:

* Install Killer Instinct from Steam. The base game is free; the Anniversary Edition DLC is optional and works the same way.
https://store.steampowered.com/app/577940/Killer_Instinct/
https://store.steampowered.com/app/2625580/Killer_Instinct_Anniversary_Edition/
Note: this mod does not cover the Microsoft Store / Xbox app version of the game.
* Run your screen reader. The mod speaks through the Prism speech library, which talks to the running screen reader. It has been tested with NVDA.
* Copy and paste dinput8.dll and the kiaccess folder (containing kiaccess.ini, prism.dll and the data folder) into the following folder, next to KILLERINSTINCTX64_R.EXE:
"C:\Program Files (x86)\Steam\steamapps\common\Killer Instinct\"
* Launch the game from Steam. You will hear "Killer Instinct narrator ready" shortly after launch, then "Start screen. Press Menu or Space" once the game has loaded.
* To uninstall, delete dinput8.dll from the game folder. The kiaccess folder can be deleted too.

Keys

All hotkeys work anywhere in the game, but only while the game window is the active window. When you switch to another program, the mod releases them so that program can use the same key combinations:

Ctrl+Shift+R: Repeat the last announcement.
Ctrl+Shift+Down and Ctrl+Shift+Up: Step through the last announcement line by line (next and previous line). The first press silences the current announcement and starts at the first line. The plain arrow keys still work in the game as usual.
Ctrl+Shift+D: Read the description of the focused item.
Ctrl+Shift+A: On Character Select, describe the physical appearance of the fighter under the cursor, or the chosen fighter. On the costume stage the retro costume gets its own text. Ctrl+Shift+D does the same there, since fighters have no description.
Ctrl+Shift+T: Read the ticker / message of the day.
Ctrl+Shift+V: Cycle verbosity: 0 label only, 1 label and position, 2 label, position and description.
Ctrl+Shift+Q: Narrator on or off.
Ctrl+Shift+P: Opponent radar on or off.

Getting into a fight:

Start screen: press Space.
Landing page: Shadow Lords, Single Player, Multiplayer, Store, Exit. Wait for the sync popup and the rotating fighter toast to pass before pressing keys.
Single Player opens the main menu (Fight, Master and so on). A daily rewards panel may pop up; Escape closes it.
Master, then Practice, leads to Character Select. Each side confirms fighter, costume and accessories with Enter. Loading takes 20 to 45 seconds.
On Character Select the Y button (R with the default keyboard binds; the controller configuration screen tells you yours) opens the game settings: Up and Down move between Time Limit and Difficulty, Left and Right change the value, Escape closes. On Stage Select the same button opens the music menu: Up and Down move through the tracks, Enter picks one, Escape closes.
During a fight, Space opens the pause menu; its Command List reads each move with its input and description, and Q and E switch between the move categories.
In Practice, Escape opens the pause menu, which has tabs (Pause Menu, Dummy Options, Practice Options, Theme). Q and E switch tabs.
To exit the game: on the landing page, press Down until "Exit, 5 of 5", then Enter, then Enter again to confirm.

Shadow Lords:

From the landing page choose Shadow Lords. The first time you play you have to finish the tutorial, which is three turns long and cannot be left early; it includes three fights with lessons. During a lesson, Enter continues a lesson popup and Tab skips a lesson once its "Need Help?" popup has appeared. Enter during a fight does nothing, Space pauses.
The hub is a row of tabs: Left and Right move, Enter opens the tab, Escape asks whether to leave the mode (Tab confirms, Escape cancels).
War Room: Up and Down move through the missions, Enter opens the loadout popup for the mission. In the loadout popup Left and Right move between the fighter slot and the Launch button, Up and Down move between the fighter, consumable and guardian sub-slots, Enter opens the picker for the focused slot. In the consumable and guardian pickers Left, Right, Up and Down move through the items, Q and E switch tiers, Enter equips. Enter on Launch Mission (after a warning popup if nothing is equipped: Enter continues) shows the Fight / Deploy confirmation: Left and Right switch, Enter starts the mission. Escape goes back at every stage.
Emporium: Q and E switch between the Packs, KI Gold, Craft and Storage tabs, Left and Right move through the items, Enter buys or crafts.
Barracks and Spirit Lair: Left and Right rotate the fighters or guardian decks, Enter opens the details.
Popups in this mode often use Tab rather than Enter for their main button; the announcement names the key.

Opponent radar:

During a fight a short pulse plays while the fight clock is running. It is silent on the loading screen, while paused and in every menu.
Pan: the pulse comes from the side the opponent is on, harder the further away.
Rate: the pulse repeats faster the closer the opponent is, from 650 milliseconds at 7 game units or more down to 120 milliseconds at point blank. Fighters start a round 3 units apart; the corners of the stage are about 9 apart.
Pitch: 440 Hz with the opponent on the ground, rising one octave at a height of 2 units, which is about the top of a normal jump.

Settings:

Settings are in kiaccess\kiaccess.ini in the game folder and are read when the game starts.
verbosity: 0, 1 or 2 (see Ctrl+Shift+V).
speech: 0 disables speech output; speech.log is still written.
radar: 1 starts the radar on, 0 off.
radar_volume: 0 to 100.
radar_range, radar_height, radar_min_ms, radar_max_ms, radar_base_hz: tune the radar's distance scale, height scale, pulse rate and base pitch.
log_events: 1 logs every engine event to speech.log (large; for development only).

Safety:

The mod is read-only with respect to the game and its files. It only observes the game's user interface and match state, never writes to them, and sends nothing over the network. The game has no anti-cheat.
