# Speech Output: Prism (chosen) and alternatives

Decision 2026-10-08 (user): use **Prism**, not Tolk. The accessibility-modding community has moved to Prism; it is newer and cross-platform.

## Prism
- Repo: https://github.com/ethindp/prism ("Platform-agnostic Reader Interface for Speech and Messages"). License MPL-2.0. C++23 core with a C ABI (`include/prism.h`), zero external build deps, CMake, also on vcpkg (`vcpkg install ethindp-prism`).
- Unifies the older libraries (Tolk, UniversalSpeech, SRAL, SpeechCore) behind one API and ships a Tolk compatibility shim (`PRISM_ENABLE_TOLK_SHIM`).
- Windows backends (from `PrismBackendId`): NVDA, JAWS, SAPI, OneCore, UIA, ZoomText, ZDSR, System Access, Window-Eyes, PC-Talker, Sense Reader, Boy PC Reader, Xbox Speech. Also AVSpeech/VoiceOver (Apple), Speech Dispatcher/Orca/Spiel (Linux), Android, Web Speech.
- Bindings: Python `prismatoid` (PyPI), .NET `prismatoid` (NuGet), Rust `prismer`, Godot, Tauri. Bindings are described as in progress.
- Used by: Voice of the Old Republic (KOTOR screen-reader mod) via its "Prism speech bridge".

### C API we will call from the mod DLL
```c
PrismConfig   prism_config_init(void);                 // fill defaults, then
PrismContext *prism_init(PrismConfig *cfg);
PrismBackend *prism_registry_create_best(PrismContext*);   // or prism_registry_acquire_best
PrismError    prism_backend_initialize(PrismBackend*);
PrismError    prism_backend_output(PrismBackend*, const char *utf8, bool interrupt); // speech + braille
PrismError    prism_backend_speak(PrismBackend*, const char *utf8, bool interrupt);
PrismError    prism_backend_braille(PrismBackend*, const char *utf8);
PrismError    prism_backend_stop(PrismBackend*);
PrismError    prism_backend_is_speaking(PrismBackend*, bool *out);
const char   *prism_backend_name(PrismBackend*);
void          prism_backend_free(PrismBackend*);  void prism_shutdown(PrismContext*);
```
`PrismConfig` = { uint8 version; PrismRegistry *registry; availability_callback; userdata; poll_interval_ms; debounce_samples; backoff_max_ms; bool auto_power_manage; baseline_callback }. The header has no thread-safety notes; call from one dedicated speech thread to be safe.

### Local status
- `pip install --user prismatoid` (0.17.3) succeeded, but the Python binding needs Python 3.11+ (`typing.Self`); this machine has 3.10. The wheel still ships `prism.dll`, so `05_tools/prism_ctypes.py` wraps the C ABI directly with ctypes and works on 3.10. That is what the Python prototype will use.
- For the shipped C++ DLL: link against Prism via vcpkg or the CMake FetchContent path; redistribute `prism.dll` next to the proxy DLL.

## Alternatives (not chosen)
- **Tolk** (LGPLv3): NVDA/JAWS/SAPI abstraction used by CFC-Access, FFC Access, FalloutNVAccess. Mature but Windows-only and unmaintained upstream; Prism supersedes it and can emulate it via the shim.
- **UniversalSpeech**: used by Sparking Zero Access and AccessMods' UnityAccessibilityLib.
- **NVDA controller client** directly: NVDA-only.
- **SAPI / Windows.Media.SpeechSynthesis**: fallback only; Prism already includes SAPI and OneCore backends.

## OCR fallback (unchanged)
Windows.Media.Ocr for unknown screens; NVDA's own OCR remains what blind KI players use today.

## Design rule
One speech thread with a queue and an interrupt flag; menu navigation calls `output(text, interrupt=true)`; long descriptions use `interrupt=false` after the focus announcement.
