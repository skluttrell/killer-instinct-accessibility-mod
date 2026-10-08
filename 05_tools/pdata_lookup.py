"""Map code RVAs to function boundaries using the exe's .pdata (RUNTIME_FUNCTION table).
Usage: python -I pdata_lookup.py <exe> 0xRVA [0xRVA ...]
"""
import bisect
import struct
import sys

exe = sys.argv[1]
data = open(exe, "rb").read()
e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
coff = e_lfanew + 4
nsec = struct.unpack_from("<H", data, coff + 2)[0]
opt_size = struct.unpack_from("<H", data, coff + 16)[0]
opt = coff + 20
dd_off = opt + 112
exc_rva, exc_size = struct.unpack_from("<II", data, dd_off + 8 * 3)
sec_off = opt + opt_size
sections = []
for i in range(nsec):
    name, vsize, va, rsize, rptr = struct.unpack_from("<8sIIII", data, sec_off + 40 * i)
    sections.append((va, vsize, rptr, rsize))


def rva2off(rva):
    for va, vsize, rptr, rsize in sections:
        if va <= rva < va + rsize:
            return rva - va + rptr


off = rva2off(exc_rva)
funcs = []
for i in range(exc_size // 12):
    begin, end, unwind = struct.unpack_from("<III", data, off + 12 * i)
    funcs.append((begin, end))
funcs.sort()
starts = [f[0] for f in funcs]
print("runtime functions:", len(funcs))
for a in sys.argv[2:]:
    rva = int(a, 16)
    i = bisect.bisect_right(starts, rva) - 1
    if i >= 0 and funcs[i][0] <= rva < funcs[i][1]:
        # chained unwind entries split one function into several records; merge contiguous ones
        b, e = funcs[i]
        j = i
        while j > 0 and funcs[j - 1][1] == funcs[j][0]:
            j -= 1
        b = funcs[j][0]
        k = i
        while k + 1 < len(funcs) and funcs[k][1] == funcs[k + 1][0]:
            k += 1
        e = funcs[k][1]
        print("RVA 0x%x -> function 0x%x .. 0x%x (%d bytes)" % (rva, b, e, e - b))
    else:
        print("RVA 0x%x -> no function record" % rva)
