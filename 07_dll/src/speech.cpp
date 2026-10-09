#include "speech.h"
#include <condition_variable>
#include <deque>
#include <thread>

namespace ki {
namespace speech {

// C ABI of prism.dll as used by 05_tools/prism_ctypes.py (verified on this machine with the prismatoid wheel)
#pragma pack(push, 8)
struct PrismConfig {
    uint8_t version;
    void* registry;
    void* availability_callback;
    void* availability_userdata;
    uint32_t availability_poll_interval_ms;
    uint32_t availability_debounce_samples;
    uint32_t availability_backoff_max_ms;
    bool availability_auto_power_manage;
    void* availability_baseline_callback;
};
#pragma pack(pop)

using prism_config_init_t = PrismConfig (*)(void);
using prism_init_t = void* (*)(PrismConfig*);
using prism_shutdown_t = void (*)(void*);
using prism_registry_create_best_t = void* (*)(void*);
using prism_backend_initialize_t = int (*)(void*);
using prism_backend_name_t = const char* (*)(void*);
using prism_backend_output_t = int (*)(void*, const char*, bool);
using prism_backend_stop_t = int (*)(void*);
using prism_backend_free_t = void (*)(void*);

static HMODULE s_lib = nullptr;
static void* s_ctx = nullptr;
static void* s_backend = nullptr;
static prism_backend_output_t s_output = nullptr;
static prism_backend_stop_t s_stop = nullptr;
static std::string s_name;

struct Item { std::string text; bool interrupt; };
static std::deque<Item> s_queue;
static std::mutex s_qlock;
static std::condition_variable s_cv;
static std::thread s_worker;
static bool s_running = false;

static void workerLoop() {
    while (true) {
        Item it;
        {
            std::unique_lock<std::mutex> lk(s_qlock);
            s_cv.wait(lk, [] { return !s_running || !s_queue.empty(); });
            if (!s_running && s_queue.empty()) return;
            it = s_queue.front();
            s_queue.pop_front();
        }
        if (s_output && s_backend) s_output(s_backend, it.text.c_str(), it.interrupt);
    }
}

bool init(const std::wstring& prismDllPath) {
    s_lib = LoadLibraryW(prismDllPath.c_str());
    if (!s_lib) { logLine("prism.dll not found at " + utf8(prismDllPath)); return false; }
    auto cfgInit = (prism_config_init_t)GetProcAddress(s_lib, "prism_config_init");
    auto pinit = (prism_init_t)GetProcAddress(s_lib, "prism_init");
    auto best = (prism_registry_create_best_t)GetProcAddress(s_lib, "prism_registry_create_best");
    auto binit = (prism_backend_initialize_t)GetProcAddress(s_lib, "prism_backend_initialize");
    auto bname = (prism_backend_name_t)GetProcAddress(s_lib, "prism_backend_name");
    s_output = (prism_backend_output_t)GetProcAddress(s_lib, "prism_backend_output");
    s_stop = (prism_backend_stop_t)GetProcAddress(s_lib, "prism_backend_stop");
    if (!cfgInit || !pinit || !best || !binit || !bname || !s_output) { logLine("prism.dll: missing exports"); return false; }
    PrismConfig cfg = cfgInit();
    s_ctx = pinit(&cfg);
    if (!s_ctx) { logLine("prism_init failed"); return false; }
    s_backend = best(s_ctx);
    if (!s_backend) { logLine("prism: no usable speech backend"); return false; }
    int rc = binit(s_backend);
    if (rc != 0 && rc != 15) { logLine("prism backend init failed, error " + std::to_string(rc)); return false; }
    s_name = bname(s_backend) ? bname(s_backend) : "?";
    s_running = true;
    s_worker = std::thread(workerLoop);
    return true;
}

void speak(const std::string& text, bool interrupt) {
    if (!s_running) return;
    {
        std::lock_guard<std::mutex> g(s_qlock);
        if (interrupt) s_queue.clear();   // a newer interrupting utterance supersedes anything still queued
        s_queue.push_back({text, interrupt});
    }
    s_cv.notify_one();
}

void stop() {
    if (s_stop && s_backend) s_stop(s_backend);
}

std::string backendName() { return s_name; }

void shutdown() {
    if (!s_running) return;
    {
        std::lock_guard<std::mutex> g(s_qlock);
        s_running = false;
        s_queue.clear();
    }
    s_cv.notify_all();
    if (s_worker.joinable()) s_worker.join();
    auto bfree = (prism_backend_free_t)GetProcAddress(s_lib, "prism_backend_free");
    auto pshut = (prism_shutdown_t)GetProcAddress(s_lib, "prism_shutdown");
    if (bfree && s_backend) bfree(s_backend);
    if (pshut && s_ctx) pshut(s_ctx);
    s_backend = nullptr;
    s_ctx = nullptr;
}

void abandon() {
    s_running = false;
    if (s_worker.joinable()) s_worker.detach();
}

}  // namespace speech
}  // namespace ki
