"""Disassemble a range of the exe with string-reference annotations.
Usage: python disasm.py <exe> <start_rva_hex> <length_hex>
"""
import re
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, CS_OP_IMM, Cs
from capstone.x86 import X86_REG_RIP

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
exe, start, length = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
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


def rva2off(rva):
    for _, va, vsize, rptr, rsize in sections:
        if va <= rva < va + rsize:
            return rva - va + rptr


def annotate(rva):
    off = rva2off(rva)
    if off is None:
        return ""
    chunk = data[off:off + 64]
    m = re.match(rb"[\x20-\x7e]{3,}", chunk)
    if m:
        return '  ; "%s"' % m.group()[:48].decode()
    m = re.match(rb"(?:[\x20-\x7e]\x00){3,}", chunk)
    if m:
        return '  ; L"%s"' % m.group().decode("utf-16-le")[:48]
    q = struct.unpack_from("<Q", data, off)[0] - image_base
    if 0 < q < 0x3000000:
        return "  ; -> 0x%x" % q
    return ""


md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
code = data[rva2off(start):rva2off(start) + length]
for insn in md.disasm(code, image_base + start):
    note = ""
    for op in insn.operands:
        if op.type == CS_OP_MEM and op.mem.base == X86_REG_RIP:
            tgt = insn.address + insn.size + op.mem.disp - image_base
            note = "  [rva 0x%x]" % tgt + annotate(tgt)
        elif op.type == CS_OP_IMM and insn.mnemonic in ("call", "jmp") and 0 < op.imm - image_base < 0x3000000:
            note = "  [-> 0x%x]" % (op.imm - image_base)
    print("0x%08x  %-6s %-40s%s" % (insn.address - image_base, insn.mnemonic, insn.op_str, note))
