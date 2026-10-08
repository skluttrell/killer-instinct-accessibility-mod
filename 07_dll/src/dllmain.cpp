// kiaccess - Killer Instinct accessibility narrator, Phase 2 in-process DLL.
// Loaded either as the dinput8.dll proxy (copy next to KILLERINSTINCTX64_R.EXE as dinput8.dll) or injected for
// development (05_tools/inject.py). Read-only with respect to the game: three MinHook detours, the GFx::Value API
// called on the UI thread, Prism for speech. Files live in <dll dir>\kiaccess\ (kiaccess.ini, prism.dll, data\, speech.log).
#include "common.h"
#include "gfx.h"
#include "hooks.h"
#include "narrator.h"
#include "snapshot.h"
#include "speech.h"
#include "strings.h"

HMODULE g_thisModule = nullptr;

namespace ki {

static Strings* s_strings = nullptr;
static Narrator* s_nar = nullptr;
static HANDLE s_hotkeyThread = nullptr;
static DWORD s_hotkeyThreadId = 0;
static bool s_initialized = false;

// ---- hotkeys: Ctrl+Shift+R repeat, D description, T ticker, V verbosity, Q toggle narration, U unload (development) ----
enum { HK_REPEAT = 1, HK_DESC, HK_TICKER, HK_VERBOSITY, HK_TOGGLE, HK_UNLOAD };
static void unloadSelf();

static DWORD WINAPI hotkeyThread(LPVOID) {
    const std::pair<int, int> keys[] = {{HK_REPEAT, 'R'}, {HK_DESC, 'D'}, {HK_TICKER, 'T'}, {HK_VERBOSITY, 'V'}, {HK_TOGGLE, 'Q'}, {HK_UNLOAD, 'U'}};
    for (auto& k : keys)
        if (!RegisterHotKey(nullptr, k.first, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, k.second)) logLine("hotkey registration failed: " + std::to_string(k.first));
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message != WM_HOTKEY) continue;
        std::lock_guard<std::recursive_mutex> g(g_lock);
        switch ((int)msg.wParam) {
            case HK_REPEAT: s_nar->repeat(); break;
            case HK_DESC: s_nar->read_desc(); break;
            case HK_TICKER: s_nar->read_ticker(); break;
            case HK_VERBOSITY: s_nar->cycle_verbosity(); break;
            case HK_TOGGLE: {
                bool on = !hooks::enabled();
                hooks::setEnabled(on);
                s_nar->say(on ? "narrator on" : "narrator off", true);
                break;
            }
            case HK_UNLOAD:
                s_nar->say("narrator unloading", true);
                Sleep(300);
                unloadSelf();
                return 0;
        }
    }
    for (auto& k : keys) UnregisterHotKey(nullptr, k.first);
    return 0;
}

static DWORD WINAPI initThread(LPVOID) {
    CreateDirectoryW(dataDir().c_str(), nullptr);
    loadConfig();
    logLine("=== kiaccess starting ===");
    s_strings = new Strings();
    if (!s_strings->load(dataDir() + L"data\\strings_en.tsv")) logLine("warning: data\\strings_en.tsv missing, keys will be spoken raw");
    else logLine("strings: " + std::to_string(s_strings->size()));
    s_nar = new Narrator(*s_strings);
    s_nar->setVerbosity(g_cfg.verbosity);
    s_nar->loadData();
    if (g_cfg.speech) {
        if (speech::init(dataDir() + L"prism.dll")) logLine("speech backend: " + speech::backendName());
        else g_cfg.speech = false;
    }
    if (!hooks::resolveAddresses()) { logLine("hook addresses unresolved: narrator disabled"); return 1; }
    snap::init();
    if (!hooks::install(s_nar)) { logLine("hooks not installed: narrator disabled"); return 1; }
    logLine("hooks installed");
    s_hotkeyThread = CreateThread(nullptr, 0, hotkeyThread, nullptr, 0, &s_hotkeyThreadId);
    s_initialized = true;
    s_nar->say("Killer Instinct narrator ready", true);
    return 0;
}

static void shutdown() {
    if (!s_initialized) return;
    s_initialized = false;
    hooks::uninstall();
    speech::shutdown();
    logLine("=== kiaccess stopped ===");
}

static DWORD WINAPI unloadThread(LPVOID) {
    shutdown();
    if (s_hotkeyThreadId) PostThreadMessageW(s_hotkeyThreadId, WM_QUIT, 0, 0);
    Sleep(200);
    FreeLibraryAndExitThread(g_thisModule, 0);
}

static void unloadSelf() { CreateThread(nullptr, 0, unloadThread, nullptr, 0, nullptr); }

}  // namespace ki

// ---- dinput8.dll proxy: the game imports only DirectInput8Create ----
typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
static HMODULE s_realDinput = nullptr;
static DirectInput8Create_t s_realCreate = nullptr;

static bool loadRealDinput() {
    if (s_realDinput) return true;
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring p = std::wstring(sys) + L"\\dinput8.dll";
    s_realDinput = LoadLibraryW(p.c_str());
    if (!s_realDinput) return false;
    s_realCreate = (DirectInput8Create_t)GetProcAddress(s_realDinput, "DirectInput8Create");
    return s_realCreate != nullptr;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    if (!loadRealDinput()) return E_FAIL;
    return s_realCreate(hinst, version, riid, out, outer);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_thisModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, ki::initThread, nullptr, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        // reserved != NULL means the process is terminating: other threads are already gone, do not unhook or join
        if (reserved == nullptr) ki::shutdown();
        else ki::logLine("=== game exiting ===");
    }
    return TRUE;
}
