"""Find code that uses a given 32-bit constant as a displacement or immediate (e.g. a struct offset).
Byte-searches .text for the little-endian value, then decodes the instruction around each hit and keeps
those whose operand really is that value. Prints RVA, instruction, and the enclosing .pdata function.
Usage: python disp_scan.py <exe> <hex_value> [<hex_value> ...]
"""
import bisect
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_64, CS_OP_IMM, CS_OP_MEM, Cs

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
exe = sys.argv[1]
vals = [int(v, 16) for v in sys.argv[2:]]
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
_, tva, tvsize, tptr, trsize = text
pdata = next(s for s in sections if s[0] == ".pdata")
starts, ends = [], {}
for o in range(pdata[3], pdata[3] + pdata[4] - 12, 12):
    b, e, _u = struct.unpack_from("<III", data, o)
    if b == 0:
        break
    starts.append(b)
    ends[b] = e
starts.sort()


def func_of(rva):
    i = bisect.bisect_right(starts, rva) - 1
    if i >= 0 and rva < ends[starts[i]]:
        return starts[i]
    return None


md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
for val in vals:
    needle = struct.pack("<I", val)
    print("== 0x%x" % val)
    pos = tptr
    seen = set()
    while True:
        i = data.find(needle, pos, tptr + trsize)
        if i < 0:
            break
        pos = i + 1
        # decode a window that ends just past the hit; keep instructions whose operand is the value
        for back in range(1, 12):
            start = i - back
            ok = False
            for insn in md.disasm(data[start:i + 8], 0):
                if insn.address + insn.size < back:  # instruction ended before the hit
                    continue
                if insn.address > back:
                    break
                for op in insn.operands:
                    if (op.type == CS_OP_MEM and op.mem.disp == val) or (op.type == CS_OP_IMM and op.imm == val):
                        rva = start + insn.address - tptr + tva
                        if rva not in seen:
                            seen.add(rva)
                            f = func_of(rva)
                            print("  0x%08x  %-6s %-36s  func %s" % (rva, insn.mnemonic, insn.op_str, "0x%x" % f if f else "?"))
                        ok = True
                break
            if ok:
                break
