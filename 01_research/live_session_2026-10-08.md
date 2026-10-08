# Live Session Findings (Phase 0 C) - 2026-10-08

Tools used: Frida 17.23 (pip, user install), Capstone, JPEXS FFDec 26.3 (winget) with Temurin JRE 21, own scripts in `05_tools/`.
The game was launched through Steam by the assistant, ran windowed at 1920x1080, signed in automatically, and was closed afterwards.
All hooks were read-only (`Interceptor.attach` logging); no memory was written.
Build: 14306144, exe SHA-256 `33bbd291...edf6`, image base 0x140000000 (no ASLR relocation observed; always compute from the module base anyway).

## 1. Decompiled ActionScript confirms the UI contract
FFDec exported 250 `.as` files for MainMenu, OptionsMenu and CharacterSelect (scratchpad `as3_src/`, regenerate with `ffdec.bat -export script`).
- `BaseScreen` (every screen): engine calls `ExternalLoadComplete()`, `ExternalAdded()`, `ExternalButtonDown(btnType, controllerID)` with `NavigationCode` strings `"up" "down" "left" "right" "a" "b" "x" "y" "start" "back" "lb" "rb" "lt" "rt"`.
- `CommunicationManager.CallUILuaScript(screen, luaFunction, json)` = `ExternalInterface.call("CallUILuaScript", scriptName, luaFunction, jsonString)`. Script name = SWF name minus `.swf`, except CharacterSelect -> `CharSelectMenu`, Popup/ShadowPopup -> `MessageBox`.
- `TextManager.SetText({ref, key})` sets `textField.text = key` directly. Keys arriving from Lua are already display text; literal AS3 keys (e.g. `COMMON_SEASON_2`) are resolved by Lua beforehand or shown via `&`-prefixed literals. A leading `&` marks literal (untranslated) text everywhere in the JSON payloads.
- `MainMenu`: `SelectionIndex` (private), `MenuEntries` (5 buttons `mcMenuGroup.mcMenuBtn0..4.mcBtn`), `Navigate(amt)` wraps and calls `CallUILuaScript("MainMenu.swf","CheckIsDestInstalled", options[i].nextState)` on every cursor move; `Select()`, `Back()`, `GetSelectionIndex()` public.
- `OptionsMenu`: 7 `mcBtn0..6` entries plus 7 toggles (`OptionsMenuToggle` with `mcValue`, `mcSlider`); `Navigate` only plays `PlayScroll` through Lua (no index is sent). `Populate` receives `States{name:{Entries[{key,type,nextState...}],Title,Legend,toggle}}`.
- `CharacterSelect`: `FighterSelect.GetFocusedFighterIndex(side)`, `DataProvider.GetFighterName(i)`, nav states `NAVSTATE_*`, `Lua_Populate(data)` with the full roster.

## 2. Localization key hash = CRC32 of the lowercased key
`crc32(key.lower())` matched 9/9 known keys (`05_tools/hash_probe.py`). `05_tools/key_harvest.py` resolved 2,246 key->text pairs into `data/strings_en_by_key.tsv` from identifiers found in AS3, the exe and XML.

## 3. Engine functions located (RVAs, this build)
| RVA | What | Evidence |
|---|---|---|
| 0x5dfe50 | **ExternalInterface dispatcher** for `CallUILuaScript`: `rcx=?, rdx=?, r8=GFx::Value[] (stride 0x30), r9=argc`. Value: type at +0x18 (`&0x8f`: 1 null, 2 bool, 3 int, 4 number, 6/7 string; bit 0x40 = indirect pointer), payload at +0x20. | registered next to the "CallUILuaScript" string at 0x55b9c2; fires on every AS3 -> Lua call |
| 0x5d9680 | Lua binding `QueueInvokeEventToWindow(windowId, nameHash, const char* swf, const char* func, const char* json)` | `data/lua_bindings.tsv`; JSON observed |
| 0x5d97d0 | `QueueInvokeEventToWindowTblFunc(...)` same but a Lua table instead of JSON (used for `MainMenu.Populate`) | |
| 0x65a950 | `uiInvokeEvent` constructor | called from both |
| 0x1a9640 | `LocalizationManager::GetString(this, uint32 hash) -> const char*` (returns "missing text" on miss) | Lua bindings `GetLocalizedString` 0x5e6470 / `GetDisplayStringForKey` 0x5e64a0 -> 0x1a97e0 |
| 0x1a8fa0 | localization manager singleton getter | |
| 0x1212430 / 0x1212410 / 0x1212350 / 0x1212220 / 0x1212370 | lua_pushstring / pushnumber / pushinteger / pushboolean / pushnil | used by the dispatcher |
| 0x1213040 | lua_type (5 = table, 6 = function) | |
| 0x1216410 | lua_call/pcall | |
| 0x4c8720, 0x4c7120, 0x1e8870, 0x12160b0 ... | Lua binding registrars (`name, func`) - 3,007 registrations listed in `data/lua_bindings.tsv` | `05_tools/luabinds.py` |
| 0x203dd38 | `Scaleform::GFx::MovieImpl` vtable (71 slots) | `05_tools/rtti.py` |
| 0x20a6768 | `AS3ValueObjectInterface` vtable (40) | |
| 0x20a2928 | `AS3::MovieRoot` vtable (40) | |
| 0x2048d70 | `GFx::Sprite` vtable (120); 0x20a4d50 `AvmSprite`; 0x20a78b0 `AvmTextField` | |
RTTI: 2,754 classes in `data/rtti_classes.txt`. No `Translator`, `ExternalInterface` or `FSCommandHandler` subclass exists; the game's UI classes live in namespace `UI` (`uiScaleform`, `uiWindow`, `uiInvokeEvent`, `uiInvokeEventWithTable`, `ScreenText`, ...).

## 4. What the live hooks showed
Hooking just three functions (0x5dfe50, 0x5d9680/0x5d97d0, 0x1a9640) gives a complete, low-overhead picture (game stays at 60 FPS):

AS3 -> Lua (`ei` events), examples captured:
```
['LandingPage', 'RequestMainMenuTransition', '{"InitialState":"Multiplayer","ScreenName":"MainMenu.swf"}']
['uiScreenUtil', 'LoadDestination', '{"Destination":"MainMenu.swf","Current":"LandingPage.swf"}']
['MainMenu', 'ScreenShown']
['MainMenu', 'CheckIsDestInstalled', '{"ScreenName":"StoreMain.swf",...}']   <- cursor moved to "Store & Collection"
['MainMenu', 'CheckIsDestInstalled', '"MultiplayerFight"']                     <- cursor moved to "Fight"
['uiSounds', 'PlayScroll'] / ['uiSounds', 'PlayBack'] / ['MessageBox', 'CommandSelect', '']
['MainMenu', 'AS_GetDailyLootExpandedData', '']  (daily-loot panel opened)
```
Lua -> AS3 (`qinv` events), examples:
```
MainMenu.swf PopulateTicker {"motdString":"&TUSK is now the FREE rotating fighter in Killer Instinct!  FIGHT ON!!!",...}
LandingPage.swf PopulateMOTD {"Message":"&...","Title":"&Free Rotating Fighter!"}
MainMenu.swf Populate (table variant, payload not decoded yet)
MainMenu.swf Lua_PopulateDailyLootContracted {"resetText":"RESET","progress":"&0/4",...}
MainMenu.swf PopulatePlayerCards {"Player1Info":{"PlayerName":"&<gamertag>", "Expanded":{"Entries":[{"Title":"&Aganos - Lvl 1",...
GenericAssets.swf SetPlayerDeviceIconVisibility {...}
FrontendBackground.swf SetBackgroundForScreen {"FrameLabel":"MainMenu"}
```
Localization (`loc` events) while MainMenu populated, in order: `... "Season 2", "Multiplayer", "Single Player", "Store", "Lvl %s", "Qualifier", "Select", "Back", "Fight", "Challenge Human Opponents", "Store & Collection", "View & Equip your Unlocks. Or Buy New Ones!", "Fight Archive", "View your Fight Profile and Stats", "Controls, Audio, Credits and More"`. Daily loot panel: `"NEXT REWARD AVAILABLE IN", "Day %d", "Back", "Craft an item in Shadow Lords", ...`. Many lookups return "missing text" (optional keys).
Screenshots confirmed: Combo Assist Info popup (Enter = "Got it, thanks!"), Daily Loot panel, Multiplayer main menu with Fight / Store & Collection / Fight Archive / Help & Options.

Keyboard facts: Enter = select, **Escape = back** (Backspace does nothing in menus despite `gfx_binds.cfg`), Space = start, arrows navigate. `05_tools/sendkeys.py` drives the window reliably after an `AttachThreadInput` foreground fix.

## 5. What is still missing for narration
- A generic "which item has focus" signal for screens whose `Navigate` does not call Lua (OptionsMenu, lists). MainMenu leaks it through `CheckIsDestInstalled`. Candidates, in order: (a) in-process `GFx::Value::Invoke("GetSelectionIndex")` on the screen root after each `PlayScroll`; (b) hook the AS3 VM `gotoAndPlay` implementation and watch for the `"GainFocus"` label (two vtable probes on `Sprite`/`AvmSprite`/`AvmTextField` with raw and boxed string reads did not catch it; the label path is a non-virtual call, and the error string "Frame label {0} was not found in scene {1}." has no direct code xref); (c) decode `uiInvokeEventWithTable` to get `Populate` tables and keep our own index by counting `up/down` `ExternalButtonDown` events (fragile on wrap).
- Decoding the Lua-table variant of invoke events (needs `lua_next`/`lua_tolstring`, which are in the 0x1212000-0x1217000 range; identify by hooking the dispatcher's calls).
- Hot vtable hooks (MovieImpl, AS3 object interface, Sprite) cost ~55 FPS; never ship those. The three cheap hooks are fine.

## 6. Prism
`prism.dll` (from the `prismatoid` 0.17.3 wheel) loads via ctypes on Python 3.10 (`05_tools/prism_ctypes.py`), `prism_registry_create_best` picked **NVDA** on this machine. The Python binding itself needs Python 3.11+.

## 7. Engine -> AS3 invoke path, focus query, table payloads (third session, C2 + C3 solved)
Found with `frida_bt_probe.py` (backtraces from the dispatcher and the `uiInvokeEvent` destructor), `vtable_calls.py`, `disp_scan.py`, and the decompiled host movie.

**Invoke path.** `uiInvokeEvent` (0xb8 bytes; DHStd strings swf at +0x40, func at +0x68; vtable 0x17d0ac8 holds only type getters, dtor slot 7 = 0x668660, clone slot 8) is appended to the list at uiManager (`[rva 0x27d5318]`) + 0xbd0 and drained by a job on the UI thread. Handler 0x998910 joins it as `"swf|func|json"`, splits on `|` into string GFx::Values and calls wrapper 0x1d6b30 -> 0x3c4fe0 (owner-thread check at wrapper+0x48, off-thread calls are deferred) -> thunk 0xe47040 -> **`AS3::MovieRoot::Invoke(root, "root.Invoke", NULL, Value[3], 3)` = RVA 0x1122ef0** (vtable 0x20a2928 slot 57). Other calls seen: `root.LoadSWF`, `root.DisplaySWF`, `root.RemoveSWF`, `root.SetSWFLevel`, `root.SetSWFLevelToTop`, `root.SetActiveSWF`.

**Host movie** = `ForegroundShell.swf` (INTERFACE.PAK; class `ShellBase`, listing in `data/as3_listings/ForegroundShell.txt`). `Invoke(swf, func, json)` looks the screen up through `LoadManager.GetSWFRefFromString(swf)` and calls `screen[func](JSONUtils.decodeJSON(json))`; it returns void and queues the message if the screen is still loading. Also public: `GetActiveSWF():String`, `GetSWFRefFromString(swf):MovieClip`, `GetActiveSWFRef()`.

**C3.** Hooking 0x1122ef0 shows every engine -> UI call as (swf, func, JSON), including the Lua-table variant: `MainMenu.Populate` arrives as JSON with `States.<state>.options[{key, description, debugText, nextState}]`, `QuickNav`, `progress`, `BuildNumber`. `OptionsMenu.Populate` carries `States.<state>.Entries[{key, descKey, nextState, type, value, textValues}]`, `Title`, `Legend`. Keys are localization keys (resolve with CRC32 table, `data/strings_en_by_key.tsv`); `&` prefix = literal. So hook 0x1122ef0 instead of 0x5d9680/0x5d97d0.

**Buttons never pass through the engine.** No `ExternalButtonDown` invoke exists; screens receive GFx key events directly (AS3 `ReferenceManager` / `IInputDeviceListener`) and run `Navigate` themselves. That is why there is no engine-side focus signal.

**C2: focus query.** On the UI thread, inside the 0x5dfe50 hook after `PlayScroll` / `CheckIsDestInstalled` / `ScreenShown` (re-entrant Invoke from the ExternalInterface callback was stable for 10+ minutes):
1. `MovieRoot::Invoke(root, "root.GetSWFRefFromString", &ref, Value[1]{swf}, 1)` -> `ref.Type == 0x4a` (managed DisplayObject)
2. `AS3ValueObjectInterface::Invoke(ref.pObjectInterface, ref.pData, &res, "GetSelectionIndex", NULL, 0, isdobj=1)` = vtable 0x20a6768 slot 6 = **RVA 0x1150a00** -> `res.Type == 3`, int at +0x20
3. `Value::Release(&ref)` = RVA 0x350bd0 (ctor/zero-init = 0x350bc0).
`root = [[[rcx of 0x5dfe50] + 0x18] + 0x18]` (check vtable == 0x20a2928; rcx is the foreground shell's uiMovie wrapper). When no transition has been seen yet, `root.GetActiveSWF` returns the active screen name.
Verified live: MainMenu 0..3 agreeing with the `CheckIsDestInstalled` destinations; OptionsMenu 2 -> 3 -> 2 on down/down/up (AS3-only navigation); LandingPage 2 = Multiplayer. Cost: under 1 ms per query (`ms: 0/1` in `data/frida_focus_invoke5.log`). CharacterSelect exposes `FighterSelect.GetFocusedFighterIndex(side)` instead; same mechanism with one int argument.

**GFx::Value in this build** = 0x30 bytes: +0x10 `pObjectInterface`, +0x18 `Type` (0 undefined, 1 null, 2 bool, 3 int, 4 uint, 5 number, 6 string, 7 wstring, 8 object, 9 array, 0xA display object, |0x40 managed, |0x80 convert), +0x20 payload (`const char*` for an unmanaged string).

**Gotchas.** Synthesized key presses need about 200 ms hold (60 ms presses were dropped by popups); `sendkeys.py` default changed. With Frida 17, when a second session attached to the same process detaches, the first session's hooks stop firing: run one probe at a time. The Combo Assist popups appear on the first main-menu visit after launch; Enter dismisses each.
