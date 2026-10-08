"""Decode a KI localization table (ext 26).
Layout (inferred): u32 version=1, u32 count, u32 table_off(=16), u32 blob_off, then count x (u32 hash, u32 str_off), then a string blob.
Usage: python -I strings_decode.py <strings.bin> <out.tsv>
"""
import struct
import sys

data = open(sys.argv[1], "rb").read()
ver, count, tab, blob = struct.unpack_from("<IIII", data, 0)
print("version", ver, "count", count, "table_off", tab, "blob_off", blob, "filesize", len(data))
assert tab + count * 8 == blob, "blob offset mismatch: %d vs %d" % (tab + count * 8, blob)

entries = [struct.unpack_from("<II", data, tab + 8 * i) for i in range(count)]


def read_str(off):
    p = blob + off
    # try UTF-8 NUL-terminated first; fall back to UTF-16 if the result is tiny and the next bytes look wide
    end = data.index(b"\0", p)
    raw = data[p:end]
    try:
        return raw.decode("utf-8"), "u8"
    except UnicodeDecodeError:
        end16 = data.index(b"\0\0", p)
        end16 += end16 % 2
        return data[p:end16].decode("utf-16-le", errors="replace"), "u16"


n_u8 = n_u16 = 0
with open(sys.argv[2], "w", encoding="utf-8") as out:
    out.write("hash\toffset\ttext\n")
    for h, off in entries:
        s, kind = read_str(off)
        if kind == "u8":
            n_u8 += 1
        else:
            n_u16 += 1
        out.write("%08x\t%d\t%s\n" % (h, off, s.replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r")))
print("decoded utf8:", n_u8, "utf16:", n_u16)
print("first 25:")
for h, off in entries[:25]:
    print("  %08x  %r" % (h, read_str(off)[0][:80]))
