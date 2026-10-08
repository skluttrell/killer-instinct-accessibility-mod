"""Read-only KI PAK_ v4 table parser.
Record = char name[240] (NUL-terminated, 0xCD padded) + u8 flags[4] + u64 offset + u32 size + u16 ext + u16 a + u16 b  (262 bytes)
Validated on patch.pak: offset+size of the last entry == file size.

Usage:
  python -I pak_table.py <pak> list <out.tsv>             # write every record to a TSV
  python -I pak_table.py <pak> peek                        # list records + first 48 bytes of each (small paks only)
  python -I pak_table.py <pak> extract <substr> <outdir>   # copy entries whose name contains substr into outdir (data only, never executed)
"""
import os
import re
import struct
import sys

REC = 262
NAME = 240
CHUNK = 64 * 1024 * 1024

pak, mode = sys.argv[1], sys.argv[2]
fsize = os.path.getsize(pak)
NAME_RE = re.compile(rb"[\x21-\x7e]{6,239}\x00")


def parse_trailer(t):
    flags = t[0:4]
    off, size, ext, a, b = struct.unpack_from("<QIHHH", t, 4)
    return flags, off, size, ext, a, b


def valid(name, trailer):
    if b"\\" not in name and b"/" not in name:
        return False
    flags, off, size, ext, a, b = parse_trailer(trailer)
    return off < fsize and size < fsize and off + size <= fsize and off >= 64


records = []
with open(pak, "rb") as f:
    base = 0
    tail = b""
    while True:
        buf = f.read(CHUNK)
        if not buf:
            break
        data = tail + buf
        start_abs = base - len(tail)
        pos = 0
        while True:
            m = NAME_RE.search(data, pos)
            if not m:
                break
            p = m.start()
            if p + REC <= len(data) and valid(m.group()[:-1], data[p + NAME:p + REC]):
                name = m.group()[:-1].decode(errors="replace")
                flags, off, size, ext, a, b = parse_trailer(data[p + NAME:p + REC])
                abs_pos = start_abs + p
                if not records or records[-1][0] != abs_pos:
                    records.append((abs_pos, name, off, size, ext, flags.hex(), a, b))
                pos = p + REC
            else:
                pos = m.end()
        base += len(buf)
        tail = data[-REC:] if len(data) >= REC else data

# de-dup overlaps from chunk boundaries
seen = set()
uniq = []
for r in records:
    if r[0] not in seen:
        seen.add(r[0])
        uniq.append(r)
records = uniq
print("records found: %d in %s (%.1f MB)" % (len(records), os.path.basename(pak), fsize / 1e6))

if mode == "list":
    with open(sys.argv[3], "w", encoding="utf-8") as out:
        out.write("rec_pos\tname\toffset\tsize\text\tflags\ta\tb\n")
        for r in records:
            out.write("%d\t%s\t%d\t%d\t%d\t%s\t%d\t%d\n" % r)
    from collections import Counter
    print("ext code histogram:", sorted(Counter(r[4] for r in records).items()))
    print("wrote", sys.argv[3])

elif mode == "peek":
    with open(pak, "rb") as f:
        for r in records:
            f.seek(r[2])
            head = f.read(48)
            print("%-60s off=%-10d size=%-9d ext=%-3d flags=%s  %s" % (r[1][:60], r[2], r[3], r[4], r[5], re.sub(rb"[^\x20-\x7e]", b".", head).decode()))

elif mode == "extract":
    sub, outdir = sys.argv[3].lower(), sys.argv[4]
    os.makedirs(outdir, exist_ok=True)
    n = 0
    with open(pak, "rb") as f:
        for r in records:
            if sub in r[1].lower():
                f.seek(r[2])
                blob = f.read(r[3])
                safe = re.sub(r"[\\/:]", "_", r[1]) + ".ext%d.bin" % r[4]
                with open(os.path.join(outdir, safe), "wb") as o:
                    o.write(blob)
                n += 1
    print("extracted", n, "entries to", outdir)
