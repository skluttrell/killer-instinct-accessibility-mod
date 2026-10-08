#include "common.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

extern HMODULE g_thisModule;

namespace ki {

Config g_cfg;
std::recursive_mutex g_lock;

std::wstring moduleDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(g_thisModule, buf, MAX_PATH);
    std::wstring p(buf, n);
    size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? L"" : p.substr(0, k + 1);
}

std::wstring dataDir() { return moduleDir() + L"kiaccess\\"; }

std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static std::mutex s_logLock;
void logLine(const std::string& line) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char ts[32];
    snprintf(ts, sizeof ts, "%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::lock_guard<std::mutex> g(s_logLock);
    writeFile(dataDir() + L"speech.log", std::string(ts) + line + "\n", true);
}

bool readFile(const std::wstring& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const std::wstring& path, const std::string& data, bool append) {
    std::ofstream f(path, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    if (!f) return false;
    f << data;
    return true;
}

void loadConfig() {
    std::string ini;
    if (!readFile(dataDir() + L"kiaccess.ini", ini)) return;
    std::istringstream ss(ini);
    std::string line;
    while (std::getline(ss, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#' || line[0] == ';') continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        while (!k.empty() && isspace((unsigned char)k.back())) k.pop_back();
        while (!v.empty() && (isspace((unsigned char)v.back()) || v.back() == '\r')) v.pop_back();
        while (!v.empty() && isspace((unsigned char)v.front())) v.erase(0, 1);
        if (k == "verbosity") g_cfg.verbosity = atoi(v.c_str());
        else if (k == "speech") g_cfg.speech = v != "0";
        else if (k == "log_events") g_cfg.logEvents = v != "0";
        else if (k == "dump_populate") g_cfg.dumpPopulate = v != "0";
    }
    if (g_cfg.verbosity < 0) g_cfg.verbosity = 0;
    if (g_cfg.verbosity > 2) g_cfg.verbosity = 2;
}

std::string jtostr(const json& j) {
    if (j.is_string()) return j.get<std::string>();
    if (j.is_null() || j.is_discarded()) return "";
    if (j.is_boolean()) return j.get<bool>() ? "True" : "False";
    if (j.is_number_integer()) return std::to_string(j.get<long long>());
    if (j.is_number()) {
        double d = j.get<double>();
        if (d == (long long)d) return std::to_string((long long)d);
        char b[64];
        snprintf(b, sizeof b, "%g", d);
        return b;
    }
    return j.dump();
}

std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
bool startsWith(const std::string& s, const char* p) { return s.compare(0, strlen(p), p) == 0; }
bool endsWith(const std::string& s, const char* p) {
    size_t n = strlen(p);
    return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
}
std::string join(const std::vector<std::string>& v, const char* sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}
std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) {
        s.replace(p, from.size(), to);
        p += to.size();
    }
    return s;
}

}  // namespace ki
