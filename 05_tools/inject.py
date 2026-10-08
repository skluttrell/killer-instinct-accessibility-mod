"""Development helper: load a DLL into the running game with CreateRemoteThread(LoadLibraryW) (no Frida).
Usage: python -I inject.py <path\\to\\kiaccess.dll>
Only for testing builds; the shipped mod is loaded by the game itself as the dinput8.dll proxy. Ctrl+Shift+U in the
narrator unloads it again so a rebuilt DLL can be injected without restarting the game.
"""
import ctypes
import ctypes.wintypes as wt
import os
import sys

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT_RESERVE = 0x3000
PAGE_READWRITE = 0x04

k32.OpenProcess.restype = wt.HANDLE
k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
k32.VirtualAllocEx.restype = wt.LPVOID
k32.VirtualAllocEx.argtypes = [wt.HANDLE, wt.LPVOID, ctypes.c_size_t, wt.DWORD, wt.DWORD]
k32.WriteProcessMemory.argtypes = [wt.HANDLE, wt.LPVOID, wt.LPCVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
k32.GetModuleHandleW.restype = wt.HMODULE
k32.GetProcAddress.restype = wt.LPVOID
k32.GetProcAddress.argtypes = [wt.HMODULE, ctypes.c_char_p]
k32.CreateRemoteThread.restype = wt.HANDLE
k32.CreateRemoteThread.argtypes = [wt.HANDLE, wt.LPVOID, ctypes.c_size_t, wt.LPVOID, wt.LPVOID, wt.DWORD, wt.LPVOID]
k32.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]
k32.GetExitCodeThread.argtypes = [wt.HANDLE, wt.LPDWORD]


def find_pid(name):
    import subprocess
    out = subprocess.check_output(["tasklist", "/FI", "IMAGENAME eq %s" % name, "/FO", "CSV", "/NH"], text=True)
    for line in out.splitlines():
        parts = [p.strip('"') for p in line.split('","')]
        if len(parts) > 1 and parts[0].lower() == name.lower():
            return int(parts[1])
    return None


def main():
    dll = os.path.abspath(sys.argv[1])
    if not os.path.isfile(dll):
        sys.exit("no such file: " + dll)
    pid = find_pid("KILLERINSTINCTX64_R.EXE")
    if not pid:
        sys.exit("game not running")
    h = k32.OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h:
        sys.exit("OpenProcess failed: %d" % ctypes.get_last_error())
    path = (dll + "\0").encode("utf-16-le")
    mem = k32.VirtualAllocEx(h, None, len(path), MEM_COMMIT_RESERVE, PAGE_READWRITE)
    written = ctypes.c_size_t()
    k32.WriteProcessMemory(h, mem, path, len(path), ctypes.byref(written))
    loadlib = k32.GetProcAddress(k32.GetModuleHandleW("kernel32.dll"), b"LoadLibraryW")
    th = k32.CreateRemoteThread(h, None, 0, loadlib, mem, 0, None)
    if not th:
        sys.exit("CreateRemoteThread failed: %d" % ctypes.get_last_error())
    k32.WaitForSingleObject(th, 10000)
    code = wt.DWORD()
    k32.GetExitCodeThread(th, ctypes.byref(code))
    print("injected into pid %d, LoadLibraryW returned 0x%x" % (pid, code.value))


if __name__ == "__main__":
    main()
