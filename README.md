Killer Instinct Accessibility Mod
v.0.2.1

Purpose:

Adds screen reader output to the menus of Killer Instinct (Steam version, free base game and Anniversary Edition) including the Shadow Lords mode, spoken descriptions of each fighter's appearance, and an opponent radar sound during matches.

Known Issues:

* This is a TEST BUILD. The mod writes kiaccess\speech.log in your Killer Instinct folder each session with every announcement. If anything goes wrong, please attach that file to your bug report.
* Shadow Lords: not read yet are the cards of a pack you open in the Emporium (press Right to turn each card, Escape to leave the pack), the Emporium's purchase and crafting popups, the artifact list in the Barracks, the guardian details and recharge popups in the Spirit Lair, the Archives, the leaderboard and the daily rewards panel. The subtitles of the story cinematics are not spoken either; the cinematics are voiced.
* The Store and online lobbies are not read yet beyond their basic layout. The Store currently announces only "Store, bundles, item 1" and not the item names.
* The fighter appearance descriptions were written from memory of the game and have not yet been reviewed by a sighted person.
* The opponent radar assumes you are Player 1 playing against the CPU. There is no radar for Player 2.
* The opponent radar pauses during the "Ready, Fight" intro of a round and for up to 3 seconds after a round ends. Please keep reporting whether the pulse, its panning and its pitch are useful.
* The fighter appearance descriptions cover the default colour of each costume only; the colour you choose is announced by number.
* Announcements are always in English. The mod reads the game's English text table regardless of the language set in the game.
* For about 10 seconds after the landing page appears, the game shows a sync popup and a "Free Rotating Fighter" toast that swallow key presses. Wait for them to be announced before navigating.
* Only the Steam version of Killer Instinct is supported. The Microsoft Store / Xbox app version is not.
* If a future game update changes the executable so that the mod can no longer find its hook points, the narrator stays silent and writes the reason to kiaccess\speech.log. The game itself still runs normally.

New in this version:

* The hotkeys are no longer held system-wide: they are active only while the game window is in front, so Ctrl+Shift+R and the others are free for other programs when you switch away.
* Fixes from the first beta report (thank you, Sightless Kombat).
* Opponent radar: the pulse no longer drops out in normal fights. The match timer only ticks every two seconds, and the radar mistook the gaps for a pause; it now watches the fighters' movement too.
* Match results read all the stats: Hero, Offense, Defense, Combos and Style with every metric, and the XP earned. Ctrl+Shift+D repeats the stats.
* The game settings menu on Character Select (the Y button, R on a keyboard with the default binds) is read: Time Limit and Difficulty with their values as you change them.
* The music menu on Stage Select (also the Y button) is read: track name, whether it is the current one, and its position. Press Down once after opening it to hear the list.
* Character Select announces each fighter's level and the level of the next unlock.
* The Command List reads each move's description (ender types, Instinct effects, Combo Assist explanations) at normal verbosity, not only at the highest one.
* The stage is announced as a fight loads ("Stage: Shadow Tiger's Lair") whenever it was not picked by name on Stage Select, so a Random pick, Shadow Lords missions and ladder fights tell you where you are.

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
War Room: Up and Down move through the missions, Enter opens the loadout popup for the mission. In the loadout popup Left and Right move between the fighter slot and the Launch button, Up and Down move between the fighter, consumable and guardian sub-slots, Enter opens the picker for the focused slot. Escape goes back.
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
