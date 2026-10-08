"""Recover localization KEY -> TEXT pairs. The table stores crc32(key.lower()) -> text (verified with 9/9 AS3 keys).
Harvest candidate keys (UPPER_SNAKE identifiers) from decompiled .as files, the exe, and XML, keep the ones whose
hash exists in the table, and write key\thash\ttext.
Usage: python -I key_harvest.py <strings_en.tsv> <out.tsv> <source1> [<source2> ...]   (sources: files or folders)
"""
import os
import re
import sys
import zlib

tsv, out = sys.argv[1], sys.argv[2]
table = {}
with open(tsv, encoding="utf-8") as f:
    next(f)
    for line in f:
        h, _, text = line.rstrip("\n").split("\t", 2)
        table[int(h, 16)] = text
print("table entries:", len(table))

KEY_RE = re.compile(rb"[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+")  # UPPER_SNAKE with at least one underscore
cands = set()


def scan(path):
    with open(path, "rb") as fh:
        data = fh.read()
    for m in KEY_RE.finditer(data):
        s = m.group()
        if 4 <= len(s) <= 80:
            cands.add(s.decode())


for src in sys.argv[3:]:
    if os.path.isdir(src):
        for root, _, files in os.walk(src):
            for fn in files:
                scan(os.path.join(root, fn))
    else:
        scan(src)
print("candidate identifiers:", len(cands))

found = {}
for k in cands:
    h = zlib.crc32(k.lower().encode("utf-8")) & 0xFFFFFFFF
    if h in table:
        found[k] = (h, table[h])
with open(out, "w", encoding="utf-8") as o:
    o.write("key\thash\ttext\n")
    for k in sorted(found):
        o.write("%s\t%08x\t%s\n" % (k, found[k][0], found[k][1]))
print("keys resolved:", len(found), "->", out)
for k in sorted(found)[:30]:
    print("  %-45s %s" % (k, found[k][1][:70]))
