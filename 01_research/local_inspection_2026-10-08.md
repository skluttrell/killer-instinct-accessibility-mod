# Local Inspection of the Installed Game (2026-10-08)

Everything below was verified against the installed Steam build, read-only, with the scripts in `05_tools/`.
No game files were modified. No third-party tools were installed; only Python 3.10 (already present) was used.

## Install facts

| Item | Value |
|---|---|
| Install path | `C:\Program Files (x86)\Steam\steamapps\common\Killer Instinct` |
| Steam app / build | 577940 (base, free) / buildid 14306144, depot 577941, 52.7 GB on disk |
| Anniversary Edition | app 2625580, sold as DLC on 577940. Same executable and paks; AE only adds entitlements. Confirms that anything built for the free build covers AE. |
| Executable | `KILLERINSTINCTX64_R.EXE`, 43,511,576 bytes |
| SHA-256 | `33bbd291e1a4e280fc07f614fc6a10039c1de7cd2dea33153b0e9c315c31edf6` |
| PE timestamp | 1715178827 = 2024-05-08 (matches the April/May 2024 bonus update) |
| Arch / signing | x64 (PE32+), **not Authenticode signed**, FileVersion 1.0.0.0, CompanyName Microsoft |
| Bundled DLLs | `bink2w64.dll` 2023.08, `PartyWin.dll` 1.7.2302 (PlayFab Party), `PlayFabMultiplayerWin.dll` 1.3.2209, `steam_api64.dll`, `d3dcompiler_47.dll` |

## Imports (what the exe links to)

`DINPUT8.dll` (only `DirectInput8Create`), `XINPUT9_1_0.dll`, `d3d11.dll` (`D3D11CreateDevice`), `dxgi.dll` (`CreateDXGIFactory1`), `D3DCOMPILER_47.dll`, `WINHTTP.dll` (18 funcs), `WS2_32`, `bcrypt`, `CRYPT32`, `steam_api64.dll` (12), `PlayFabMultiplayerWin.dll` (25), `bink2w64.dll` (19), MSVC 2015+ CRT (`MSVCP140`, `VCRUNTIME140[_1]`, `CONCRT140`, UCRT api-sets), `KERNEL32/USER32/GDI32/ADVAPI32/ole32/SHELL32/WINMM`.

Consequences:
- A `dinput8.dll` proxy is the natural auto-load vector (one export to forward). `dxgi.dll` or `d3d11.dll` proxies also work. `version.dll` and `winhttp.dll` are alternatives.
- DirectX 11 only on PC. D3D11 `Present` hook is available if OCR capture is ever needed.

## Anti-cheat / anti-tamper: none found
- No anti-cheat module in the install folder or the import table.
- The 4 "AntiCheat" strings in the exe are `AntiCheatDisconnectionWarningLevelOne/Two/Three` and `AntiCheatDisconnectionMinimumMatches`: the disconnect-penalty ("probation") config, not a scanner.
- `IsDebuggerPresent` appears once (normal CRT usage). No VMProtect/Themida/Denuvo/EAC/BattlEye strings.
- Debug remnants are still present: `windows_pc.cfg` defines developer windows ("Console", "LUA Log Output", "Reactions", "Input", "Fulgore Reactor"), and the exe contains a `lua_debug>` prompt and `CallUILuaScript` console command with CTRL+R / ALT+N / CTRL+F bindings. Whether they can be enabled in the retail build is untested.

## Engine and middleware (confirmed from strings)
- Double Helix in-house engine "HEX" (source paths `D:\Dev\Barracuda\Branches\Steam\HEX\Source\...`, `C:\ki311\HEX\Source\SrcCore\GCore\...`, `P:\Buca\Main\HEX\...`). Project codenames Barracuda / Buca.
- **Scaleform GFx 4.3** with **ActionScript 3** and CLIK (`3rdParty\Scaleform43\Resources\AS3\CLIK`). 2,730 "Scaleform" string hits, `ScaleformRendering.cpp`, `CreateScaleform/LoadScaleform/UnloadScaleform`, `ScaleformLayer_Background/Foreground`, `uiScaleform`, GFx AS3 class tables (`FocusManager`, `FocusEventEx`, `ExternalInterface`, `TextField`...).
- **Wwise** (sound banks under `AUDIO\WWISE\GENERATEDSOUNDBANKS\WINDOWS2022`, 205 `.wem` streams). `gfx_binds.cfg` is literally commented "binds used for Scaleform".
- **Havok** 2014.2.5 (hk2014_2_5_r1_determinism), **Bink 2**, **PlayFab** Party + Multiplayer.
- **Lua** scripting layer: `\x1bLua` bytecode signature present; UI scripts per screen; "Requesting to load ui script: %s"; `CallUILuaScript`. Scripts on disk are encrypted (see below).
- "Meld" (`MELDASSETS\*.xaml`, `Json\MeldRoot.json`): a 2013 Microsoft Studios community-news overlay built as WPF/XAML-style markup. Looks legacy/offline; its JSON carries `labeltext` fields. Low priority.

## Config files (install root)
- `default_pc.cfg`: `map = levels\shell\shell` (the frontend level), `useKeyboardForUI = true`, `streamingInstall = 1`.
- `gfx_binds.cfg`: UI key binds. `COMMAND_UI_SELECT=Enter`, `UI_BACK=Backspace`, `UI_START=Space`, arrows, `[`/`]` tabs, `;`/`'` triggers, numpad 2/4/6/8 right stick. Useful: a companion can drive menus with keyboard for testing.
- `vars_pc.cfg`: `volumehud=0.000000` by default (the blind-player cue slider ships at 0), `subtitles=true`, `customtext=true`, `notext=true`, `aidebug*`, `fps=false`.
- `windows_pc.cfg`: debug window layout including "LUA Log Output".
- No `userdata\<id>\577940` or `%LOCALAPPDATA%\Killer Instinct` folder found yet (settings may be cloud/Xbox-profile based).

## PAK container format (decoded and verified)
87 archives under `PAK\DX11\`. Largest: `INTERFACE4K.PAK` 6.7 GB, `INTERFACE.PAK` 1.9 GB (UI movies + textures), `GLOBAL.PAK` 1.6 GB (gameinfo, scripts, strings, flash sources), `SHELL.PAK` (frontend level), per-character `SPLIT_CHAR_*`, per-stage `STAGE_*`, `patch.pak` (2 entries, overrides).

```
magic   "PAK_"            u32 version = 4
...     header/hash tables (not fully decoded; not needed)
table   N x 262-byte records:
          char name[240]   NUL-terminated, 0xCD padded, backslash paths, NO extension
          u8   flags[4]    e.g. 01 01 01 00 / 01 00 04 00 / 21 00 04 00
          u64  offset      absolute file offset of the data
          u32  size        bytes
          u16  type        extension/type code (see table)
          u16  index       record index
          u16  zero
data    raw, UNCOMPRESSED (offset+size of the last record == file size)
```
`05_tools/pak_table.py` finds records by validating `offset+size <= filesize`; it also admits a few garbage records inside image data (type codes like 43690/21845/65535). Filter by known codes.

Type codes seen:

| code | content | evidence |
|---|---|---|
| 21 | XML text (`<ScreensDefinition>`, `<MissionDialogueDefinition>`, XFL `<DOMDocument>`) | 1,068 in GLOBAL |
| 26 | localization string table (binary, see below) | `gameinfo\strings\strings*`, `gameinfo\keyboard\keyboard_*` |
| 58 | Lua script, **encrypted** | 1,121 in GLOBAL: `interface\script*`, `gameinfo\LuaScript\*` |
| 63 | Scaleform movie, `CFX` = zlib-compressed GFX | 69 in INTERFACE: `interface\flash_season3\swfs\<Screen>` |
| 64 | DDS texture (NVTT, DXT5) | 7,151 in INTERFACE: `<screen>_i<hex>` external images for the movies |
| 9 | CSV frame data ("Attack Name,...") | `gameinfo\*FrameAndMove*` |
| 45/46/55/56 | `EIFF` engine binaries (sounds, shaders) | |
| 29/28 | animation | |
| 2/3/4 | meshes / materials | |

## Localization string table (type 26) - decoded
```
u32 version=1, u32 count, u32 table_off=16, u32 blob_off (=16+8*count)
count x { u32 hash, u32 str_off }   sorted by hash
blob: UTF-8, NUL-terminated, at blob_off+str_off
```
English `gameinfo\strings\strings`: **16,472 strings**, decoded to `data/strings_en.tsv`. Every menu label is present ("Single Player", "Multiplayer", "Help & Options", "Audio", "Audio Options", "Display Options", "Character Select", "Shadow Lords", "Dojo", "Practice", ...). Markup inside strings: `<TT>ABUTTON</TT>`, `<CLR="YELLOW">`, `\n`. The 32-bit hash is of an unknown key string (the game API is `GetLocalizedString` / `GetDisplayStringForKey`); not needed for narration since we read display text, not keys. Keyboard key names are in `keyboard_<locale>` (102 entries).
Other languages: ita, chn, spa, fra, zh-cn, ptb, rus, jpn, ger.

## UI architecture (the important part)
1. `gameinfo\ScreensDefinition.xml` (data/xml/) lists all 60 screens by SWF name: StartScreen, LandingPage, MainMenu, PauseMenu, PracticePauseMenu, CommandList, ControllerConfig, Story, CharacterSelect, Dojo, SinglePlayerLadder, StageSelect, LoadingScreen, OptionsMenu, MatchOutcome, Trials, FightArchiveMain, Leaderboards, FightTitles, Stats, Replays, ComboBreakerOptions, BlockList, Multiplayer{Lobby,Ranked,Exhibition,Ladder}, Store{Main,Fighters,Bundles,Content,ContentOverview}, Shadow{Hub,Fighter,Leaderboards,Notifications,Challenge}, MatchOutcomeShadow, PerfResult_Popup, GA_* (Shadow Lords = "Gargos Assault": WarRoom, HUB, Barracks, Emporium, SpiritLair, PreLoad, MatchRewards, Loadout_Popup, FTUE, Archives, WrapUp, Leaderboard), popups (Toast, Popup, ShadowPopup, StoryMuralPopup, GameSettingsPopup, ShadowFTUE), GenericAssets, Legend, FrontendBackground, PauseBackground.
2. Each screen = one Scaleform movie in INTERFACE.PAK (`interface\flash_season3\swfs\<Screen>`, CFX, AS3) + one Lua script in GLOBAL.PAK (`interface\script_season2|3\<Screen>`, encrypted) + external DDS images.
3. AS3 side (from the DoABC constant pools, listings in `data/as3_listings/`):
   - Screen classes extend `BaseScreen`; shared `SharedClasses` (`BaseButton`, `NavigationButton`, `TextManager`, `TextData`, `CommunicationManager`, `ReferenceManager`, `Utilities`).
   - Menu state lives in AS3: `MainMenu.SelectionIndex`, `MenuEntries`, `MenuStateDictonary`, `mcMenuGroup`, `mcMenuBtn0..4`, `GetSelectionIndex()`, `Navigate()`, `Select()`. `OptionsMenu`: `SelectionIndex`, `MenuEntries`, `MenuToggles`, `mcValue`, `mcSlider`, `mcSliderValue`, `textValues`, `GainFocus/LoseFocus`. `CharacterSelect`: `GetFocusedFighterIndex()`, `FocusedFighterLeft/Right`, `CurrentNavStateName`, `NAVSTATE_FIGHTER_SELECT/COSTUME_SELECT/COLOR_SELECT/...`, `GetFighterName()`, `GetCostumeName()`, `GetColorName()`, `mcP1CharacterName`.
   - Lua -> AS3: the engine Invokes `Lua_*` methods on the screen (`MainMenu.Lua_PopulateDailyLootExpanded`, `CharacterSelect.Lua_Populate`, `Lua_PopulateColorData`, `Lua_UpdateLegend`, `Lua_SetIsOnline`...). Their arguments carry the display data.
   - AS3 -> engine: `flash.external.ExternalInterface.call(...)` via `CommunicationManager.CallUILuaScript`, `CallLuaScreenDisplayComplete`, `CallLuaHideScreen`. The C++ side exposes the handler installed with `ExternalInterface` ("Warning: ExternalInterface.call - handler is not installed").
   - Text is set through `TextManager.SetText` / `TextData`, with `Utilities.ParseCommandText` expanding `<TT>ABUTTON</TT>`-style tokens into button glyphs.
   - DefineEditText fields in the movies are unnamed (empty var names), so TextField *instance* names come from PlaceObject/AS3 (`mcTitle`, `mcValue`...), not the EditText tag.
4. Shadow Lords text is plain XML in GLOBAL.PAK: `gameinfo\AssaultMode\MissionDialogue` (131 KB), `MissionBriefing`, `Dossiers` (150 KB), `Messaging`, `Missions\Missions_*` (copied to `data/xml/`). Also `gameinfo\Subtitles`.
5. Dojo lessons: `gameinfo\LuaScript\TrainingMode\Lessons\Lesson1..72` (encrypted Lua); training prompts go through `AddLocalizedTrainingCommand` / `SetLocalizedTrainingObjective`, so their text is in the string table.
6. Older `interface\flash\*` folders in GLOBAL.PAK are Flash **XFL sources** (DOMDocument.xml + `LIBRARY\movieclips\*.xml` with `tf_menu_1`, `tf_title`, etc.). They are 2013-era (CS6) and may not match the shipping season-3 movies, but they document TextField naming conventions.

## Lua scripts are encrypted
Type-58 blobs have entropy 7.999 bits/byte, sizes that are multiples of 16, no header, and the same script is byte-identical in GLOBAL.PAK and patch.pak (deterministic, no per-file IV visible). The exe contains `\x1bLua` (bytecode loader), zlib, and a handful of `Encrypt/Decrypt` strings; 25 "TEA" hits are ambiguous. Decrypting is a Phase-0 side quest (key is in the exe), not a requirement: the AS3 side already exposes the UI contract.

## What this changes for the mod design
- The UI is fully data-driven and introspectable: 60 known screens, every label in a decodable string table, AS3 classes with explicit `SelectionIndex` / focus state, and a single chokepoint (`ExternalInterface` handler + `Movie::Invoke`) through which all menu navigation and population flows.
- In-process hooking of the Scaleform boundary (Option 2) is now the strongest candidate, because the hook points are generic GFx 4.3 API calls rather than per-screen reverse engineering.
- The read-only external approach (Option 1) remains viable for a prototype: AS3 strings (UTF-8) such as "mcMenuBtn0" and the current label text are in process memory and can be located by scanning.
