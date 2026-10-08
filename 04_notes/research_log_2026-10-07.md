# Research Log - 2026-10-07

Method: web search and page fetches only. No downloads, no installs, no game files examined.

## What was searched
- KI Anniversary Edition modding tools, Nexus, Project K, Hitbox Editor
- KI pak unpacker / file formats / reverse engineering
- Cheat Engine tables and trainers (exe name, pointers, version drift)
- Engine, middleware (Scaleform, Wwise, Havok, Bink), DirectX version, anti-cheat
- Steam Deck / Proton behavior (anti-cheat proxy evidence)
- KI's own blind accessibility history (postmortem, SightlessKombat, forum requests, AudioGames.net)
- Screen-reader mods for other games (CFC-Access, Sparking Zero Access, FFC Access, KOTOR, DDLC, Fallout NV)
- Speech libraries (Tolk, UniversalSpeech, NVDA controller, SAPI) and OCR options
- Xbox Accessibility Guidelines

## Pages that refused automated fetch (403) and should be read in a browser
- Nexus Mods pages (Project K, Hitbox Editor, mod list)
- PCGamingWiki Killer Instinct
- SteamDB app pages
- AudioGames.net threads 17990 and 19853
- MobyGames and Giant Bomb credits

## Key conclusions reached today
1. Menus are the only major blind-accessibility gap; in-match audio is already strong.
2. Steam build is Win32 x64, `KillerInstinctX64_r.exe`, DX11, no reported anti-cheat, runs under Proton.
3. UI is Scaleform GFx (community-reported; verify), audio is Wwise (confirmed by postmortem).
4. Community unpacker and hitbox editor exist; no repacker documented.
5. Closest architectural precedent is CFC-Access (external memory reading + Tolk + OCR fallback).
6. Recommended path: external read-only companion first, in-process Scaleform hook only if focus tracking needs it.

## Next actions (require user approval to install anything)
- Approve installing the Steam game and the read-only inspection tools listed in `03_resources/tools_to_evaluate.md`.
- Work through `04_notes/verification_checklist.md` section A and B.
- Join Project K Discord and post on AudioGames.net to find a blind KI tester.
