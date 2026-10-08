"""Copy PAK entries (by name substring, via a pak_table.py TSV) into an output folder as plain data files.
Usage: python -I extract_entry.py <pak> <table.tsv> <substr> <outdir>
"""
import os
import re
import sys

pak, tsv, sub, outdir = sys.argv[1], sys.argv[2], sys.argv[3].lower(), sys.argv[4]
os.makedirs(outdir, exist_ok=True)
n = 0
with open(tsv, encoding="utf-8") as f, open(pak, "rb") as src:
    next(f)
    for line in f:
        p = line.rstrip("\n").split("\t")
        if sub in p[1].lower():
            src.seek(int(p[2]))
            blob = src.read(int(p[3]))
            safe = re.sub(r"[\\/:]", "_", p[1]) + ".ext%s.bin" % p[4]
            with open(os.path.join(outdir, safe), "wb") as o:
                o.write(blob)
            n += 1
            print("  %s (%d bytes)" % (safe, len(blob)))
print("extracted", n)
