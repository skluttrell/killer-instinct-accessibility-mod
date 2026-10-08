"""Characterize the ext-58 (Lua script) blobs: entropy, byte histogram, and whether the same script in two paks is identical.
Also lists compression/crypto library names present in the exe.
Usage: python -I lua_probe.py <exe> <blob1> [<blob2>]
"""
import math
import re
import sys
from collections import Counter

sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def entropy(b):
    c = Counter(b)
    n = len(b)
    return -sum(v / n * math.log2(v / n) for v in c.values())


exe = open(sys.argv[1], "rb").read()
print("exe library hints:")
for kw in (b"lz4", b"LZ4", b"zlib", b"inflate", b"deflate", b"oodle", b"Oodle", b"Kraken", b"snappy", b"lzma", b"LZMA", b"AES", b"Rijndael", b"blowfish", b"XTEA", b"TEA", b"RC4", b"xxhash", b"luaL_loadbuffer", b"luac", b"\x1bLua", b"LuaJIT", b"LJ", b"bytecode", b"Bytecode", b"decrypt", b"Decrypt", b"encrypt", b"Encrypt", b"ScriptKey", b"script key"):
    n = exe.count(kw)
    if n:
        print("  %6d  %r" % (n, kw))

for p in sys.argv[2:]:
    b = open(p, "rb").read()
    print("\n%s: %d bytes, entropy %.3f bits/byte" % (p.rsplit("\\", 1)[-1], len(b), entropy(b)))
    print("  head:", b[:16].hex(" "), " tail:", b[-16:].hex(" "))
    print("  size mod 16 = %d, mod 8 = %d" % (len(b) % 16, len(b) % 8))
    print("  printable run count (>=6):", len(re.findall(rb"[\x20-\x7e]{6,}", b)))
if len(sys.argv) > 3:
    a = open(sys.argv[2], "rb").read()
    b = open(sys.argv[3], "rb").read()
    print("\nblob1 == blob2:", a == b, "(sizes %d / %d)" % (len(a), len(b)))
