# Research Log - 2026-10-08 (sixth session: Phase 2, the C++ DLL)

User decision: Phase 2 now; verbosity 1 is fine as a default (labels are short).

## Done
1. `07_dll/`: `kiaccess.dll` = direct port of `06_narrator` (agent.js -> `gfx.cpp` + `snapshot.cpp`, narrator.py -> `narrator.cpp`
   on nlohmann::json). MinHook detours on the dispatcher (0x5dfe50), `MovieRoot::Invoke` (0x1122ef0) and
   `MovieImpl::Advance` (0xe4a700); exe SHA-256 gate with fixed RVAs, otherwise AOB scan (`05_tools/aob_sigs.py` -> `src/sigs.h`,
   all six signatures unique). Prism through a speech worker thread; global hotkeys on their own thread; `dinput8.dll` proxy export.
   Build: MSVC 14.29 x64 via `build.ps1`; deps: MinHook (git), nlohmann/json 3.11.3.
2. Development loop without restarting the game: `05_tools/inject.py` + Ctrl+Shift+U unload.
3. Verified live by injection on the same screens as session 5 (log format identical, 0-1 ms hook-to-speech), then installed
   as `dinput8.dll` + `kiaccess\` in the game folder and verified from a cold Steam start (DLL up 2 s after launch, hooks
   installed, landing page narrated). Clean game exit logs `=== game exiting ===` without a crash.

## Gotchas found today
- **GFx managed Values must not move.** The engine tracks a managed `GFx::Value` by its address (ObjectInterface::ObjectAddRef/Release);
  my first `readPath` memcpy'd a Value into another slot before releasing it and the game crashed inside `ValueRelease`
  (+0x17: dereferencing the stale object interface). Fixed: one fixed slot per path segment, released in place.
  The Frida agent never hit this because every Value lived at a fixed heap address.
- `DLL_PROCESS_DETACH` with `lpReserved != NULL` (process exit) must not unhook or join threads.
- The earlier game crash in `frida-agent.dll` (15:46) was the user choosing Exit while the Frida hooks were installed:
  a Frida teardown fault, not a reader bug.
- Keys sent too early after "Loading" go into the match as gameplay input; the practice menu needs ~25 s after the loading screen.
- `nlohmann::json` rejects raw control characters in strings: popup bodies are re-parsed with newlines escaped.

## Next
- User test with NVDA (installed and running now).
- Store / lobbies / Shadow Lords readers; first-run generation of `strings_en.tsv` from the PAK; packaging.
