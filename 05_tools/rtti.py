"""Recover MSVC x64 RTTI from the exe: class name -> vtable RVA (+ first N vtable slots).
Lets us locate Scaleform GFx classes (MovieImpl, Value, Translator, ExternalInterface handler...) without symbols.
Usage:
  python -I rtti.py <exe> list [substring]            # list class names (optionally filtered)
  python -I rtti.py <exe> vtable <name-substring> [N]  # vtables for matching classes, with N slots (default 48)
"""
import re
import struct
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
exe = sys.argv[1]
mode = sys.argv[2]
data = open(exe, "rb").read()

e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
coff = e_lfanew + 4
nsec = struct.unpack_from("<H", data, coff + 2)[0]
opt_size = struct.unpack_from("<H", data, coff + 16)[0]
opt = coff + 20
image_base = struct.unpack_from("<Q", data, opt + 24)[0]
sec_off = opt + opt_size
sections = []
for i in range(nsec):
    name, vsize, va, rsize, rptr = struct.unpack_from("<8sIIII", data, sec_off + 40 * i)
    sections.append((name.rstrip(b"\0").decode(), va, vsize, rptr, rsize))


def off2rva(off):
    for _, va, vsize, rptr, rsize in sections:
        if rptr <= off < rptr + rsize:
            return off - rptr + va
    return None


def rva2off(rva):
    for _, va, vsize, rptr, rsize in sections:
        if va <= rva < va + rsize:
            return rva - va + rptr
    return None


# 1. TypeDescriptors: 8-byte vtable ptr, 8-byte spare, then ".?AV<name>@@" string. Record RVA of the descriptor start.
tds = {}
for m in re.finditer(rb"\.\?A[VU][\x21-\x7e]{1,300}?@@\0", data):
    off = m.start() - 16
    rva = off2rva(off)
    if rva is None:
        continue
    tds[rva] = m.group()[:-1].decode()
print("type descriptors:", len(tds))

if mode == "list":
    sub = sys.argv[3].lower() if len(sys.argv) > 3 else ""
    for rva, name in sorted(tds.items(), key=lambda kv: kv[1]):
        if sub in name.lower():
            print("  %s" % name)
    sys.exit()

sub = sys.argv[3].lower()
nslots = int(sys.argv[4]) if len(sys.argv) > 4 else 48
want = {rva: name for rva, name in tds.items() if sub in name.lower()}
print("matching classes:", len(want))

# 2. CompleteObjectLocators in .rdata: {sig=1, offset, cdOffset, pTypeDescriptor(RVA), pClassDescriptor, pSelf(RVA)}
rdata = next(s for s in sections if s[0] == ".rdata")
_, rva0, vsize, rptr, rsize = rdata
cols = {}
for off in range(rptr, rptr + rsize - 24, 4):
    if data[off:off + 4] != b"\x01\x00\x00\x00":
        continue
    sig, offset, cdoff, ptd, pcd, pself = struct.unpack_from("<IIIIII", data, off)
    if ptd in want and pself == off2rva(off):
        cols[off2rva(off)] = (want[ptd], offset)
print("complete object locators:", len(cols))

# 3. vtables: an 8-byte pointer (image_base + COL RVA) immediately precedes the vtable
text = next(s for s in sections if s[0] == ".text")
tlo, thi = text[1], text[1] + text[2]
for col_rva, (name, offset) in sorted(cols.items(), key=lambda kv: kv[1]):
    needle = struct.pack("<Q", image_base + col_rva)
    pos = rptr
    while True:
        i = data.find(needle, pos, rptr + rsize)
        if i < 0:
            break
        vt_rva = off2rva(i + 8)
        slots = []
        for k in range(nslots):
            q = i + 8 + 8 * k
            if q + 8 > len(data):
                break
            fn = struct.unpack_from("<Q", data, q)[0] - image_base
            if not (tlo <= fn < thi):
                break
            slots.append(fn)
        print("\n%s  (offset %d)  vtable RVA 0x%x  (%d code slots)" % (name, offset, vt_rva, len(slots)))
        print("   " + " ".join("%d:0x%x" % (k, s) for k, s in enumerate(slots)))
        pos = i + 8
