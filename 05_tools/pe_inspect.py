"""Read-only PE inspection: hash, version, imports, section list, keyword strings.
Pure stdlib. Usage: python -I pe_inspect.py <path-to-exe>
"""
import hashlib
import re
import struct
import sys

path = sys.argv[1]
data = open(path, "rb").read()
print("size:", len(data))
print("sha256:", hashlib.sha256(data).hexdigest())

# --- PE headers ---
e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
assert data[e_lfanew:e_lfanew + 4] == b"PE\0\0"
coff = e_lfanew + 4
machine, nsec, timestamp, _, _, opt_size, chars = struct.unpack_from("<HHIIIHH", data, coff)
opt = coff + 20
magic = struct.unpack_from("<H", data, opt)[0]
print("machine: 0x%04x  (0x8664 = x64)" % machine)
print("timestamp:", timestamp)
print("pe32+:", magic == 0x20B)
dd_off = opt + (112 if magic == 0x20B else 96)
dirs = [struct.unpack_from("<II", data, dd_off + 8 * i) for i in range(16)]
sec_off = opt + opt_size
sections = []
for i in range(nsec):
    name, vsize, va, rsize, rptr = struct.unpack_from("<8sIIII", data, sec_off + 40 * i)
    sections.append((name.rstrip(b"\0").decode(errors="replace"), va, vsize, rptr, rsize))
print("sections:")
for s in sections:
    print("  %-8s va=0x%08x vsize=0x%08x raw=0x%08x rsize=0x%08x" % s)


def rva2off(rva):
    for _, va, vsize, rptr, rsize in sections:
        if va <= rva < va + max(vsize, rsize):
            return rva - va + rptr
    return None


def cstr(off):
    end = data.index(b"\0", off)
    return data[off:end].decode(errors="replace")


# --- imports ---
imp_rva, imp_size = dirs[1]
print("\nimports:")
off = rva2off(imp_rva)
while True:
    ilt, ts, fwd, name_rva, iat = struct.unpack_from("<IIIII", data, off)
    if name_rva == 0:
        break
    dll = cstr(rva2off(name_rva))
    thunk = rva2off(ilt or iat)
    names = []
    while True:
        v = struct.unpack_from("<Q", data, thunk)[0]
        if v == 0:
            break
        if v & (1 << 63):
            names.append("ord#%d" % (v & 0xFFFF))
        else:
            names.append(cstr(rva2off(v & 0x7FFFFFFF) + 2))
        thunk += 8
    print("  %-28s %4d funcs" % (dll, len(names)))
    if dll.lower() in ("dinput8.dll", "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll", "dxgi.dll", "d3d11.dll", "d3d12.dll", "version.dll", "winhttp.dll"):
        print("      ", ", ".join(names[:40]))
    off += 20

# --- delay-load imports ---
dl_rva, dl_size = dirs[13]
if dl_rva:
    print("\ndelay-load imports:")
    off = rva2off(dl_rva)
    while True:
        attrs, name_rva = struct.unpack_from("<II", data, off)
        if name_rva == 0:
            break
        print("  ", cstr(rva2off(name_rva)))
        off += 32

# --- keyword strings ---
keywords = [
    b"Scaleform", b"GFx", b"ActionScript", b"gfx", b"swf",
    b"Wwise", b"AK::", b"AkSoundEngine",
    b"PlayFab", b"Xbox", b"xal", b"XSAPI",
    b"EasyAntiCheat", b"BattlEye", b"BEClient", b"Denuvo", b"VMProtect", b"Themida", b"anticheat", b"AntiCheat",
    b"Havok", b"Bink", b"Noesis", b"Xaml", b"XAML", b"Coherent", b"Iggy",
    b"IsDebuggerPresent", b"CheckRemoteDebuggerPresent", b"NtQueryInformationProcess",
    b"localization", b"Localization", b"stringtable", b"StringTable", b"loc.", b"strings",
    b"Narrator", b"narration", b"TextToSpeech", b"SpeechSynthes",
]
print("\nkeyword hits (count, keyword):")
for k in keywords:
    n = data.count(k)
    if n:
        print("  %6d  %s" % (n, k.decode()))

# --- sample ASCII strings around a few keywords ---
def ctx(kw, limit=12):
    out = []
    for m in re.finditer(re.escape(kw), data):
        s = max(0, m.start() - 60)
        e = min(len(data), m.end() + 60)
        chunk = re.sub(rb"[^\x20-\x7e]", b".", data[s:e]).decode()
        out.append(chunk)
        if len(out) >= limit:
            break
    return out

for kw in (b"Scaleform", b"GFx", b"AkSoundEngine", b"Noesis", b"Havok"):
    hits = ctx(kw, 8)
    if hits:
        print("\ncontext for %s:" % kw.decode())
        for h in hits:
            print("   ", h)

# --- UTF-16 strings sampling for menu labels ---
labels = [u"Single Player", u"Multiplayer", u"Shadow Lords", u"Help & Options", u"CHARACTER SELECT", u"Character Select", u"Dojo", u"Practice"]
print("\nUTF-16 label hits in exe:")
for l in labels:
    n = data.count(l.encode("utf-16-le"))
    a = data.count(l.encode("ascii"))
    print("  %-18s utf16=%d ascii=%d" % (l, n, a))
