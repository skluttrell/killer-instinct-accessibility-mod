// Scaleform GFx 4.3 Value API as used by the game (read-only access to the AS3 display list through the engine's
// own entry points). Layout of GFx::Value (0x30 bytes): +0x10 pObjectInterface, +0x18 Type, +0x20 payload.
#pragma once
#include "common.h"

namespace ki {
namespace gfx {

struct GValue {
    uint8_t raw[0x30];
    GValue() { memset(raw, 0, sizeof raw); }
    uint32_t type() const { return *(const uint32_t*)(raw + 0x18); }
    uint32_t kind() const { return type() & 0x3f; }
    bool isObj() const { uint32_t k = kind(); return k == 8 || k == 9 || k == 0xA; }
    void* iface() const { return *(void* const*)(raw + 0x10); }
    void* pdata() const { return *(void* const*)(raw + 0x20); }
};

// resolved entry points (module base + RVA, or AOB results)
struct Entry {
    uintptr_t rootInvoke = 0, objInvoke = 0, objGetMember = 0, valueRelease = 0, dispatcher = 0, advance = 0;
};
extern Entry g_fn;
extern void* g_root;          // AS3::MovieRoot of the host shell (ForegroundShell.swf)

void release(GValue& v);                    // ValueRelease for managed values, then zero
json read(const GValue& v);                 // -> json (discarded = undefined, null, bool, number, string, {"obj":kind})
void setStr(GValue& v, const char* s);      // string Value referencing a persistent C string (kept alive by caller)
const char* cstr(const std::string& s);     // interned, persistent C string

void* findRoot(void* wrapper);              // from the dispatcher's first argument
bool rootInvoke(const char* name, GValue& out, GValue* args, unsigned argc);
std::string rootCallStr(const char* name);  // root.<name>() -> string ("" if not a string)
bool getRef(const std::string& swf, GValue& out);   // root.GetSWFRefFromString(swf) -> object (caller releases)
bool getMember(const GValue& obj, const char* name, GValue& out);
bool memberObj(const GValue& obj, const char* name, GValue& out);
json readPath(const GValue& obj, const std::string& path);   // "a.b.c" -> json (discarded when unreadable)
json invoke(const GValue& obj, const char* name, const std::vector<json>& args);   // obj.name(args...)
json invokePath(const GValue& obj, const std::string& path, const char* name);     // obj.a.b.name() (discarded when unreachable)

}  // namespace gfx
}  // namespace ki
