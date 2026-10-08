# Tools (our own scripts)

Static scripts are pure stdlib and read-only; run them with `python -I`. Scripts marked (frida/capstone) need the user-site
packages, so run them with plain `python`. `G` = `C:\Program Files (x86)\Steam\steamapps\common\Killer Instinct`.

## Static analysis
| Script | Purpose |
|---|---|
| `pe_inspect.py` | Hash, sections, imports, keyword strings of an exe |
| `pak_table.py` / `extract_entry.py` / `peek_entry.py` / `pak_names.py` | `PAK_` v4 table parsing, extraction, peeking |
| `strings_decode.py` | Decode a type-26 localization table to TSV |
| `hash_probe.py` | Proves the table hash is CRC32(lowercase key) |
| `key_harvest.py` | Harvest UPPER_SNAKE keys from sources and resolve them to text |
| `swf_inspect.py` | Inflate CFX movies, list tags, symbols, AS3 strings; writes `.gfx` for FFDec |
| `lua_probe.py` | Entropy stats of the encrypted Lua blobs |
| `xref.py` (capstone) | Which code references given strings |
| `pdata_lookup.py` | RVA -> enclosing function via .pdata |
| `rtti.py` | MSVC RTTI: class list and vtables (`list`, `vtable`) |
| `disasm.py` (capstone) | Annotated disassembly of a range |
| `luabinds.py` (capstone) | Enumerate name->function registrations (Lua bindings, console commands) |
| `vtable_calls.py` (capstone) | Per-slot summary of a vtable: calls, indirect calls, string refs, exact bounds from .pdata |
| `disp_scan.py` (capstone) | Find instructions using a given displacement/immediate (struct offsets), with enclosing function |

## Live (game running)
| Script | Purpose |
|---|---|
| `frida_discover.py <log> [sec]` (frida) | Attach read-only hooks: ExternalInterface dispatcher, invoke queue, localization resolver (+ optional hot vtable hooks gated by `ENABLE_VT_HOOKS`) |
| `frida_focus_probe.py <log> [sec]` (frida) | 20-second probe of Sprite/AvmSprite/AvmTextField vtables for focus frame labels (heavy; superseded) |
| `frida_bt_probe.py <log> [sec]` (frida) | Backtraces from the dispatcher and the `uiInvokeEvent` destructor (found the invoke path) |
| `frida_invoke_path.py <log> [sec]` (frida) | Traces the game wrappers -> `MovieRoot::Invoke`, resolves the real vtable target |
| `frida_focus_invoke.py <log> [sec]` (frida) | **Working C2/C3 probe**: hooks dispatcher + `MovieRoot::Invoke` (all Lua->AS3 JSON) and reads `GetSelectionIndex()` in-process after every scroll. Base of the Phase 1 narrator. Run only one Frida probe at a time. |
| `memscan.py` | Read-only memory search/watch without Frida |
| `sendkeys.py up down enter esc space q e ctrl+shift+r wait:N ...` | Drive the game window with synthesized keys (Escape = back, Space = menu/start, Q/E = LB/RB page tabs; modifiers `ctrl+shift+x` for the narrator hotkeys; 200 ms hold, shorter presses get dropped by popups) |
| `aob_sigs.py <exe>` (capstone) | Unique byte signatures for the six hook RVAs -> `07_dll/sigs.json` + `07_dll/src/sigs.h` |
| `inject.py <dll>` | Development loader: CreateRemoteThread(LoadLibraryW) into the running game (the shipped mod is the dinput8 proxy) |
| `frida_advance_probe.py [sec]` (frida) | Hooks all 71 MovieImpl vtable slots for a few seconds: call rate, thread, and which slot encloses the dispatcher (found `Advance` = slot 24, RVA 0xe4a700) |
| `screenshot.ps1 out.png` | Half-size screen capture for orientation |
| `prism_ctypes.py ["text"]` | Load `prism.dll` via ctypes; prints the best backend; speaks if text given |

FFDec CLI: `"C:\Program Files (x86)\FFDec\ffdec.bat" -export script <outdir> <movie.gfx>` (Java 21 installed under Eclipse Adoptium).
