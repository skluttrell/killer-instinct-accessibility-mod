"""Read-only process memory scanner for Phase 0 C (pure stdlib, Windows only).

Opens KILLERINSTINCTX64_R.EXE with PROCESS_VM_READ | PROCESS_QUERY_INFORMATION only (never write),
walks committed readable regions, and searches for byte patterns.

Usage (run while the game is at the main menu):
  python -I memscan.py find "Single Player" "Multiplayer" "mcMenuBtn0"       # UTF-8 and UTF-16 search
  python -I memscan.py hex "4d 61 69 6e 4d 65 6e 75"                          # raw byte pattern
  python -I memscan.py watch 0x1F2E3D4C5B6A 16 0.25                           # poll an address, print changes
  python -I memscan.py regions                                                 # list regions (size, protect)

Notes:
- Run as the same user that launched the game. If OpenProcess fails, run from an elevated prompt.
- Heap addresses change every launch; use `find` to re-locate, then `watch` to see which bytes change
  as you move the cursor. Integers that step 0,1,2.. with the cursor are candidate SelectionIndex copies.
STATUS: written 2026-10-08, not yet exercised against the running game.
"""
import ctypes
import ctypes.wintypes as wt
import struct
import sys
import time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
MEM_COMMIT = 0x1000
PAGE_NOACCESS = 0x01
PAGE_GUARD = 0x100
TH32CS_SNAPPROCESS = 0x2
EXE = "KILLERINSTINCTX64_R.EXE"


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)), ("th32ModuleID", wt.DWORD),
                ("cntThreads", wt.DWORD), ("th32ParentProcessID", wt.DWORD), ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", wt.DWORD), ("szExeFile", ctypes.c_wchar * 260)]


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wt.DWORD), ("PartitionId", wt.WORD), ("RegionSize", ctypes.c_size_t),
                ("State", wt.DWORD), ("Protect", wt.DWORD), ("Type", wt.DWORD)]


k32.CreateToolhelp32Snapshot.restype = wt.HANDLE
k32.Process32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
k32.Process32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
k32.OpenProcess.restype = wt.HANDLE
k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
k32.VirtualQueryEx.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(MEMORY_BASIC_INFORMATION), ctypes.c_size_t]
k32.VirtualQueryEx.restype = ctypes.c_size_t
k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
k32.ReadProcessMemory.restype = wt.BOOL


def find_pid(name):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    pe = PROCESSENTRY32W()
    pe.dwSize = ctypes.sizeof(pe)
    pid = None
    if k32.Process32FirstW(snap, ctypes.byref(pe)):
        while True:
            if pe.szExeFile.upper() == name.upper():
                pid = pe.th32ProcessID
                break
            if not k32.Process32NextW(snap, ctypes.byref(pe)):
                break
    k32.CloseHandle(snap)
    return pid


def open_proc():
    pid = find_pid(EXE)
    if pid is None:
        sys.exit("game process not found (%s). Launch the game first." % EXE)
    h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not h:
        sys.exit("OpenProcess failed, error %d (try an elevated prompt)" % ctypes.get_last_error())
    print("attached read-only to pid", pid)
    return h


def regions(h):
    addr = 0
    mbi = MEMORY_BASIC_INFORMATION()
    while addr < 0x7FFFFFFFFFFF:
        if not k32.VirtualQueryEx(h, ctypes.c_void_p(addr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
            break
        base = mbi.BaseAddress or 0
        if mbi.State == MEM_COMMIT and not (mbi.Protect & PAGE_GUARD) and mbi.Protect != PAGE_NOACCESS:
            yield base, mbi.RegionSize, mbi.Protect, mbi.Type
        addr = base + mbi.RegionSize


def read(h, addr, size):
    buf = ctypes.create_string_buffer(size)
    n = ctypes.c_size_t()
    if k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, size, ctypes.byref(n)):
        return buf.raw[: n.value]
    return b""


def scan(h, patterns, max_hits=40):
    hits = {p: [] for p in patterns}
    total = 0
    for base, size, prot, typ in regions(h):
        if size > 512 * 1024 * 1024:
            continue
        data = read(h, base, size)
        if not data:
            continue
        total += len(data)
        for p in patterns:
            start = 0
            while len(hits[p]) < max_hits:
                i = data.find(p, start)
                if i < 0:
                    break
                hits[p].append((base + i, prot))
                start = i + 1
    print("scanned %.1f MB" % (total / 1e6))
    return hits


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    h = open_proc()
    if cmd == "regions":
        n = 0
        for base, size, prot, typ in regions(h):
            n += 1
            if size >= 1 << 20:
                print("  %016x  %8.1f MB  prot=0x%02x type=0x%x" % (base, size / 1e6, prot, typ))
        print(n, "committed readable regions")
    elif cmd in ("find", "hex"):
        pats = []
        for a in sys.argv[2:]:
            if cmd == "hex":
                pats.append(bytes.fromhex(a))
            else:
                pats.append(a.encode("utf-8"))
                pats.append(a.encode("utf-16-le"))
        hits = scan(h, pats)
        for p, lst in hits.items():
            print("\n%r: %d hits%s" % (p, len(lst), " (capped)" if len(lst) >= 40 else ""))
            for addr, prot in lst[:12]:
                ctx = read(h, addr - 16, 64)
                printable = bytes(c if 32 <= c < 127 else 46 for c in ctx).decode()
                print("  %016x prot=0x%02x  %s" % (addr, prot, printable))
    elif cmd == "watch":
        addr = int(sys.argv[2], 16)
        size = int(sys.argv[3]) if len(sys.argv) > 3 else 16
        period = float(sys.argv[4]) if len(sys.argv) > 4 else 0.25
        last = None
        print("watching %016x (%d bytes) every %.2fs; Ctrl+C to stop" % (addr, size, period))
        while True:
            cur = read(h, addr, size)
            if cur != last:
                ints = struct.unpack("<%dI" % (len(cur) // 4), cur[: len(cur) // 4 * 4]) if len(cur) >= 4 else ()
                print("%s  %s  u32=%s" % (time.strftime("%H:%M:%S"), cur.hex(" "), ints))
                last = cur
            time.sleep(period)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
