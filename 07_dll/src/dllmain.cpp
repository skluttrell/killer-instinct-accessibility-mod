// kiaccess - Killer Instinct accessibility narrator, Phase 2 in-process DLL.
// Loaded either as the dinput8.dll proxy (copy next to KILLERINSTINCTX64_R.EXE as dinput8.dll) or injected for
// development (05_tools/inject.py). Read-only with respect to the game: three MinHook detours, the GFx::Value API
// called on the UI thread, Prism for speech. Files live in <dll dir>\kiaccess\ (kiaccess.ini, prism.dll, data\, speech.log).
#include "common.h"
#include "gfx.h"
#include "hooks.h"
#include "narrator.h"
#include "pak.h"
#include "radar.h"
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

// ---- hotkeys: Ctrl+Shift+R repeat, D description, A fighter appearance, T ticker, V verbosity, Q toggle narration, U unload (development) ----
enum { HK_REPEAT = 1, HK_DESC, HK_APPEARANCE, HK_TICKER, HK_VERBOSITY, HK_TOGGLE, HK_RADAR, HK_UNLOAD };
static void unloadSelf();

// The hotkeys are system-wide while registered, which would take Ctrl+Shift+R and friends away from every other program
// for as long as the game runs (user request 2026-10-10). They are therefore registered only while a window of this
// process is in the foreground, checked every 200 ms, and released as soon as the player switches away.
static bool gameHasFocus() {
    DWORD pid = 0;
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static DWORD WINAPI hotkeyThread(LPVOID) {
    const std::pair<int, int> keys[] = {{HK_REPEAT, 'R'}, {HK_DESC, 'D'}, {HK_APPEARANCE, 'A'}, {HK_TICKER, 'T'}, {HK_VERBOSITY, 'V'}, {HK_TOGGLE, 'Q'}, {HK_RADAR, 'P'}, {HK_UNLOAD, 'U'}};
    bool registered = false;
    auto setRegistered = [&](bool on) {
        if (on == registered) return;
        for (auto& k : keys) {
            if (on) { if (!RegisterHotKey(nullptr, k.first, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, k.second)) logLine("hotkey registration failed: " + std::to_string(k.first)); }
            else UnregisterHotKey(nullptr, k.first);
        }
        registered = on;
    };
    MSG msg;
    while (true) {
        DWORD w = MsgWaitForMultipleObjects(0, nullptr, FALSE, 200, QS_ALLINPUT);
        if (w == WAIT_TIMEOUT) { setRegistered(gameHasFocus()); continue; }
        if (!PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) continue;
        if (msg.message == WM_QUIT) break;
        if (msg.message != WM_HOTKEY) continue;
        std::lock_guard<std::recursive_mutex> g(g_lock);
        switch ((int)msg.wParam) {
            case HK_REPEAT: s_nar->repeat(); break;
            case HK_DESC: s_nar->read_desc(); break;
            case HK_APPEARANCE: s_nar->read_appearance(); break;
            case HK_TICKER: s_nar->read_ticker(); break;
            case HK_VERBOSITY: s_nar->cycle_verbosity(); break;
            case HK_RADAR: {
                bool on = !radar::enabled();
                radar::setEnabled(on);
                s_nar->say(on ? "radar on" : "radar off", true);
                break;
            }
            case HK_TOGGLE: {
                bool on = !hooks::enabled();
                hooks::setEnabled(on);
                s_nar->say(on ? "narrator on" : "narrator off", true);
                break;
            }
            case HK_UNLOAD:
                s_nar->say("narrator unloading", true);
                Sleep(300);
                setRegistered(false);
                unloadSelf();
                return 0;
        }
    }
    setRegistered(false);
    return 0;
}

// The localization TSV is generated from the game's own PAK on first run (and again after a game update changes GLOBAL.PAK)
// so the mod never ships the game's text.
static bool stringsTsvStale(const std::wstring& tsv, const std::wstring& pak) {
    WIN32_FILE_ATTRIBUTE_DATA a{}, b{};
    if (!GetFileAttributesExW(tsv.c_str(), GetFileExInfoStandard, &a)) return true;          // missing
    if (!GetFileAttributesExW(pak.c_str(), GetFileExInfoStandard, &b)) return false;         // no PAK (injected into something else): keep what we have
    return CompareFileTime(&b.ftLastWriteTime, &a.ftLastWriteTime) > 0;                      // PAK newer than TSV
}

static DWORD WINAPI initThread(LPVOID) {
    CreateDirectoryW(dataDir().c_str(), nullptr);
    loadConfig();
    logLine("=== kiaccess starting ===");
    const std::wstring tsv = dataDir() + L"data\\strings_en.tsv";
    const std::wstring pak = exeDir() + L"PAK\\DX11\\GLOBAL.PAK";
    if (stringsTsvStale(tsv, pak)) {
        CreateDirectoryW((dataDir() + L"data").c_str(), nullptr);
        int n = 0;
        std::string err;
        int64_t t0 = nowMs();
        if (pak::generateStringsTsv(pak, tsv, n, err))
            logLine("generated data\\strings_en.tsv from PAK\\DX11\\GLOBAL.PAK: " + std::to_string(n) + " strings in " + std::to_string(nowMs() - t0) + " ms");
        else logLine("could not generate data\\strings_en.tsv: " + err);
    }
    s_strings = new Strings();
    if (!s_strings->load(tsv)) logLine("warning: data\\strings_en.tsv missing, keys will be spoken raw");
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
    radar::setMatchLiveCallback([](int32_t levelId) { std::lock_guard<std::recursive_mutex> g(g_lock); if (s_nar) s_nar->on_match_live(levelId); });
    if (radar::start()) logLine(std::string("radar thread started (") + (g_cfg.radar ? "on" : "off") + ")");
    s_hotkeyThread = CreateThread(nullptr, 0, hotkeyThread, nullptr, 0, &s_hotkeyThreadId);
    s_initialized = true;
    s_nar->say("Killer Instinct narrator ready", true);
    return 0;
}

static void shutdown() {
    if (!s_initialized) return;
    s_initialized = false;
    radar::stop();
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
        // reserved != NULL means the process is terminating: other threads are already gone, do not unhook or join.
        // The CRT then destroys our statics; a still-joinable std::thread would call std::terminate -> abort (seen as
        // a crash dump on every game exit until 2026-10-09), so the speech worker is detached first.
        if (reserved == nullptr) ki::shutdown();
        else { ki::speech::abandon(); ki::logLine("=== game exiting ==="); }
    }
    return TRUE;
}
