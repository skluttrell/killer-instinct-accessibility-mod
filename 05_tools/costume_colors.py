"""Derive a one-line description of every costume colour from the game's own textures, read-only.

Each fighter's colour maps live in PAK\DX11\SPLIT_CHAR_<NAME>.PAK (default costume: characters\<f>\<f>_cm plus
<f>_cm_variationN) and RETRO<NAME>.PAK (characters\retro<f>\retro<f>_cm plus _cm_vN). A texture record (pak type 4) is
a header, the entry name as a NUL-terminated string, then DXT5 (BC3) blocks of mip 0 and its mips. Decoded 2026-10-10
("jago_cm_variation3": 2048 x 4096, data at the byte after the name).

For each variation the texture is compared with the base map: the pixels that changed colour are named (hue / sat /
lightness buckets) in both the base and the variation, by area, and written as
  "Colour 3: the default's dark green and red parts become black and blue."
Usage: python costume_colors.py <pak dir> <out.json> [--only jago,kimwu] [--preview <dir>]
"""
import argparse
import colorsys
import io
import json
import os
import re
import struct
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
# characters\<folder>\<stem>  where the stem starts with the folder name, contains "_cm" and optionally a variation token
# in one of the spellings seen: _variation3, _variation_3, _var3, _v3 (before or after "_cm"). Entries in sub-folders
# (AltCostume\accNN, the purchasable accessories) are ignored; so are the special skins (mimic, shadow, gold, ...).
CM_RE = re.compile(r"^characters\\([^\\]+)\\([a-z0-9_]+)$", re.I)
VAR_RE = re.compile(r"_(?:variation_?|var|v)(\d+)(?=_|$)", re.I)
SKIP_RE = re.compile(r"_(mimic|shadow|gold|silver|stone|mirror|bronze|iron|wood|wrapmask|hand)(_|$)", re.I)


def pak_table(pak, cache_dir):
    tsv = os.path.join(cache_dir, os.path.basename(pak) + ".table.tsv")
    if not os.path.exists(tsv):
        subprocess.run([sys.executable, "-I", os.path.join(HERE, "pak_table.py"), pak, "list", tsv], check=True, capture_output=True)
    rows = []
    with open(tsv, encoding="utf-8") as f:
        next(f)
        for line in f:
            p = line.rstrip("\n").split("\t")
            rows.append((p[1], int(p[2]), int(p[3]), int(p[4])))
    return rows


def dds_dxt5(blocks, w, h):
    hdr = struct.pack("<4sI I I I I I I 11I", b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000, h, w, w * h, 0, 1, *([0] * 11))
    pf = struct.pack("<I I 4s I I I I I", 32, 0x4, b"DXT5", 0, 0, 0, 0, 0)
    caps = struct.pack("<I I I I I", 0x1000, 0, 0, 0, 0)
    return hdr + pf + caps + blocks


DIMS = [(4096, 4096), (2048, 4096), (4096, 2048), (2048, 2048), (1024, 2048), (2048, 1024), (1024, 1024), (512, 1024), (1024, 512), (512, 512),
        (256, 512), (512, 256), (256, 256)]


def decode_record(b):
    """Return the mip-0 RGB image (numpy uint8, h x w x 3) of a texture record, or None."""
    i = b.find(b"characters")
    if i < 0:
        return None
    off = b.index(b"\x00", i) + 1
    remaining = len(b) - off
    # DXT5 = 1 byte per pixel; records carry a full mip chain (4/3) or mip 0 only
    cands = [(w, h) for w, h in DIMS if 0 <= remaining - w * h * 4 // 3 < 4096 or 0 <= remaining - w * h < 4096]
    best = None
    for w, h in cands:
        try:
            im = Image.open(io.BytesIO(dds_dxt5(b[off:off + w * h], w, h)))
            im.load()
            a = np.asarray(im.convert("RGB"))
        except Exception:
            continue
        strip = a[: min(h, 256)].astype(np.int16)
        score = np.abs(np.diff(strip, axis=1)).mean()
        if best is None or score < best[0]:
            best = (score, a)
    return None if best is None else best[1]


def shrink(a, width=256):
    h, w = a.shape[:2]
    f = max(1, w // width)
    return a[: h // f * f, : w // f * f].reshape(h // f, f, w // f, f, 3).mean(axis=(1, 3))


def colour_name(rgb):
    r, g, b = [v / 255.0 for v in rgb]
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    deg = h * 360
    if l < 0.09:
        return "black"
    if s < 0.13 or (l > 0.9 and s < 0.3):
        if l < 0.3:
            return "dark grey"
        if l < 0.62:
            return "grey"
        if l < 0.85:
            return "light grey"
        return "white"
    if l > 0.82 and s < 0.5:
        return "pale " + hue_word(deg)
    if 15 <= deg < 50 and (l < 0.3 or (l < 0.45 and s < 0.45)):
        return "brown"
    if 20 <= deg < 50 and l >= 0.45 and s < 0.45:
        return "tan"
    if 28 <= deg < 62 and 0.3 <= l < 0.72 and s >= 0.45:
        return "gold"
    word = hue_word(deg)
    if l < 0.28:
        return "dark blue" if word == "light blue" else "dark " + word
    if l > 0.72:
        return word if word.startswith("light") else "light " + word
    return word


def hue_word(deg):
    if deg < 15 or deg >= 345:
        return "red"
    if deg < 42:
        return "orange"
    if deg < 66:
        return "yellow"
    if deg < 90:
        return "yellow-green"
    if deg < 160:
        return "green"
    if deg < 195:
        return "teal"
    if deg < 220:
        return "light blue"
    if deg < 262:
        return "blue"
    if deg < 292:
        return "purple"
    if deg < 345:
        return "pink"
    return "red"


def name_shares(pixels):
    """pixels: N x 3 float array -> list of (name, share) by area, merged by name, >= 8% only, max 3."""
    if len(pixels) == 0:
        return []
    q = (pixels // 16).astype(np.int32)  # 16-level bins per channel, named per bin
    keys = q[:, 0] * 256 + q[:, 1] * 16 + q[:, 2]
    vals, counts = np.unique(keys, return_counts=True)
    shares = {}
    for k, c in zip(vals, counts):
        rgb = ((int(k) // 256) * 16 + 8, ((int(k) // 16) % 16) * 16 + 8, (int(k) % 16) * 16 + 8)
        n = colour_name(rgb)
        shares[n] = shares.get(n, 0) + c
    total = float(len(pixels))
    out = sorted(((n, c / total) for n, c in shares.items()), key=lambda t: -t[1])
    return [(n, s) for n, s in out if s >= 0.08][:3]


def join_names(names):
    names = [n for n, _ in names]
    if not names:
        return ""
    if len(names) == 1:
        return names[0]
    return ", ".join(names[:-1]) + " and " + names[-1]


def describe(base, var, n):
    b = shrink(base)
    v = shrink(var)
    if b.shape != v.shape:
        return "Colour %d: different layout, not compared." % n, {}
    dark = (b.max(axis=2) < 20) & (v.max(axis=2) < 20)   # empty atlas space
    diff = np.abs(b - v).max(axis=2)
    changed = (diff > 28) & ~dark
    area = changed.sum() / float((~dark).sum() or 1)
    stats = {"changed_share": round(float(area), 3)}
    if area < 0.03:
        return "Colour %d: nearly the same as the default." % n, stats
    # group the changed pixels by their colour name in the default, and name what each group became
    bp, vp = b[changed], v[changed]
    old_names = np.array([colour_name(tuple(int(c) for c in px)) for px in (bp // 16 * 16 + 8)])
    total = float(len(bp))
    groups = []
    for name in sorted(set(old_names.tolist()), key=lambda nm: -(old_names == nm).sum()):
        sel = old_names == name
        share = sel.sum() / total
        if share < 0.07 or len(groups) >= 3:
            continue
        new = name_shares(vp[sel])
        if not new:
            continue
        new_main = new[0][0]
        if new_main == name and len(new) > 1:
            new_main = new[1][0] if new[1][1] > 0.35 else name
        groups.append((name, new_main, round(float(share), 2)))
    stats["groups"] = groups
    pairs = ["%s becomes %s" % (o, nw) if o != nw else "%s changes shade" % o for o, nw, _ in groups]
    if not pairs:
        return "Colour %d: small changes only." % n, stats
    extent = "most of the costume" if area > 0.55 else ("much of the costume" if area > 0.3 else "parts of the costume")
    return "Colour %d: %s changes; %s." % (n, extent, ", ".join(pairs)), stats


def compose(n, merged, area):
    """merged: (old name, new name, weight) across the parts of one variation -> one sentence."""
    if area < 0.03 or not merged:
        return "Colour %d: nearly the same as the default." % n
    agg = {}
    for o, nw, wgt in merged:
        agg[(o, nw)] = agg.get((o, nw), 0.0) + wgt
    pairs = []
    seen_old = set()
    for (o, nw), wgt in sorted(agg.items(), key=lambda t: -t[1]):
        if o in seen_old or len(pairs) >= 3:
            continue
        seen_old.add(o)
        pairs.append("%s becomes %s" % (o, nw) if o != nw else "%s changes shade" % o)
    extent = "most of the costume" if area > 0.55 else ("much of the costume" if area > 0.3 else "parts of the costume")
    return "Colour %d: %s changes; %s." % (n, extent, ", ".join(pairs))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pakdir")
    ap.add_argument("out")
    ap.add_argument("--only", default="")
    ap.add_argument("--preview", default="")
    ap.add_argument("--cache", default=os.path.join(os.environ.get("TEMP", "."), "ki_pak_tables"))
    args = ap.parse_args()
    os.makedirs(args.cache, exist_ok=True)
    only = [s.strip().lower() for s in args.only.split(",") if s.strip()]
    paks = sorted(p for p in os.listdir(args.pakdir) if re.match(r"(SPLIT_CHAR_|RETRO)[A-Z0-9]+\.pak$", p, re.I))
    result, stats_all = {}, {}
    for pak in paks:
        fighter = re.sub(r"^(SPLIT_CHAR_|RETRO)", "", pak, flags=re.I).rsplit(".", 1)[0].lower().rstrip("1").rstrip("2")
        if only and fighter not in only:
            continue
        rows = pak_table(os.path.join(args.pakdir, pak), args.cache)
        groups = {}   # folder -> part -> {variation: (off, size)}
        for name, off, size, ext in rows:
            if ext != 4 or "\\" not in name:
                continue
            m = CM_RE.match(name)
            if not m:
                continue
            folder, stem = m.group(1).lower(), m.group(2).lower()
            first = stem.split("_")[0]
            # the stem normally starts with the folder name; TJ Combo's parts are "tj_accessories", "tj_hair"
            if not (stem.startswith(folder) or (len(first) >= 2 and folder.startswith(first))) or "_cm" not in stem or SKIP_RE.search(stem):
                continue
            vm = VAR_RE.search(stem)
            var = int(vm.group(1)) if vm else 1
            part = VAR_RE.sub("", stem).replace("_cm", "")
            g = groups.setdefault(folder, {}).setdefault(part, {})
            if var not in g or size > g[var][1]:
                g[var] = (off, size)
        with open(os.path.join(args.pakdir, pak), "rb") as f:
            def load(rec):
                f.seek(rec[0])
                return decode_record(f.read(rec[1]))
            for folder, parts in groups.items():
                costume = "retro" if folder.startswith("retro") else "default"
                code = folder[5:] if costume == "retro" else folder
                bases = {}
                for part, vars_ in parts.items():
                    if 1 in vars_ and any(v != 1 for v in vars_):
                        img = load(vars_[1])
                        if img is not None:
                            bases[part] = img
                        else:
                            print(pak, part, "base map undecodable; skipped")
                if not bases:
                    print(pak, folder, "has no usable base colour map; skipped")
                    continue
                texts = result.setdefault(code, {}).setdefault(costume, {})
                st = stats_all.setdefault(code, {}).setdefault(costume, {})
                if args.preview:
                    for part, img in bases.items():
                        Image.fromarray(shrink(img).astype(np.uint8)).save(os.path.join(args.preview, "%s_%s_%s_1.png" % (code, costume, part)))
                allvars = sorted({v for p in bases for v in parts[p] if v != 1})
                for var in allvars:
                    merged = []   # (old, new, weighted share) across the parts that have this variation
                    area = 0.0
                    for part, base in bases.items():
                        if var not in parts[part]:
                            continue
                        img = load(parts[part][var])
                        if img is None:
                            continue
                        text, stats = describe(base, img, var)
                        weight = base.shape[0] * base.shape[1]
                        area = max(area, stats.get("changed_share", 0.0))
                        for o, nw, s in stats.get("groups", []):
                            merged.append((o, nw, s * weight))
                        if args.preview:
                            Image.fromarray(shrink(img).astype(np.uint8)).save(os.path.join(args.preview, "%s_%s_%s_%d.png" % (code, costume, part, var)))
                    text = compose(var, merged, area)
                    texts[str(var)] = text
                    st[str(var)] = {"changed_share": round(area, 3), "groups": [(o, nw, round(s, 1)) for o, nw, s in merged]}
                    print("%s %s %s" % (code, costume, text))
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=1, sort_keys=True)
    with open(args.out.replace(".json", "_stats.json"), "w", encoding="utf-8") as f:
        json.dump(stats_all, f, indent=1, sort_keys=True)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
