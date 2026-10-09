#include "hooks.h"
#include "gfx.h"
#include "radar.h"
#include "snapshot.h"
#include "sigs.h"
#include "../deps/minhook/include/MinHook.h"
#include <bcrypt.h>
#include <set>
#pragma comment(lib, "bcrypt.lib")

namespace ki {
namespace hooks {

using gfx::GValue;

static Narrator* s_nar = nullptr;
static bool s_enabled = true;
static bool s_installed = false;

// ---- address resolution ----
static std::string sha256File(const std::wstring& path) {
    std::string data;
    if (!readFile(path, data)) return "";
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return "";
    std::string out;
    if (BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0) {
        BCryptHashData(h, (PUCHAR)data.data(), (ULONG)data.size(), 0);
        unsigned char dig[32];
        if (BCryptFinishHash(h, dig, 32, 0) == 0) {
            char hex[65];
            for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", dig[i]);
            out = hex;
        }
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
}

static uintptr_t aobScan(uintptr_t base, const char* pattern) {
    // parse "48 89 ?? 24" into bytes + mask
    std::vector<int> pat;
    for (const char* p = pattern; *p;) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (p[0] == '?') pat.push_back(-1);
        else pat.push_back((int)strtoul(std::string(p, 2).c_str(), nullptr, 16));
        p += 2;
    }
    if (pat.empty() || pat[0] < 0) return 0;
    auto* dos = (IMAGE_DOS_HEADER*)base;
    auto* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; s++, sec++) {
        if (memcmp(sec->Name, ".text", 5) != 0) continue;
        const uint8_t* text = (const uint8_t*)(base + sec->VirtualAddress);
        size_t size = sec->Misc.VirtualSize, n = pat.size();
        for (size_t i = 0; i + n <= size; i++) {
            if (text[i] != pat[0]) continue;
            bool ok = true;
            for (size_t k = 1; k < n && ok; k++) if (pat[k] >= 0 && text[i + k] != pat[k]) ok = false;
            if (ok) return (uintptr_t)(text + i);
        }
    }
    return 0;
}

bool resolveAddresses() {
    HMODULE exe = GetModuleHandleW(nullptr);
    uintptr_t base = (uintptr_t)exe;
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(exe, path, MAX_PATH);
    std::string hash = sha256File(path);
    bool known = hash == KNOWN_EXE_SHA256;
    logLine(std::string("exe sha256 ") + hash.substr(0, 16) + (known ? " (known build: fixed offsets)" : " (unknown build: signature scan)"));
    for (const Sig& s : SIGS) {
        uintptr_t addr = known ? base + s.rva : aobScan(base, s.pattern);
        if (!addr) { logLine(std::string("signature not found: ") + s.name); return false; }
        std::string n = s.name;
        if (n == "dispatcher") gfx::g_fn.dispatcher = addr;
        else if (n == "rootInvoke") gfx::g_fn.rootInvoke = addr;
        else if (n == "objInvoke") gfx::g_fn.objInvoke = addr;
        else if (n == "objGetMember") gfx::g_fn.objGetMember = addr;
        else if (n == "valueRelease") gfx::g_fn.valueRelease = addr;
        else if (n == "advance") gfx::g_fn.advance = addr;
        else if (n == "matchState") radar::setMatchGetter(addr);
        else if (n == "roundState") radar::setRoundGetter(addr);
        if (!known) logLine(n + " found at +0x" + [](uintptr_t v) { char b[32]; snprintf(b, sizeof b, "%llx", (unsigned long long)v); return std::string(b); }(addr - base));
    }
    return true;
}

// ---- state shared by the detours (UI thread only, except the enable flag) ----
static const std::map<std::string, std::string> SCRIPT2SWF = {{"MainMenu", "MainMenu.swf"}, {"MessageBox", "Popup.swf"}, {"CharSelectMenu", "CharacterSelect.swf"}};
static const std::set<std::string> HELPERS = {"uiSounds", "uiScreenUtil", "Legend", "GenericAssets", "PauseBackground", "FrontendBackground",
    "FrontEndBackground", "Toast", "LoadingScreen", "BackgroundShell", "ForegroundShell"};
static const std::set<std::string> TRIGGERS = {"PlayScroll", "PlayPauseMenuButtons", "CheckIsDestInstalled", "ScreenShown", "PlayScrollLeftRight",
    "PlayScrollLeftRight2", "PlayNegative", "PlayAccept", "PlayBack", "SetVariables", "CommandSelect", "PlayOption", "PlaySoundString",
    "PlayScrollP1", "PlayScrollP2", "PlayNegativeP1", "PlayNegativeP2", "PlayBackP1", "PlayBackP2", "AS_PlayerPickedFighter",
    "AS_PlayerPickedCostume", "AS_PlayerPickedColor", "AS_ResetPlayerSelection", "AS_SetSelections",
    "PlayExitMenu", "ResetPauseMenuButtons", "PlayTransIn", "PlayScroll1", "PlayScroll2", "PlayPostMatchButtonsP1", "PlayPostMatchButtonsP2",
    "PlayAcceptP1", "PlayAcceptP2", "AS_ListenForAnyKey", "GetEntriesInCategory", "UpdateCategoryIndex", "AS_EnteredPracticeTab",
    "EntrySelected", "StageSelectionChanged", "CallPostMatchSelectionChanged", "UpdateMenuIndices"};
static const std::set<std::string> INV_TRIGGERS = {"Populate", "Lua_Populate", "PopulateEntries", "PopulateStatesOnly", "RefreshPage",
    "Lua_ChangeSelectionTo", "Lua_ReceiveAnyKeyPress", "CancelMapping", "UpdatePlayerMenuIndex"};
static const int SETTLE_FRAMES[] = {1, 30, 180};   // +1 frame, +0.5 s, +3 s (menus that build on with an animation)

static std::string s_currentSwf, s_prevSwf;
static DWORD s_uiThread = 0;
static void* s_tickMovie = nullptr;
struct Pending { std::string swf, why; int frame = 0; size_t due = 0; bool active = false; };
static Pending s_pending;

static std::string argStr(const GValue* v) {
    json j = gfx::read(*v);
    if (j.is_string()) return j.get<std::string>();
    if (j.is_discarded()) return "";
    return j.dump();
}

static void deliverFocus(json&& rec) {
    if (rec.empty()) return;
    if (g_cfg.logEvents) logLine("focus " + rec.dump().substr(0, 400));
    std::lock_guard<std::recursive_mutex> g(g_lock);
    s_nar->on_focus(rec);
}

static void schedule(const std::string& swf, const std::string& why) { s_pending = {swf, why, 0, 0, true}; }

static void runPending() {
    if (!s_pending.active || GetCurrentThreadId() != s_uiThread) return;
    s_pending.frame++;
    if (s_pending.frame < SETTLE_FRAMES[s_pending.due]) return;
    s_pending.due++;
    Pending p = s_pending;
    if (p.due >= sizeof(SETTLE_FRAMES) / sizeof(SETTLE_FRAMES[0])) s_pending.active = false;
    deliverFocus(snap::snapshot(p.swf, p.why + (p.frame > 1 ? "++" : "+")));
}

// ---- detours ----
using DispatcherFn = uint64_t (*)(void* wrapper, const char* method, GValue* args, unsigned argc);
using RootInvokeFn = uint8_t (*)(void* self, const char* name, GValue* result, GValue* args, unsigned argc);
using AdvanceFn = float (*)(void* self, float deltaT, unsigned frameCatchUp, bool capture);
static DispatcherFn s_origDispatcher = nullptr;
static RootInvokeFn s_origRootInvoke = nullptr;
static AdvanceFn s_origAdvance = nullptr;

static uint64_t hookDispatcher(void* wrapper, const char* method, GValue* args, unsigned argc) {
    std::string script, fn, js, swf;
    bool trigger = false;
    if (s_enabled) {
        if (!gfx::g_root) { gfx::g_root = gfx::findRoot(wrapper); if (gfx::g_root) logLine("root found"); }
        if (!s_uiThread) s_uiThread = GetCurrentThreadId();
        std::string a[3];
        for (unsigned i = 0; i < argc && i < 3; i++) a[i] = argStr(args + i);
        script = a[0]; fn = a[1]; js = a[2];
        if (fn == "LoadDestination") {
            json j = json::parse(js, nullptr, false, true);
            std::string d = jstr(j, "Destination");
            if (!d.empty()) s_currentSwf = d;
        } else if (!script.empty() && !HELPERS.count(script)) {
            auto it = SCRIPT2SWF.find(script);
            std::string sw = it != SCRIPT2SWF.end() ? it->second : script + ".swf";
            if (sw == "Popup.swf" && s_currentSwf != "Popup.swf") s_prevSwf = s_currentSwf;
            s_currentSwf = sw;
        }
        swf = s_currentSwf;
        if (g_cfg.logEvents) logLine("ei " + script + "." + fn + " " + js.substr(0, 200));
        {
            std::lock_guard<std::recursive_mutex> g(g_lock);
            s_nar->on_ei(script, fn, js, nowMs());
        }
        trigger = TRIGGERS.count(fn) > 0;
    }
    uint64_t ret = s_origDispatcher(wrapper, method, args, argc);
    if (s_enabled && trigger && gfx::g_root) {
        if (swf.empty()) { swf = gfx::rootCallStr("root.GetActiveSWF"); if (!swf.empty()) s_currentSwf = swf; }
        if (!swf.empty()) { deliverFocus(snap::snapshot(swf, fn)); schedule(swf, fn); }
    }
    return ret;
}

static uint8_t hookRootInvoke(void* self, const char* name, GValue* result, GValue* args, unsigned argc) {
    std::string a[3];
    bool isInvoke = false;
    if (s_enabled) {
        if (!gfx::g_root) { void** vt = *(void***)self; if (vt && vt[57] == (void*)gfx::g_fn.rootInvoke) gfx::g_root = self; }
        std::string n = name ? name : "";
        if (n == "root.Invoke") {
            isInvoke = true;
            for (unsigned i = 0; i < argc && i < 3; i++) a[i] = argStr(args + i);
            if (g_cfg.logEvents) logLine("inv " + a[0] + "." + a[1] + " " + a[2].substr(0, 300));
            if (a[0] == "Popup.swf" && a[1] == "Close" && s_currentSwf == "Popup.swf" && !s_prevSwf.empty()) s_currentSwf = s_prevSwf;   // popup closed
            std::lock_guard<std::recursive_mutex> g(g_lock);
            s_nar->on_inv(a[0], a[1], a[2]);
        }
    }
    uint8_t ret = s_origRootInvoke(self, name, result, args, argc);
    if (s_enabled && isInvoke && gfx::g_root && INV_TRIGGERS.count(a[1]) && snap::hasScreen(a[0])) {
        deliverFocus(snap::snapshot(a[0], "inv:" + a[1]));
        schedule(a[0], "inv:" + a[1]);
    }
    return ret;
}

static float hookAdvance(void* self, float deltaT, unsigned frameCatchUp, bool capture) {
    if (!s_tickMovie) s_tickMovie = self;   // count frames of one movie only (several movies advance per frame)
    float r = s_origAdvance(self, deltaT, frameCatchUp, capture);
    if (s_enabled && self == s_tickMovie && s_pending.active) runPending();
    return r;
}

bool install(Narrator* nar) {
    s_nar = nar;
    if (MH_Initialize() != MH_OK) { logLine("MH_Initialize failed"); return false; }
    if (MH_CreateHook((void*)gfx::g_fn.dispatcher, (void*)hookDispatcher, (void**)&s_origDispatcher) != MH_OK ||
        MH_CreateHook((void*)gfx::g_fn.rootInvoke, (void*)hookRootInvoke, (void**)&s_origRootInvoke) != MH_OK ||
        MH_CreateHook((void*)gfx::g_fn.advance, (void*)hookAdvance, (void**)&s_origAdvance) != MH_OK) {
        logLine("MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) { logLine("MH_EnableHook failed"); return false; }
    s_installed = true;
    return true;
}

void uninstall() {
    if (!s_installed) return;
    s_enabled = false;
    MH_DisableHook(MH_ALL_HOOKS);
    Sleep(100);   // let in-flight detour calls return before the trampolines go away
    MH_Uninitialize();
    s_installed = false;
}

void setEnabled(bool on) { s_enabled = on; }
bool enabled() { return s_enabled; }

}  // namespace hooks
}  // namespace ki
