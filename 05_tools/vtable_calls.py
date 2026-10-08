"""Summarize a vtable: for each slot, the function RVA and the calls it makes (direct targets, indirect
vtable calls, RIP-relative string references). Quick way to find the "Execute" of an event class.
Usage: python vtable_calls.py <exe> <vtable_rva_hex> [nslots=16] [max_bytes_hex=400]
"""
import re
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_64, CS_OP_IMM, CS_OP_MEM, Cs
from capstone.x86 import X86_REG_RIP

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
exe, vt = sys.argv[1], int(sys.argv[2], 16)
nslots = int(sys.argv[3]) if len(sys.argv) > 3 else 16
maxb = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0x400
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
text = next(s for s in sections if s[0] == ".text")
tlo, thi = text[1], text[1] + text[2]


def rva2off(rva):
    for _, va, vsize, rptr, rsize in sections:
        if va <= rva < va + rsize:
            return rva - va + rptr


def strat(rva):
    off = rva2off(rva)
    if off is None:
        return None
    m = re.match(rb"[\x20-\x7e]{3,}\x00", data[off:off + 80])
    return m.group()[:-1].decode() if m else None


# .pdata RUNTIME_FUNCTION table gives exact function bounds
pdata = next((s for s in sections if s[0] == ".pdata"), None)
bounds = {}
if pdata:
    _, pva, pvsize, pptr, prsize = pdata
    for o in range(pptr, pptr + prsize - 12, 12):
        b, e, _u = struct.unpack_from("<III", data, o)
        if b == 0:
            break
        bounds[b] = e


def func_end(fn):
    return bounds.get(fn, fn + maxb)


md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
for i in range(nslots):
    off = rva2off(vt + 8 * i)
    fn = struct.unpack_from("<Q", data, off)[0] - image_base
    if not (tlo <= fn < thi):
        print("slot %d: 0x%x (not code, stop)" % (i, fn))
        break
    calls, ind, strs = [], [], []
    size = 0
    end = func_end(fn)
    for insn in md.disasm(data[rva2off(fn):rva2off(end)], image_base + fn):
        size = insn.address + insn.size - image_base - fn
        if insn.mnemonic in ("call", "jmp"):
            for op in insn.operands:
                if op.type == CS_OP_IMM:
                    t = op.imm - image_base
                    if tlo <= t < thi and insn.mnemonic == "call":
                        calls.append("0x%x" % t)
                elif op.type == CS_OP_MEM and insn.mnemonic == "call":
                    if op.mem.base == X86_REG_RIP:
                        ind.append("[rva 0x%x]" % (insn.address + insn.size + op.mem.disp - image_base))
                    else:
                        ind.append("[%s+0x%x]" % (insn.reg_name(op.mem.base), op.mem.disp))
        for op in insn.operands:
            if op.type == CS_OP_MEM and op.mem.base == X86_REG_RIP and insn.mnemonic == "lea":
                s = strat(insn.address + insn.size + op.mem.disp - image_base)
                if s:
                    strs.append(s)
    print("slot %2d: 0x%x  (%d bytes%s)" % (i, fn, size, "" if fn in bounds else ", no pdata entry"))
    if calls:
        print("    calls:    " + " ".join(calls))
    if ind:
        print("    indirect: " + " ".join(ind))
    if strs:
        print("    strings:  " + " | ".join(strs[:8]))
