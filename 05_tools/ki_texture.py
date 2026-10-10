"""Decode a Killer Instinct texture record (pak type 4) as seen in SPLIT_CHAR_*.PAK, read-only.
Hypothesis under test (2026-10-10): header of ~0x6cc bytes (name string at the offset stored at +0x5c, then the record
list), followed by BC1 blocks of a 4096x4096 image with 13 mips.
Usage: python ki_texture.py <record.bin> <out.png> [--offset 0x6cc] [--size 4096] [--scale 4] [--format bc1|bc3|bc7]
Writes the top mip downscaled by --scale (block-averaged) as a PNG.
"""
import argparse
import struct

import numpy as np
from PIL import Image


def bc1_decode(data, w, h):
    """Decode BC1 (DXT1) blocks to an RGB uint8 array (w, h multiples of 4)."""
    bw, bh = w // 4, h // 4
    blocks = np.frombuffer(data, dtype="<u2", count=bw * bh * 4).reshape(bh, bw, 4)
    c0 = blocks[:, :, 0].astype(np.uint32)
    c1 = blocks[:, :, 1].astype(np.uint32)
    idx = (blocks[:, :, 2].astype(np.uint32) | (blocks[:, :, 3].astype(np.uint32) << 16))

    def rgb565(c):
        r = ((c >> 11) & 31) * 255 // 31
        g = ((c >> 5) & 63) * 255 // 63
        b = (c & 31) * 255 // 31
        return np.stack([r, g, b], axis=-1).astype(np.int32)

    p0, p1 = rgb565(c0), rgb565(c1)
    four = (c0 > c1)[:, :, None]
    p2 = np.where(four, (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(four, (p0 + 2 * p1) // 3, 0)
    palette = np.stack([p0, p1, p2, p3], axis=2)  # bh, bw, 4, 3
    out = np.zeros((h, w, 3), dtype=np.uint8)
    for py in range(4):
        for px in range(4):
            shift = 2 * (py * 4 + px)
            sel = (idx >> shift) & 3
            out[py::4, px::4] = np.take_along_axis(palette, sel[:, :, None, None].repeat(3, axis=3), axis=2)[:, :, 0]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("record")
    ap.add_argument("out")
    ap.add_argument("--offset", default="0x6cc")
    ap.add_argument("--size", type=int, default=4096)
    ap.add_argument("--scale", type=int, default=4)
    args = ap.parse_args()
    b = open(args.record, "rb").read()
    off = int(args.offset, 0)
    w = h = args.size
    need = w * h // 2
    print("record", len(b), "bytes; decoding", w, "x", h, "BC1 from", hex(off), "need", need)
    img = bc1_decode(b[off:off + need], w, h)
    s = args.scale
    small = img.reshape(h // s, s, w // s, s, 3).mean(axis=(1, 3)).astype(np.uint8)
    Image.fromarray(small).save(args.out)
    print("wrote", args.out, small.shape)
    # dominant colours of the full image, coarse 6-bit bins
    q = (img >> 2).reshape(-1, 3)
    keys = q[:, 0].astype(np.int32) * 4096 + q[:, 1] * 64 + q[:, 2]
    vals, counts = np.unique(keys, return_counts=True)
    order = np.argsort(-counts)[:8]
    for i in order:
        k = vals[i]
        print("  colour (%3d,%3d,%3d) share %.1f%%" % ((k // 4096) * 4, ((k // 64) % 64) * 4, (k % 64) * 4, 100.0 * counts[i] / keys.size))


if __name__ == "__main__":
    main()
