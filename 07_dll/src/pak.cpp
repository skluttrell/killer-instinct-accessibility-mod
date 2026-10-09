#include "pak.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace ki {
namespace pak {

namespace {
const size_t REC = 262, NAME = 240;
const size_t CHUNK = 16u << 20;   // the record table of GLOBAL.PAK sits in the first ~7 MB; later chunks are a fallback

struct File {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~File() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    bool open(const std::wstring& path) {
        h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        return h != INVALID_HANDLE_VALUE;
    }
    int64_t size() const { LARGE_INTEGER li; return GetFileSizeEx(h, &li) ? li.QuadPart : -1; }
    bool readAt(int64_t off, void* buf, size_t n) const {
        LARGE_INTEGER li; li.QuadPart = off;
        if (!SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) return false;
        char* p = (char*)buf;
        while (n) {
            DWORD got = 0;
            if (!ReadFile(h, p, (DWORD)(n > (1u << 30) ? (1u << 30) : n), &got, nullptr) || got == 0) return false;
            p += got; n -= got;
        }
        return true;
    }
};

inline uint32_t rd32(const char* p) { uint32_t v; memcpy(&v, p, 4); return v; }
inline uint64_t rd64(const char* p) { uint64_t v; memcpy(&v, p, 8); return v; }
std::string werr(const char* what) { char b[128]; snprintf(b, sizeof b, "%s (error %lu)", what, GetLastError()); return b; }
}  // namespace

bool readEntry(const std::wstring& pakPath, const std::string& name, std::string& out, std::string& err) {
    if (name.empty() || name.size() >= NAME) { err = "bad record name"; return false; }
    File f;
    if (!f.open(pakPath)) { err = werr("cannot open PAK"); return false; }
    const int64_t fsize = f.size();
    if (fsize < 64) { err = "PAK too small"; return false; }
    char magic[4];
    if (!f.readAt(0, magic, 4) || memcmp(magic, "PAK_", 4) != 0) { err = "not a PAK_ file"; return false; }
    std::string needle = name;
    needle.push_back('\0');   // the name is NUL-terminated inside the 240-byte field, so "strings" does not match "strings_ita"
    std::vector<char> buf(CHUNK + REC);
    int64_t base = 0;
    while (base < fsize) {
        size_t want = (size_t)((fsize - base) < (int64_t)buf.size() ? (fsize - base) : (int64_t)buf.size());
        if (!f.readAt(base, buf.data(), want)) { err = werr("read failed"); return false; }
        for (size_t p = 0; p + REC <= want;) {
            const char* hit = (const char*)memchr(buf.data() + p, needle[0], want - REC - p + 1);
            if (!hit) break;
            p = hit - buf.data();
            if (memcmp(hit, needle.data(), needle.size()) == 0) {
                const char* tr = hit + NAME;
                uint64_t off = rd64(tr + 4);
                uint32_t size = rd32(tr + 12);
                if (off >= 64 && size > 0 && off + size <= (uint64_t)fsize) {
                    out.resize(size);
                    if (!f.readAt((int64_t)off, &out[0], size)) { err = werr("read of record data failed"); return false; }
                    return true;
                }
            }
            p++;
        }
        if (want < buf.size()) break;
        base += CHUNK;   // the next chunk starts REC bytes before the end of this one, so a record spanning the boundary is seen whole
    }
    err = "record not found: " + name;
    return false;
}

bool decodeStringTable(const std::string& blob, std::string& tsv, int& count, std::string& err) {
    count = 0;
    if (blob.size() < 16) { err = "string table too small"; return false; }
    const char* d = blob.data();
    uint32_t ver = rd32(d), n = rd32(d + 4), tab = rd32(d + 8), boff = rd32(d + 12);
    if (ver != 1 || tab != 16 || (uint64_t)tab + (uint64_t)n * 8 != boff || boff > blob.size()) { err = "unexpected string table header"; return false; }
    tsv.clear();
    tsv.reserve(blob.size() + n * 16);
    tsv += "hash\toffset\ttext\n";
    char head[32];
    for (uint32_t i = 0; i < n; i++) {
        uint32_t hash = rd32(d + tab + 8 * i), soff = rd32(d + tab + 8 * i + 4);
        uint64_t p = (uint64_t)boff + soff;
        if (p >= blob.size()) { err = "string offset out of range"; return false; }
        const char* s = d + p;
        const char* e = (const char*)memchr(s, 0, blob.size() - p);
        if (!e) { err = "unterminated string"; return false; }
        snprintf(head, sizeof head, "%08x\t%u\t", hash, soff);
        tsv += head;
        for (; s < e; s++) {
            switch (*s) {
                case '\t': tsv += "\\t"; break;
                case '\n': tsv += "\\n"; break;
                case '\r': tsv += "\\r"; break;
                default: tsv.push_back(*s);
            }
        }
        tsv.push_back('\n');
        count++;
    }
    return true;
}

bool generateStringsTsv(const std::wstring& pakPath, const std::wstring& outTsv, int& count, std::string& err) {
    std::string blob, tsv;
    if (!readEntry(pakPath, "gameinfo\\strings\\strings", blob, err)) return false;
    if (!decodeStringTable(blob, tsv, count, err)) return false;
    HANDLE h = CreateFileW(outTsv.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { err = werr("cannot write TSV"); return false; }
    DWORD written = 0;
    bool ok = WriteFile(h, tsv.data(), (DWORD)tsv.size(), &written, nullptr) && written == tsv.size();
    CloseHandle(h);
    if (!ok) { err = werr("TSV write failed"); DeleteFileW(outTsv.c_str()); return false; }
    return true;
}

}  // namespace pak
}  // namespace ki
