"""Inflate a Scaleform CFX/GFX movie and list the tags that matter for narration:
DefineEditText (TextField var names + initial text), SymbolClass/ExportAssets names,
DoABC constant-pool strings (AS3 class/method/property names), and GFx exporter tags.
Usage: python -I swf_inspect.py <movie.bin> [outdir]
"""
import os
import re
import struct
import sys
import zlib

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

path = sys.argv[1]
raw = open(path, "rb").read()
sig = raw[:3]
ver = raw[3]
flen = struct.unpack_from("<I", raw, 4)[0]
if sig in (b"CFX", b"CWS"):
    body = zlib.decompress(raw[8:])
    data = (b"GFX" if sig == b"CFX" else b"FWS") + raw[3:8] + body
elif sig in (b"GFX", b"FWS"):
    data = raw
else:
    sys.exit("not a SWF/GFX: %r" % sig)
print("sig=%s version=%d declared_len=%d actual=%d" % (sig.decode(), ver, flen, len(data)))
if len(sys.argv) > 2:
    os.makedirs(sys.argv[2], exist_ok=True)
    out = os.path.join(sys.argv[2], os.path.basename(path) + ".gfx")
    open(out, "wb").write(data)
    print("wrote decompressed movie to", out)

# header: RECT (nbits in top 5 bits), then u16 frame rate (8.8), u16 frame count
p = 8
nbits = data[p] >> 3
rect_bits = 5 + 4 * nbits
p += (rect_bits + 7) // 8
rate = struct.unpack_from("<H", data, p)[0] / 256.0
count = struct.unpack_from("<H", data, p + 2)[0]
p += 4
print("frame rate %.1f, frames %d" % (rate, count))

TAGS = {0: "End", 1: "ShowFrame", 2: "DefineShape", 9: "SetBackgroundColor", 10: "DefineFont", 11: "DefineText", 13: "DefineFontInfo", 20: "DefineBitsLossless", 21: "DefineBitsJPEG2", 22: "DefineShape2", 26: "PlaceObject2", 28: "RemoveObject2", 32: "DefineShape3", 33: "DefineText2", 34: "DefineButton2", 35: "DefineBitsJPEG3", 36: "DefineBitsLossless2", 37: "DefineEditText", 39: "DefineSprite", 43: "FrameLabel", 48: "DefineFont2", 56: "ExportAssets", 57: "ImportAssets", 65: "ScriptLimits", 69: "FileAttributes", 70: "PlaceObject3", 71: "ImportAssets2", 73: "DefineFontAlignZones", 74: "CSMTextSettings", 75: "DefineFont3", 76: "SymbolClass", 77: "Metadata", 78: "DefineScalingGrid", 82: "DoABC", 83: "DefineShape4", 86: "DefineSceneAndFrameLabelData", 88: "DefineFontName", 1000: "GFxExporterInfo", 1001: "GFxDefineExternalImage", 1002: "GFxFontTextureInfo", 1003: "GFxDefineExternalGradient", 1004: "GFxDefineGradientMap", 1005: "GFxDefineCompactedFont", 1006: "GFxDefineExternalSound", 1007: "GFxDefineExternalStreamSound", 1008: "GFxStartSound", 1009: "GFxDefineExternalImage2", 1010: "GFxDefineSubImage"}


def cstr(buf, off):
    e = buf.index(b"\0", off)
    return buf[off:e].decode("utf-8", errors="replace"), e + 1


from collections import Counter
hist = Counter()
edit_texts = []
symbols = []
abc_strings = []
exporter = None
ext_images = []


def parse_edit_text(b):
    cid = struct.unpack_from("<H", b, 0)[0]
    q = 2
    nb = b[q] >> 3
    q += (5 + 4 * nb + 7) // 8
    flags = struct.unpack_from("<H", b, q)[0]
    q += 2
    # flags read little-endian: low byte = first byte of the tag (HasText=0x80 ...), high byte = second
    has_text = flags & 0x0080
    has_font = flags & 0x0001
    has_max = flags & 0x0002
    has_color = flags & 0x0004
    has_layout = flags & 0x2000
    has_font_class = flags & 0x8000
    if has_font:
        q += 2
    if has_font_class:
        _, q = cstr(b, q)
    if has_font:
        q += 2
    if has_color:
        q += 4
    if has_max:
        q += 2
    if has_layout:
        q += 9
    var, q = cstr(b, q)
    init = ""
    if has_text:
        init, q = cstr(b, q)
    return cid, var, init, flags


def parse_abc(b):
    # DoABC: u32 flags, cstring name, then abcFile: u16 minor, u16 major, then constant pools
    q = 4
    name, q = cstr(b, q)

    def u30():
        nonlocal q
        v = 0
        shift = 0
        while True:
            c = b[q]
            q += 1
            v |= (c & 0x7F) << shift
            if not c & 0x80:
                return v
            shift += 7

    q += 4
    n = u30()
    for _ in range(max(0, n - 1)):
        u30()
    n = u30()
    for _ in range(max(0, n - 1)):
        u30()
    n = u30()
    q += 8 * max(0, n - 1)
    n = u30()
    strs = []
    for _ in range(max(0, n - 1)):
        ln = u30()
        strs.append(b[q:q + ln].decode("utf-8", errors="replace"))
        q += ln
    return name, strs


while p < len(data):
    code_len = struct.unpack_from("<H", data, p)[0]
    code, ln = code_len >> 6, code_len & 0x3F
    p += 2
    if ln == 0x3F:
        ln = struct.unpack_from("<I", data, p)[0]
        p += 4
    body = data[p:p + ln]
    p += ln
    hist[code] += 1
    if code == 37:
        try:
            edit_texts.append(parse_edit_text(body))
        except Exception as e:
            edit_texts.append((-1, "<parse error %s>" % e, "", 0))
    elif code in (56, 76):
        n = struct.unpack_from("<H", body, 0)[0]
        q = 2
        for _ in range(n):
            tag_id = struct.unpack_from("<H", body, q)[0]
            nm, q = cstr(body, q + 2)
            symbols.append((tag_id, nm))
    elif code == 82:
        try:
            abc_strings.append(parse_abc(body))
        except Exception as e:
            abc_strings.append(("<parse error %s>" % e, []))
    elif code == 1000:
        exporter = re.sub(rb"[^\x20-\x7e]", b".", body).decode()
    elif code in (1001, 1009):
        ext_images.append(re.sub(rb"[^\x20-\x7e]", b".", body).decode())
    if code == 0:
        break

print("\ntag histogram:")
for c, n in sorted(hist.items()):
    print("  %4d %-28s x%d" % (c, TAGS.get(c, "?"), n))
if exporter:
    print("\nexporter info:", exporter)
if ext_images:
    print("\nexternal images (%d), first 5:" % len(ext_images))
    for s in ext_images[:5]:
        print("  ", s)

print("\nDefineEditText fields (%d):" % len(edit_texts))
for cid, var, init, flags in edit_texts:
    print("  id=%-4d var=%-40s init=%r" % (cid, var, init[:60]))

print("\nSymbolClass/ExportAssets (%d):" % len(symbols))
for tid, nm in symbols:
    print("  %-5d %s" % (tid, nm))

for name, strs in abc_strings:
    print("\nDoABC %r: %d constant-pool strings" % (name, len(strs)))
    interesting = [s for s in strs if re.search(r"(?i)focus|select|text|label|menu|item|option|cursor|index|External|fscommand|gfx|Lua|call|highlight|navigate|button|list|value|slider|setText|htmlText|name", s)]
    print("  interesting (%d):" % len(interesting))
    for s in interesting[:400]:
        print("    ", s)
