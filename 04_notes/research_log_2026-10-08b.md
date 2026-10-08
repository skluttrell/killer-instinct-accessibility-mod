# Research Log - 2026-10-08 (second session: tools approved, live hooking)

User decisions this session: switch speech library from Tolk to **Prism**; tool installs approved provided the assistant can run them unattended (Cheat Engine excluded: GUI-only and not accessible to the user either).

## Installed
- pip --user: frida 17.23.0, frida-tools 14.11.0, prismatoid 0.17.3 (ships prism.dll). capstone 5.0.9 was already present.
- winget: JPEXS.FFDec 26.3.0, EclipseAdoptium.Temurin.21.JRE.
- Not installed: Cheat Engine, x64dbg, System Informer.

## Done
1. FFDec CLI export of MainMenu/OptionsMenu/CharacterSelect ActionScript (250 files); read MainMenu, OptionsMenu, BaseScreen, CommunicationManager, TextManager, MainMenuButton, Utilities, NavigationCode.
2. Hash function for the string table identified: CRC32(lowercase key). 2,246 keys resolved (`data/strings_en_by_key.tsv`).
3. Static analysis scripts added: `xref.py` (Capstone string xrefs), `pdata_lookup.py`, `rtti.py` (vtables), `disasm.py`, `luabinds.py` (3,007 name->function registrations), results in `data/`.
4. Located the ExternalInterface dispatcher, the Lua->UI invoke queue, the localization resolver and Lua API helpers (table in `01_research/live_session_2026-10-08.md`).
5. Launched the game via Steam, attached Frida, drove menus with synthesized keys (`sendkeys.py`), took screenshots (`screenshot.ps1`), captured AS3<->Lua traffic and localized text in real time (`data/frida_session*.log`). Game closed afterwards.
6. Verified `prism.dll` works from Python 3.10 via ctypes and selects NVDA.
7. Two 25-second vtable probes for a generic focus signal (Sprite/AvmSprite/AvmTextField) found nothing; recorded as open item C2.

## Lessons
- Frida 17 removed `Module.findBaseAddress`; use `Process.findModuleByName(...).base`.
- `readUtf8String(n)` reads exactly n bytes; use `readUtf8String()` for NUL-terminated strings.
- Hooking per-frame Scaleform vtables drops the game to ~2 FPS and makes key presses look lost; keep hooks to the three cheap functions.
- Escape is "back" in menus; Backspace is not.
- `python -I` ignores user-site packages (frida/capstone); run those scripts with plain `python`.

## Next
- C2 focus index (try `GetSelectionIndex` via GFx Value in-process; or hook the AS3 VM method-call path for `Activate`).
- Build the Python prototype narrator on top of `frida_discover.py` + `prism_ctypes.py`.
