"""Enumerate name->function registrations (Lua bindings, console commands) by pattern-scanning .text:
within a short window: lea r8|rax,[code]  ...  lea rdx,[string]  ...  call <registrar>.
Usage: python luabinds.py <exe> <out.tsv>
"""
import re
import struct
import sys
from collections import Counter

from capstone import CS_ARCH_X86, CS_MODE_64, CS_OP_IMM, CS_OP_MEM, Cs
from capstone.x86 import X86_REG_RIP, X86_REG_RDX, X86_REG_R8, X86_REG_RAX, X86_REG_RCX, X86_REG_R9

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
exe, out = sys.argv[1], sys.argv[2]
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


def cstr_at(rva):
    off = rva2off(rva)
    if off is None:
        return None
    m = re.match(rb"[\x20-\x7e]{2,120}\x00", data[off:off + 121])
    return m.group()[:-1].decode() if m else None


md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
md.skipdata = True
code = data[text[3]:text[3] + text[4]]
recent = []  # (index, reg, target_rva, is_code)
results = []
i = 0
for insn in md.disasm(code, image_base + tlo):
    i += 1
    if insn.id == 0:
        continue
    if insn.mnemonic == "lea" and len(insn.operands) == 2 and insn.operands[1].type == CS_OP_MEM and insn.operands[1].mem.base == X86_REG_RIP:
        tgt = insn.address + insn.size + insn.operands[1].mem.disp - image_base
        recent.append((i, insn.operands[0].reg, tgt, tlo <= tgt < thi))
        recent = [r for r in recent if i - r[0] <= 8]
    elif insn.mnemonic == "call" and insn.operands and insn.operands[0].type == CS_OP_IMM:
        callee = insn.operands[0].imm - image_base
        name_rva = next((r[2] for r in reversed(recent) if r[1] == X86_REG_RDX and not r[3]), None)
        func_rva = next((r[2] for r in reversed(recent) if r[1] in (X86_REG_R8, X86_REG_RAX, X86_REG_R9) and r[3]), None)
        if name_rva is not None and func_rva is not None:
            name = cstr_at(name_rva)
            if name and re.match(r"^[A-Za-z_][A-Za-z0-9_.:]*$", name):
                results.append((callee, name, func_rva, insn.address - image_base))
        recent = [r for r in recent if i - r[0] <= 8]

by_callee = Counter(r[0] for r in results)
print("registration-like call sites:", len(results))
print("top registrars (callee RVA: count):")
for callee, n in by_callee.most_common(12):
    print("   0x%x: %d" % (callee, n))
with open(out, "w", encoding="utf-8") as f:
    f.write("registrar\tname\tfunc_rva\tsite_rva\n")
    for callee, name, func, site in sorted(results, key=lambda r: (r[0], r[1])):
        f.write("0x%x\t%s\t0x%x\t0x%x\n" % (callee, name, func, site))
print("wrote", out)
