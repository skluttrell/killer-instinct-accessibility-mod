"""Print the first bytes of PAK entries whose name contains a substring, using a table TSV from pak_table.py.
Usage: python -I peek_entry.py <pak> <table.tsv> <substr> [max_entries] [nbytes]
"""
import re
import sys

pak, tsv, sub = sys.argv[1], sys.argv[2], sys.argv[3].lower()
maxn = int(sys.argv[4]) if len(sys.argv) > 4 else 10
nbytes = int(sys.argv[5]) if len(sys.argv) > 5 else 96

rows = []
with open(tsv, encoding="utf-8") as f:
    next(f)
    for line in f:
        p = line.rstrip("\n").split("\t")
        if sub in p[1].lower():
            rows.append(p)
print("%d matching entries (showing %d)" % (len(rows), min(maxn, len(rows))))
with open(pak, "rb") as f:
    for p in rows[:maxn]:
        off, size, ext = int(p[2]), int(p[3]), p[4]
        f.seek(off)
        head = f.read(nbytes)
        print("\n%s  off=%d size=%d ext=%s flags=%s" % (p[1], off, size, ext, p[5]))
        for i in range(0, len(head), 32):
            row = head[i:i + 32]
            print("   %-96s %s" % (row.hex(" "), re.sub(rb"[^\x20-\x7e]", b".", row).decode()))
