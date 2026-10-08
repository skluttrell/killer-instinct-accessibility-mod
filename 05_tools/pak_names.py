"""Dump every unique path-like name found in a PAK (streamed) to a text file.
Usage: python -I pak_names.py <pak> <out.txt>
"""
import re
import sys

PATH_RE = re.compile(rb"(?:interface|gameinfo|levels|characters|animation|audio|fonts|shaders|textures|ui)[\\/][\x21-\x7e]{3,160}", re.I)
CHUNK = 32 * 1024 * 1024
OVERLAP = 512

names = set()
with open(sys.argv[1], "rb") as f:
    tail = b""
    while True:
        buf = f.read(CHUNK)
        if not buf:
            break
        data = tail + buf
        for m in PATH_RE.finditer(data):
            s = re.split(rb"[\x00\"'<>|]", m.group())[0]
            if len(s) > 8:
                names.add(s.decode())
        tail = data[-OVERLAP:]

with open(sys.argv[2], "w", encoding="utf-8") as out:
    for n in sorted(names):
        out.write(n + "\n")
print("wrote %d names to %s" % (len(names), sys.argv[2]))
