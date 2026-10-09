// kiaccess - Killer Instinct accessibility narrator (Phase 2, in-process DLL). Shared helpers.
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include "../deps/json/json.hpp"

using json = nlohmann::json;

namespace ki {

struct Config {
    int verbosity = 1;
    bool speech = true;
    bool logEvents = false;     // log every ei/inv event (large log)
    bool dumpPopulate = false;  // write data/populate_<swf>.json for reader development
    // opponent radar (radar.cpp)
    bool radar = true;          // pulse on at start (Ctrl+Shift+P toggles)
    int radarVolume = 50;       // 0-100, squared like FFVII-Access tones
    double radarRange = 7.0;    // horizontal distance (game units) at which the pulse is slowest (round start = 3.0, corner to corner = 9)
    double radarHeight = 2.0;   // opponent height above the ground (game units) for the full octave rise
    int radarMinMs = 120, radarMaxMs = 650;   // pulse interval at point blank / at radarRange
    double radarBaseHz = 440;   // pitch with the opponent on the ground
    int radarAxisH = 0, radarAxisV = 1;       // which transform axis is screen-horizontal / up (0 X, 1 Y, 2 Z)
    bool radarFlip = false;     // swap left/right if the world X axis runs right-to-left on screen
    bool radarDebug = false;    // log positions once a second
};
extern Config g_cfg;
extern std::recursive_mutex g_lock;   // guards the narrator (UI thread hooks vs. hotkey thread)

std::wstring moduleDir();          // directory of this DLL (with trailing backslash)
std::wstring dataDir();            // moduleDir + L"kiaccess\\"
std::wstring exeDir();             // directory of the game exe (the process), with trailing backslash
std::string utf8(const std::wstring& w);
std::wstring wide(const std::string& s);
int64_t nowMs();                   // steady clock, milliseconds
void logLine(const std::string& line);   // timestamped line to kiaccess\speech.log
void loadConfig();
bool readFile(const std::wstring& path, std::string& out);
bool writeFile(const std::wstring& path, const std::string& data, bool append = false);

// json helpers with Python dict.get() semantics
inline bool isObj(const json& j) { return j.is_object(); }
inline const json& jget(const json& j, const char* k) {
    static const json none;  // null
    if (!j.is_object()) return none;
    auto it = j.find(k);
    return it == j.end() ? none : *it;
}
inline std::string jstr(const json& j, const char* k) {   // "" when missing or not a string
    const json& v = jget(j, k);
    return v.is_string() ? v.get<std::string>() : std::string();
}
inline bool jtruthy(const json& j) {   // Python truthiness
    if (j.is_null() || j.is_discarded()) return false;
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number()) return j.get<double>() != 0;
    if (j.is_string()) return !j.get<std::string>().empty();
    return !j.empty();
}
inline bool jisInt(const json& j) { return j.is_number_integer(); }
inline int jint(const json& j, int def = 0) { return j.is_number() ? (int)j.get<double>() : def; }
std::string jtostr(const json& j);   // Python str(): numbers without trailing zeros, strings as-is
std::string lower(std::string s);
bool startsWith(const std::string& s, const char* p);
bool endsWith(const std::string& s, const char* p);
std::string join(const std::vector<std::string>& v, const char* sep);
std::string replaceAll(std::string s, const std::string& from, const std::string& to);

}  // namespace ki
