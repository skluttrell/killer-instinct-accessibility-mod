#include "strings.h"
#include <sstream>

namespace ki {

uint32_t Strings::crc32(const std::string& s) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : s) c = table[(c ^ ch) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool Strings::load(const std::wstring& tsvPath) {
    std::string data;
    if (!readFile(tsvPath, data)) return false;
    std::istringstream ss(data);
    std::string line;
    bool first = true;
    while (std::getline(ss, line)) {
        if (first) { first = false; continue; }   // header
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t t1 = line.find('\t');
        if (t1 == std::string::npos) continue;
        size_t t2 = line.find('\t', t1 + 1);
        if (t2 == std::string::npos) continue;
        uint32_t h = (uint32_t)strtoul(line.substr(0, t1).c_str(), nullptr, 16);
        byHash_[h] = line.substr(t2 + 1);
    }
    return !byHash_.empty();
}

std::string Strings::resolve(const json& key) const {
    if (key.is_null() || key.is_discarded()) return "";
    if (!key.is_string()) return jtostr(key);
    return resolve(key.get<std::string>());
}

std::string Strings::resolve(const std::string& key) const {
    if (key.empty()) return "";
    if (key[0] == '&') return key.substr(1);
    if (key[0] == '#' && key.size() > 1) {
        size_t i = 1;
        if (key[i] == '-') i++;
        bool digits = i < key.size();
        for (size_t k = i; k < key.size(); k++) if (!isdigit((unsigned char)key[k])) digits = false;
        if (digits) {
            long long v = strtoll(key.c_str() + 1, nullptr, 10);
            auto it = byHash_.find((uint32_t)(v & 0xFFFFFFFF));
            return it == byHash_.end() ? key : it->second;
        }
    }
    auto it = byHash_.find(crc32(lower(key)));
    return it == byHash_.end() ? key : it->second;
}

}  // namespace ki
