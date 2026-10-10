#include "gfx.h"
#include <unordered_map>

namespace ki {
namespace gfx {

Entry g_fn;
void* g_root = nullptr;

using RootInvokeFn = uint8_t (*)(void* self, const char* name, GValue* result, GValue* args, unsigned argc);
using ObjInvokeFn = uint8_t (*)(void* iface, void* pdata, GValue* result, const char* name, GValue* args, uint64_t nargs, uint8_t isdobj);
using ObjGetMemberFn = uint8_t (*)(void* iface, void* pdata, const char* name, GValue* out, uint8_t isdobj);
using ValueReleaseFn = void (*)(GValue* v);

const char* cstr(const std::string& s) {
    static std::unordered_map<std::string, std::string*> pool;   // never freed: the strings must outlive every Value
    auto it = pool.find(s);
    if (it != pool.end()) return it->second->c_str();
    auto* p = new std::string(s);
    pool.emplace(s, p);
    return p->c_str();
}

void release(GValue& v) {
    if ((v.type() & 0x40) && g_fn.valueRelease) ((ValueReleaseFn)g_fn.valueRelease)(&v);
    memset(v.raw, 0, sizeof v.raw);
}

json read(const GValue& v) {
    uint32_t t = v.type(), k = t & 0x3f;
    const uint8_t* p = v.raw + 0x20;
    if (k == 6 || k == 7) {
        if (t & 0x40) p = *(const uint8_t* const*)p;
        if (!p) return json(json::value_t::discarded);
        const void* s = *(const void* const*)p;
        if (!s) return std::string();
        if (k == 6) return std::string((const char*)s);
        return utf8(std::wstring((const wchar_t*)s));
    }
    if (k == 5) return *(const double*)p;
    if (k == 4) return *(const uint32_t*)p;
    if (k == 3) return *(const int32_t*)p;
    if (k == 2) return *(const uint8_t*)p != 0;
    if (k == 1) return nullptr;
    if (k == 0) return json(json::value_t::discarded);
    return json{{"obj", k}};
}

void setStr(GValue& v, const char* s) {
    memset(v.raw, 0, sizeof v.raw);
    *(uint32_t*)(v.raw + 0x18) = 6;
    *(const char**)(v.raw + 0x20) = s;
}

void* findRoot(void* wrapper) {
    __try {
        void* a = *(void**)wrapper;
        if (!a) return nullptr;
        void* b = *(void**)((uint8_t*)a + 0x18);
        if (!b) return nullptr;
        void* z = *(void**)((uint8_t*)b + 0x18);
        if (!z) return nullptr;
        void** vt = *(void***)z;
        if (vt && vt[57] == (void*)g_fn.rootInvoke) return z;   // MovieRoot vtable slot 57 = Invoke
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

bool rootInvoke(const char* name, GValue& out, GValue* args, unsigned argc) {
    if (!g_root) return false;
    return ((RootInvokeFn)g_fn.rootInvoke)(g_root, name, &out, args, argc) != 0;
}

std::string rootCallStr(const char* name) {
    GValue out;
    std::string r;
    if (rootInvoke(name, out, nullptr, 0)) {
        json j = read(out);
        if (j.is_string()) r = j.get<std::string>();
    }
    release(out);
    return r;
}

bool getRef(const std::string& swf, GValue& out) {
    GValue arg;
    setStr(arg, cstr(swf));
    if (rootInvoke("root.GetSWFRefFromString", out, &arg, 1) && out.isObj()) return true;
    release(out);
    return false;
}

bool getMember(const GValue& obj, const char* name, GValue& out) {
    void* iface = obj.iface();
    if (!iface) return false;
    return ((ObjGetMemberFn)g_fn.objGetMember)(iface, obj.pdata(), name, &out, obj.kind() == 0xA ? 1 : 0) != 0;
}

bool memberObj(const GValue& obj, const char* name, GValue& out) {
    if (getMember(obj, name, out) && out.isObj()) return true;
    release(out);
    return false;
}

json readPath(const GValue& obj, const std::string& path) {
    // Managed Values are tracked by the engine by their address until released: every intermediate Value lives in its
    // own fixed slot and is never copied (a moved Value crashed the game in ValueRelease, 2026-10-08).
    enum { MAX_DEPTH = 10 };
    std::string segs[MAX_DEPTH];
    size_t n = 0, start = 0;
    while (true) {
        size_t dot = path.find('.', start);
        if (n >= MAX_DEPTH) return json(json::value_t::discarded);
        segs[n++] = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    GValue vals[MAX_DEPTH];
    const GValue* src = &obj;
    for (size_t i = 0; i < n; i++) {
        bool ok = getMember(*src, cstr(segs[i]), vals[i]);
        if (i > 0) release(vals[i - 1]);   // the previous link is no longer needed once the lookup used it
        if (!ok) { release(vals[i]); return json(json::value_t::discarded); }
        if (i + 1 < n && !vals[i].isObj()) { release(vals[i]); return json(json::value_t::discarded); }
        src = &vals[i];
    }
    json r = read(vals[n - 1]);
    release(vals[n - 1]);
    return r;
}

json invokePath(const GValue& obj, const std::string& path, const char* name) {
    // same fixed-slot walk as readPath, then a method call on the last object
    enum { MAX_DEPTH = 10 };
    std::string segs[MAX_DEPTH];
    size_t n = 0, start = 0;
    while (true) {
        size_t dot = path.find('.', start);
        if (n >= MAX_DEPTH) return json(json::value_t::discarded);
        segs[n++] = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    GValue vals[MAX_DEPTH];
    const GValue* src = &obj;
    for (size_t i = 0; i < n; i++) {
        bool ok = getMember(*src, cstr(segs[i]), vals[i]);
        if (i > 0) release(vals[i - 1]);
        if (!ok || !vals[i].isObj()) { release(vals[i]); return json(json::value_t::discarded); }
        src = &vals[i];
    }
    json r = invoke(vals[n - 1], name, {});
    release(vals[n - 1]);
    return r;
}

json invoke(const GValue& obj, const char* name, const std::vector<json>& args) {
    void* iface = obj.iface();
    if (!iface) return json(json::value_t::discarded);
    std::vector<GValue> argv(args.size());
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].is_number()) {
            *(uint32_t*)(argv[i].raw + 0x18) = 3;
            *(int32_t*)(argv[i].raw + 0x20) = (int32_t)args[i].get<double>();
        } else {
            setStr(argv[i], cstr(jtostr(args[i])));
        }
    }
    GValue out;
    uint8_t ok = ((ObjInvokeFn)g_fn.objInvoke)(iface, obj.pdata(), &out, name, args.empty() ? nullptr : argv.data(), (uint64_t)args.size(),
                                              obj.kind() == 0xA ? 1 : 0);
    json r = ok ? read(out) : json(json::value_t::discarded);
    release(out);
    return r;
}

}  // namespace gfx
}  // namespace ki
