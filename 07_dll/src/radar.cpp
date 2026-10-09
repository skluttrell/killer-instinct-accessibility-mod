#include "radar.h"
#include <mmsystem.h>
#include <atomic>
#include <cmath>
#include <vector>
#pragma comment(lib, "winmm.lib")

namespace ki {
namespace radar {

namespace {
// Layout taken from the Lua bindings Opponent_GetPosX/Y/Z (exe RVA 0x6e3ac0 / 0x6e3b30 / 0x6e3bb0):
//   match = *global; valid when (match->+0x48 & 0x8000)
//   player slot i = match + i*0xd0; a 3x4 transform at +0x3e0 whose translation column is X +0x3ec, Y +0x3fc, Z +0x40c
const uintptr_t SLOT_STRIDE = 0xd0, OFF_POS = 0x3ec, OFF_FLAGS = 0x48;
// round-state object (GetRoundElapsedTime, RVA 0x6ef910): elapsed = [+0x370] - [+0x368] as frame counts, or the float at
// +0x938 when the byte at +0x374 is set. Live 2026-10-09: +0x370 is static during the fight, so the gate watches all
// three words and treats "none of them changed for STALL_MS" as paused / loading / not fighting.
const uintptr_t OFF_CLOCK[3] = {0x368, 0x370, 0x938};
const uint32_t FLAG_LIVE = 0x8000;
const int PULSE_MS = 90, STALL_MS = 400;   // frame counter unchanged this long = paused / loading / not fighting

uintptr_t s_global = 0, s_roundGlobal = 0;
std::atomic<bool> s_enabled{true}, s_run{false};
HANDLE s_thread = nullptr;

// Reads of game memory from our own thread: the pointer can go stale at match teardown, so every read is guarded.
bool readBytes(uintptr_t addr, void* out, size_t n) {
    if (!addr) return false;
    __try { memcpy(out, (const void*)addr, n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template <class T> bool readT(uintptr_t addr, T& out) { return readBytes(addr, &out, sizeof(T)); }

double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// One pulse: two-harmonic tone with a 4 ms attack and an exponential release, constant-power panned, 44.1 kHz stereo.
// Blocks until the device has played it (like Tones::Play in FFVII-Access); the radar thread is the only caller.
void playPulse(double freqHz, double pan, int vol) {
    const int rate = 44100;
    const size_t n = (size_t)rate * PULSE_MS / 1000, attack = rate * 4 / 1000;
    const double amp = 32767.0 * 0.8 * (vol / 100.0) * (vol / 100.0);
    const double a = (clamp01((pan + 1) / 2)) * 1.57079632679;   // 0 = left, pi/2 = right
    const double gl = cos(a), gr = sin(a);
    const double w = 2 * 3.14159265358979 * freqHz / rate, tau = rate * 0.030;   // 30 ms release
    std::vector<int16_t> buf(n * 2);
    for (size_t i = 0; i < n; i++) {
        double env = i < attack ? (double)i / attack : exp(-((double)i - attack) / tau);
        double s = (sin(w * i) + 0.35 * sin(2 * w * i)) / 1.35;
        buf[2 * i] = (int16_t)(amp * env * s * gl);
        buf[2 * i + 1] = (int16_t)(amp * env * s * gr);
    }
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = rate * 4;
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!ev) return;
    HWAVEOUT h = nullptr;
    if (waveOutOpen(&h, WAVE_MAPPER, &fmt, (DWORD_PTR)ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) { CloseHandle(ev); return; }
    WAVEHDR hdr{};
    hdr.lpData = (LPSTR)buf.data();
    hdr.dwBufferLength = (DWORD)(buf.size() * sizeof(int16_t));
    if (waveOutPrepareHeader(h, &hdr, sizeof hdr) == MMSYSERR_NOERROR && waveOutWrite(h, &hdr, sizeof hdr) == MMSYSERR_NOERROR) {
        const DWORD deadline = GetTickCount() + PULSE_MS + 1000;
        while (!(hdr.dwFlags & WHDR_DONE)) {
            if ((int32_t)(deadline - GetTickCount()) <= 0) { waveOutReset(h); break; }
            WaitForSingleObject(ev, 50);
        }
    }
    waveOutUnprepareHeader(h, &hdr, sizeof hdr);
    waveOutClose(h);
    CloseHandle(ev);
}

struct Pos { float v[3]; };
bool readPos(uintptr_t match, int slot, Pos& p) {
    uintptr_t base = match + slot * SLOT_STRIDE + OFF_POS;
    for (int k = 0; k < 3; k++) {
        if (!readT(base + k * 0x10, p.v[k])) return false;
        if (!std::isfinite(p.v[k]) || fabs(p.v[k]) > 1e6f) return false;
    }
    return true;
}

// Ground level = the lowest height either fighter has had in the last few seconds. A running minimum over the whole
// match is wrong: during the round intro the fighters are placed below the floor (seen: -0.63), which would make a
// standing opponent read as airborne for the rest of the match. Nobody stays in the air for 3 s, so a window works.
struct FloorTracker {
    struct S { int64_t t; float y; };
    std::vector<S> samples;
    float update(int64_t now, float y) {
        samples.push_back({now, y});
        size_t keep = 0;
        for (size_t i = 0; i < samples.size(); i++) if (now - samples[i].t <= 3000) samples[keep++] = samples[i];
        samples.resize(keep);
        float g = 1e9f;
        for (auto& s : samples) if (s.y < g) g = s.y;
        return g;
    }
    void reset() { samples.clear(); }
};

DWORD WINAPI thread(LPVOID) {
    uintptr_t lastMatch = 0;
    FloorTracker floor;
    int64_t lastLog = 0;
    bool wasLive = false, wasRunning = false;
    uint32_t lastClock[3] = {0, 0, 0};
    int64_t lastFrameChange = 0;
    while (s_run) {
        uintptr_t m = 0;
        uint32_t flags = 0;
        bool live = readT(s_global, m) && m && readT(m + OFF_FLAGS, flags) && (flags & FLAG_LIVE);
        Pos me{}, opp{};
        if (live) live = readPos(m, 0, me) && readPos(m, 1, opp);
        // the flag is already set in the front end with both transforms zeroed; fighters never share the origin in a match
        if (live && me.v[0] == 0 && me.v[1] == 0 && me.v[2] == 0 && opp.v[0] == 0 && opp.v[1] == 0 && opp.v[2] == 0) live = false;
        if (live != wasLive) { logLine(live ? "radar: match state live" : "radar: match state gone"); wasLive = live; }
        // the fight clock: still on the loading screen, during a pause and in menus
        bool running = true;
        uint32_t clock[3] = {0, 0, 0};
        if (live && s_roundGlobal) {
            uintptr_t r = 0;
            uint32_t rflags = 0;
            int64_t now = nowMs();
            if (readT(s_roundGlobal, r) && r && readT(r + OFF_FLAGS, rflags) && (rflags & FLAG_LIVE) &&
                readT(r + OFF_CLOCK[0], clock[0]) && readT(r + OFF_CLOCK[1], clock[1]) && readT(r + OFF_CLOCK[2], clock[2])) {
                bool changed = false;
                for (int k = 0; k < 3; k++) if (clock[k] != lastClock[k]) { lastClock[k] = clock[k]; changed = true; }
                if (changed) lastFrameChange = now;
                running = now - lastFrameChange < STALL_MS;
            } else running = false;
        }
        if (live && running != wasRunning) { logLine(running ? "radar: fight clock running" : "radar: fight clock stopped"); wasRunning = running; }
        if (!live || !running || !s_enabled) { floor.reset(); Sleep(150); continue; }
        if (m != lastMatch) { lastMatch = m; floor.reset(); }
        const int H = g_cfg.radarAxisH, V = g_cfg.radarAxisV;
        float ground = floor.update(nowMs(), me.v[V] < opp.v[V] ? me.v[V] : opp.v[V]);
        // Measured 2026-10-09: Player 1 starts at X +1.5 on the left of the screen, Player 2 at -1.5 on the right, and
        // walking right decreases X, so screen-right is -X. Y is up (a jump peaks near +2.0); Z is the depth lane.
        double dx = (double)me.v[H] - opp.v[H];
        if (g_cfg.radarFlip) dx = -dx;
        double dist = fabs(dx), height = opp.v[V] - ground;
        if (height < 0.15) height = 0;   // crouch / idle animation noise
        double d = clamp01(dist / g_cfg.radarRange);
        int interval = (int)(g_cfg.radarMinMs + (g_cfg.radarMaxMs - g_cfg.radarMinMs) * d);
        double freq = g_cfg.radarBaseHz * pow(2.0, clamp01(height / g_cfg.radarHeight));   // up to one octave
        double pan = (dx < 0 ? -1.0 : 1.0) * (0.35 + 0.65 * d);
        int64_t t0 = nowMs();
        if (g_cfg.radarDebug && t0 - lastLog >= 1000) {
            lastLog = t0;
            char b[256];
            float clockF;
            memcpy(&clockF, &clock[2], 4);
            snprintf(b, sizeof b, "radar: me (%.2f %.2f %.2f) opp (%.2f %.2f %.2f) dist %.2f height %.2f clock %u %u %.2f -> %d ms %.0f Hz pan %.2f",
                     me.v[0], me.v[1], me.v[2], opp.v[0], opp.v[1], opp.v[2], dist, height, clock[0], clock[1], clockF, interval, freq, pan);
            logLine(b);
        }
        if (g_cfg.radarVolume > 0) playPulse(freq, pan, g_cfg.radarVolume > 100 ? 100 : g_cfg.radarVolume);
        int rem = interval - (int)(nowMs() - t0);
        if (rem > 0) Sleep(rem);
    }
    return 0;
}
}  // namespace

// Both getters start with `48 8b 05 <disp32>` (mov rax, [rip+disp]): decode the global they read.
static uintptr_t decodeGlobal(uintptr_t fn, const char* what) {
    uint8_t op[3] = {};
    int32_t disp = 0;
    if (!readBytes(fn, op, 3) || op[0] != 0x48 || op[1] != 0x8b || op[2] != 0x05 || !readT(fn + 3, disp)) {
        logLine(std::string("radar: ") + what + " getter has an unexpected shape");
        return 0;
    }
    uintptr_t g = fn + 7 + disp;
    char b[96];
    snprintf(b, sizeof b, "radar: %s global at +0x%llx", what, (unsigned long long)(g - (uintptr_t)GetModuleHandleW(nullptr)));
    logLine(b);
    return g;
}

void setMatchGetter(uintptr_t fn) { s_global = decodeGlobal(fn, "match-state"); if (!s_global) logLine("radar: disabled"); }
void setRoundGetter(uintptr_t fn) { s_roundGlobal = decodeGlobal(fn, "round-state"); if (!s_roundGlobal) logLine("radar: no fight clock, the pulse will also run while paused"); }

bool start() {
    if (!s_global || s_thread) return false;
    s_enabled = g_cfg.radar;
    s_run = true;
    s_thread = CreateThread(nullptr, 0, thread, nullptr, 0, nullptr);
    return s_thread != nullptr;
}

void stop() {
    if (!s_thread) return;
    s_run = false;
    WaitForSingleObject(s_thread, 1500);
    CloseHandle(s_thread);
    s_thread = nullptr;
}

void setEnabled(bool on) { s_enabled = on; }
bool enabled() { return s_enabled; }

}  // namespace radar
}  // namespace ki
