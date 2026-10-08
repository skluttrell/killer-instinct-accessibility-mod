"""Identify the hash function used by the KI localization table by testing known UI keys against the table's hashes.
Usage: python -I hash_probe.py <strings_en.tsv>
Keys below were read from the decompiled ActionScript (TextManager.SetText key=...).
"""
import sys
import zlib

tsv = sys.argv[1]
hashes = set()
with open(tsv, encoding="utf-8") as f:
    next(f)
    for line in f:
        hashes.add(int(line.split("\t", 1)[0], 16))
print("table hashes:", len(hashes))

KEYS = ["COMMON_SEASON_2", "HUD_XP", "LEGEND_SELECT", "LEGEND_BACK", "NOTIFICATION_CONTENT_NOT_INSTALLED_YET",
        "MAIN_MENU_STORECOLLECTION", "LANDING_PAGE_SINGLEPLAYER", "LANDING_PAGE_MULTIPLAYER", "LANDING_PAGE_GA_MODE"]


def fnv1_32(s):
    h = 0x811C9DC5
    for c in s:
        h = (h * 0x01000193) & 0xFFFFFFFF
        h ^= c
    return h


def fnv1a_32(s):
    h = 0x811C9DC5
    for c in s:
        h ^= c
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


def djb2(s):
    h = 5381
    for c in s:
        h = (h * 33 + c) & 0xFFFFFFFF
    return h


def djb2_xor(s):
    h = 5381
    for c in s:
        h = ((h * 33) ^ c) & 0xFFFFFFFF
    return h


def sdbm(s):
    h = 0
    for c in s:
        h = (c + (h << 6) + (h << 16) - h) & 0xFFFFFFFF
    return h


def crc32(s):
    return zlib.crc32(s) & 0xFFFFFFFF


def crc32_inv(s):
    return (~zlib.crc32(s)) & 0xFFFFFFFF


def jenkins_oaat(s):
    h = 0
    for c in s:
        h = (h + c) & 0xFFFFFFFF
        h = (h + (h << 10)) & 0xFFFFFFFF
        h ^= h >> 6
    h = (h + (h << 3)) & 0xFFFFFFFF
    h ^= h >> 11
    h = (h + (h << 15)) & 0xFFFFFFFF
    return h


def murmur3_32(data, seed=0):
    c1, c2 = 0xCC9E2D51, 0x1B873593
    h = seed
    n = len(data) // 4
    for i in range(n):
        k = int.from_bytes(data[4 * i:4 * i + 4], "little")
        k = (k * c1) & 0xFFFFFFFF
        k = ((k << 15) | (k >> 17)) & 0xFFFFFFFF
        k = (k * c2) & 0xFFFFFFFF
        h ^= k
        h = ((h << 13) | (h >> 19)) & 0xFFFFFFFF
        h = (h * 5 + 0xE6546B64) & 0xFFFFFFFF
    tail = data[4 * n:]
    k = 0
    if len(tail) >= 3:
        k ^= tail[2] << 16
    if len(tail) >= 2:
        k ^= tail[1] << 8
    if len(tail) >= 1:
        k ^= tail[0]
        k = (k * c1) & 0xFFFFFFFF
        k = ((k << 15) | (k >> 17)) & 0xFFFFFFFF
        k = (k * c2) & 0xFFFFFFFF
        h ^= k
    h ^= len(data)
    h ^= h >> 16
    h = (h * 0x85EBCA6B) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 0xC2B2AE35) & 0xFFFFFFFF
    h ^= h >> 16
    return h


FUNCS = {"fnv1_32": fnv1_32, "fnv1a_32": fnv1a_32, "djb2": djb2, "djb2_xor": djb2_xor, "sdbm": sdbm,
         "crc32": crc32, "crc32_inv": crc32_inv, "jenkins_oaat": jenkins_oaat, "murmur3_32": murmur3_32}
VARIANTS = {"as-is": lambda k: k, "lower": str.lower, "upper": str.upper}

for fname, fn in FUNCS.items():
    for vname, vf in VARIANTS.items():
        for enc in ("utf-8", "utf-16-le"):
            n = 0
            for k in KEYS:
                if fn(vf(k).encode(enc)) in hashes:
                    n += 1
            if n:
                print("MATCH %-13s %-6s %-9s %d/%d keys" % (fname, vname, enc, n, len(KEYS)))
print("done (no MATCH lines means none of the candidate hashes fit)")
