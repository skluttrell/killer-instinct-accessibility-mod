"""Build unique byte signatures for the narrator's hook RVAs from the game exe (read-only).
Usage: python -I aob_sigs.py <exe> -> prints JSON {name: {rva, sig}} where sig is "48 89 5c 24 ?? ..." (?? = wildcard);
also writes 07_dll/sigs.json. Wildcards cover RIP-relative displacements and absolute immediates.
"""
import hashlib
import json
import os
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_64, CS_OP_IMM, CS_OP_MEM, Cs
from capstone.x86 import X86_REG_RIP

exe = sys.argv[1]
data = open(exe, "rb").read()
e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
coff = e_lfanew + 4
nsec = struct.unpack_from("<H", data, coff + 2)[0]
opt_size = struct.unpack_from("<H", data, coff + 16)[0]
opt = coff + 20
sec_off = opt + opt_size
sections = []
for i in range(nsec):
    name, vsize, va, rsize, rptr = struct.unpack_from("<8sIIII", data, sec_off + 40 * i)
    sections.append((name.rstrip(b"\0").decode(), va, vsize, rptr, rsize))
text = next(s for s in sections if s[0] == ".text")
tva, tptr, tsize = text[1], text[3], text[4]
textbytes = data[tptr:tptr + tsize]
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

RVAS = {"dispatcher": 0x5dfe50, "rootInvoke": 0x1122ef0, "objInvoke": 0x1150a00, "objGetMember": 0x11502e0,
        "valueRelease": 0x350bd0, "advance": 0xe4a700, "matchState": 0x9d7240, "roundState": 0x74d520, "levelState": 0x68b020}


def signature(rva, maxlen=48):
    off = rva - tva
    pat = []
    for insn in md.disasm(bytes(textbytes[off:off + maxlen + 16]), rva):
        b = list(insn.bytes)
        mask = [True] * len(b)
        # wildcard displacement / immediate bytes that depend on layout
        for op in insn.operands:
            if op.type == CS_OP_MEM and op.mem.base == X86_REG_RIP:
                for k in range(insn.disp_offset, insn.disp_offset + insn.disp_size):
                    mask[k] = False
            elif op.type == CS_OP_IMM and insn.imm_size >= 4:
                for k in range(insn.imm_offset, insn.imm_offset + insn.imm_size):
                    mask[k] = False
        if insn.mnemonic.startswith("call") or insn.mnemonic.startswith("j"):
            for k in range(1, len(b)):
                mask[k] = False
        pat += [("%02x" % x) if m else "??" for x, m in zip(b, mask)]
        if len(pat) >= maxlen:
            break
    return pat


def count_matches(pat):
    first = pat[0]
    n = 0
    plen = len(pat)
    fb = bytes.fromhex(first)
    i = textbytes.find(fb)
    while i != -1 and n < 2:
        if all(p == "??" or textbytes[i + k] == int(p, 16) for k, p in enumerate(pat)):
            n += 1
        i = textbytes.find(fb, i + 1)
    return n


out = {"sha256": hashlib.sha256(data).hexdigest(), "sigs": {}}
for name, rva in RVAS.items():
    for ln in (24, 32, 48, 64):
        pat = signature(rva, ln)
        n = count_matches(pat)
        if n == 1:
            break
    out["sigs"][name] = {"rva": "0x%x" % rva, "sig": " ".join(pat), "matches": n}
    print("%-13s rva 0x%x len %2d matches %d" % (name, rva, len(pat), n))
dst = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "07_dll", "sigs.json")
json.dump(out, open(dst, "w"), indent=1)
print("wrote", dst)
