"""Static cross-reference finder: which code addresses reference given strings in the exe.
Linear-sweeps .text with Capstone and records RIP-relative operands that point at the wanted strings.
Usage: python -I xref.py <exe> "<string1>" "<string2>" ...
Output: for each string, its RVA and the RVAs (and nearest preceding function-ish boundary) of referencing instructions.
Addresses are RVAs; add the module base at runtime (Frida: Module.findBaseAddress('KILLERINSTINCTX64_R.EXE')).
"""
import struct
import sys
import time

from capstone import CS_ARCH_X86, CS_MODE_64, Cs, CS_OP_MEM
from capstone.x86 import X86_REG_RIP

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
exe = sys.argv[1]
wanted = [w.encode() for w in sys.argv[2:]]
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


# locate strings (NUL-terminated, exact) -> RVAs
targets = {}
for w in wanted:
    pos = 0
    while True:
        i = data.find(w + b"\0", pos)
        if i < 0:
            break
        if i == 0 or data[i - 1] == 0:  # starts at a string boundary
            targets.setdefault(w, []).append(off2rva(i))
        pos = i + 1
for w, rvas in targets.items():
    print("string %-50r at RVAs %s" % (w.decode(), [hex(r) for r in rvas]))
target_set = {r for rvas in targets.values() for r in rvas}
if not target_set:
    sys.exit("no target strings found")

text = next(s for s in sections if s[0] == ".text")
_, tva, tvsize, tptr, trsize = text
code = data[tptr:tptr + trsize]
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
md.skipdata = True
hits = {}
t0 = time.time()
count = 0
for insn in md.disasm(code, image_base + tva):
    count += 1
    if insn.id == 0:  # skipdata placeholder, no operand info
        continue
    if not insn.operands:
        continue
    for op in insn.operands:
        if op.type == CS_OP_MEM and op.mem.base == X86_REG_RIP:
            tgt = insn.address + insn.size + op.mem.disp - image_base
            if tgt in target_set:
                hits.setdefault(tgt, []).append((insn.address - image_base, insn.mnemonic + " " + insn.op_str))
print("disassembled %d instructions in %.0fs" % (count, time.time() - t0))
rev = {r: w for w, rvas in targets.items() for r in rvas}
for tgt, lst in hits.items():
    print("\n%r (RVA 0x%x): %d references" % (rev[tgt].decode(), tgt, len(lst)))
    for rva, txt in lst[:20]:
        print("   code RVA 0x%08x   %s" % (rva, txt))
