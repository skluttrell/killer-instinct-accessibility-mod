# Modding Resources for Killer Instinct (PC / Steam)

Status: web research only, 2026-10-07. Nothing downloaded or installed.

## Community hubs

| Resource | What it offers | Notes |
|---|---|---|
| Nexus Mods - Killer Instinct | ~11 mods: Project K, Green Screen Stage, Season 3 Build (blocking removed), Hitbox Editor Tool | Pages returned HTTP 403 to automated fetch; browse manually. https://www.nexusmods.com/games/killerinstinct/mods |
| Project K Discord | Downloads and the most active KI modding community (gameplay overhaul mod) | Invite was posted on X by @rrurururu1. Best place to ask about file formats and loaders. https://www.nexusmods.com/killerinstinct/mods/2 |
| Ultra-Combo forums (official) | Official community. Mod *ideas* allowed; mod links/tutorials are explicitly banned by moderators | Good for accessibility advocacy, bad for technical mod talk. https://forums.ultra-combo.com |
| AudioGames.net forum | Blind player threads on KI menus, OCR workflows, PC questions | Threads 17990 (menu guide) and 19853 (PC questions). https://forum.audiogames.net |

## Tools (not installed)

### KIUnpacker / ".pak Unpacker"
- Author: thethiny. C++ tool that extracts the game's `.pak` archives into folders plus a `.txt` file listing.
- Repo: https://github.com/thethiny/Modding-Database (branch `Killer-Instinct`, folder `Killer Instinct/.pak Unpacker`). GPL-3.0.
- Release thread: https://forums.ultra-combo.com/t/killer-instinct-pc-file-extractor-release/13072
- Only unpacks. No repacker documented. Compression details not documented in the README. QuickBMS was mentioned as a prior alternative.
- Why it matters for us: lets us find the Scaleform `.gfx`/`.swf` UI movies and localization string tables inside the paks, which is where menu labels live.
- Unknown: whether it still works on Anniversary Edition paks (the tool predates AE by years).

### Killer Instinct Hitbox Editor Tool
- Author "Luke", Nexus mod #10, last updated 2026-03-19. Desktop editor for `.EVF` files (character move/hitbox data) and "ZIP-based move data". Exports to JSON and rebuilds.
- Proves the AE-era data formats have been reverse engineered by someone active this year. Author is a likely contact for pak/format questions.
- https://www.nexusmods.com/killerinstinct/mods/10

### Project K
- Gameplay overhaul (all characters buffed, playable Boss Gargos, updated UI sounds/visuals). Works on "Steam/Win10".
- Significant because it modifies UI assets, so its authors have handled the UI file formats.
- https://www.nexusmods.com/killerinstinct/mods/2

### Cheat Engine tables and trainers
- FearLess Revolution "Killer Instinct (Steam)" table: health, shadow, instinct, AI health. https://fearlessrevolution.com/viewtopic.php?t=5040
- FearLess Revolution "Killer Instinct Anniversary - Free Camera and stuffs" (2024): targets `KillerInstinctX64_r.exe`, camera, health, game speed. Author later found their build was the 2017 Steam version; table broke on other versions with "bytes at expected location are not what was expected" assertion errors, i.e. AOB signatures need re-finding per version. https://fearlessrevolution.com/viewtopic.php?t=28944
- CheatHappens / MrAntiFun / CheatEvolution trainers exist for Steam build `v3.9.39060.1.289149.r` etc.
- Why it matters: confirms external memory reading of the process is practical and that nobody reports anti-cheat interference.

## Save / config locations (Steam, original app 577940)
- `<Steam>\userdata\<user-id>\577940\` holds config and save data (per Steam discussions).
- Install folder contains `Fightreel\` and `Trailer\` video folders alongside the paks.

## Steam app IDs
- 577940 = Killer Instinct (base app, now free-to-play shell).
- 2625580 = Killer Instinct: Anniversary Edition (sold as DLC on top of 577940; all prior Steam owners upgraded free).
- Implication: the executable and depots belong to 577940; AE is content unlock. Mods for "Steam KI" generally target the same binary lineage.

## Historical notes relevant to modding
- 2017 Steam release moved KI from UWP to Win32, which is what made DLL injection and file mods possible at all. (Ultra-Combo thread 22652.)
- 2023-11-28 Anniversary Edition shipped (Iron Galaxy). Matchmaking migrated to PlayFab. Xbox got DX12; PC still lists DX11 minimum.
- 2024-04 bonus update (new stage, colorways). No later feature patches found; assume the game is in maintenance mode. Good for mod stability (addresses stop moving), bad for official narration.
