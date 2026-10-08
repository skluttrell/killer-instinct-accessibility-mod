"""Send keyboard input to the Killer Instinct window (UI binds: arrows, Enter=select, Backspace=back, Space=start).
Usage: python sendkeys.py <key> [<key> ...]   keys: up down left right enter back space esc tab a-z 0-9, modifiers ctrl+shift+r ; delay with 'wait:0.5'
Read-only w.r.t. the game; it only brings the window to the foreground and synthesizes key presses.
"""
import ctypes
import ctypes.wintypes as wt
import sys
import time

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

user32 = ctypes.WinDLL("user32", use_last_error=True)
VK = {"up": 0x26, "down": 0x28, "left": 0x25, "right": 0x27, "enter": 0x0D, "back": 0x08, "space": 0x20, "esc": 0x1B, "tab": 0x09,
      "lbracket": 0xDB, "rbracket": 0xDD, "semicolon": 0xBA, "apostrophe": 0xDE, "num2": 0x62, "num4": 0x64, "num6": 0x66, "num8": 0x68}
for c in "abcdefghijklmnopqrstuvwxyz":
    VK[c] = ord(c.upper())
for c in "0123456789":
    VK[c] = ord(c)

PUL = ctypes.POINTER(ctypes.c_ulong)


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD), ("time", wt.DWORD), ("dwExtraInfo", PUL)]


class INPUT(ctypes.Structure):
    class _I(ctypes.Union):
        _fields_ = [("ki", KEYBDINPUT), ("pad", ctypes.c_byte * 32)]
    _anonymous_ = ("u",)
    _fields_ = [("type", wt.DWORD), ("u", _I)]


def find_window():
    hwnds = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wt.HWND, wt.LPARAM)
    def cb(hwnd, lparam):
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        buf = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, buf, 256)
        if user32.IsWindowVisible(hwnd) and "killer instinct" in buf.value.lower():
            hwnds.append((hwnd, buf.value))
        return True

    user32.EnumWindows(cb, 0)
    return hwnds


def press(vk, hold=0.2):  # 60 ms was dropped by popups (2026-10-08); 200 ms registers everywhere
    # KEYEVENTF_SCANCODE (0x0008): the in-match input layer only sees scan-code events (menus accept either)
    extended = 0x0001 if vk in (0x26, 0x28, 0x25, 0x27) else 0
    scan = user32.MapVirtualKeyW(vk, 0)
    down = INPUT(type=1, ki=KEYBDINPUT(vk, scan, extended | 0x0008, 0, None))
    up = INPUT(type=1, ki=KEYBDINPUT(vk, scan, extended | 0x0008 | 0x0002, 0, None))
    user32.SendInput(1, ctypes.byref(down), ctypes.sizeof(INPUT))
    time.sleep(hold)
    user32.SendInput(1, ctypes.byref(up), ctypes.sizeof(INPUT))


if __name__ == "__main__":
    wins = find_window()
    if not wins:
        sys.exit("no Killer Instinct window found")
    hwnd, title = wins[0]
    # foreground-stealing workaround: tap ALT, attach to the foreground thread, then SetForegroundWindow
    fg = user32.GetForegroundWindow()
    tid_fg = user32.GetWindowThreadProcessId(fg, None)
    tid_me = ctypes.windll.kernel32.GetCurrentThreadId()
    user32.AttachThreadInput(tid_me, tid_fg, True)
    user32.ShowWindow(hwnd, 9)
    user32.SetForegroundWindow(hwnd)
    user32.AttachThreadInput(tid_me, tid_fg, False)
    time.sleep(0.4)
    now = user32.GetForegroundWindow()
    buf = ctypes.create_unicode_buffer(256)
    user32.GetWindowTextW(now, buf, 256)
    print("window:", title, "| foreground now:", buf.value, "| ok" if now == hwnd else "| NOT FOREGROUND")
    MODS = {"ctrl": 0x11, "shift": 0x10, "alt": 0x12}
    for k in sys.argv[1:]:
        if k.startswith("wait:"):
            time.sleep(float(k[5:]))
            continue
        parts = k.lower().split("+")          # e.g. ctrl+shift+r (global hotkeys of the narrator)
        mods = [MODS[m] for m in parts[:-1]]
        for m in mods:
            user32.SendInput(1, ctypes.byref(INPUT(type=1, ki=KEYBDINPUT(m, user32.MapVirtualKeyW(m, 0), 0, 0, None))), ctypes.sizeof(INPUT))
            time.sleep(0.05)
        press(VK[parts[-1]])
        for m in reversed(mods):
            user32.SendInput(1, ctypes.byref(INPUT(type=1, ki=KEYBDINPUT(m, user32.MapVirtualKeyW(m, 0), 0x0002, 0, None))), ctypes.sizeof(INPUT))
            time.sleep(0.05)
        time.sleep(0.25)
        print("pressed", k)
