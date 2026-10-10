"""Check that the narrator's Ctrl+Shift+R hotkey is held only while the game window is in the foreground.
Tries to register Ctrl+Shift+R from this process (it fails while another process holds it) with the game in front,
then after an Alt+Tab away, then after an Alt+Tab back. SetForegroundWindow to another process and minimizing the game
do not move the focus away from the game (Windows foreground rules), Alt+Tab does.
Usage: python hotkey_probe.py
"""
import ctypes
import sys
import time

sys.path.insert(0, __file__.rsplit("\\", 1)[0] if "\\" in __file__ else ".")
from sendkeys import INPUT, KEYBDINPUT, find_window, user32  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
MOD_CONTROL, MOD_SHIFT = 0x0002, 0x0004


def key(vk, down):
    scan = user32.MapVirtualKeyW(vk, 0)
    user32.SendInput(1, ctypes.byref(INPUT(type=1, ki=KEYBDINPUT(vk, scan, 0 if down else 0x0002, 0, None))), ctypes.sizeof(INPUT))


def alt_tab():
    key(0x12, True)
    time.sleep(0.05)
    key(0x09, True)
    time.sleep(0.05)
    key(0x09, False)
    time.sleep(0.3)
    key(0x12, False)


def foreground_title():
    b = ctypes.create_unicode_buffer(256)
    user32.GetWindowTextW(user32.GetForegroundWindow(), b, 256)
    return b.value


def hotkey_free():
    ok = user32.RegisterHotKey(None, 0x4B49, MOD_CONTROL | MOD_SHIFT, ord("R"))
    if ok:
        user32.UnregisterHotKey(None, 0x4B49)
    return bool(ok)


wins = find_window()
if not wins:
    sys.exit("no Killer Instinct window found")
game = wins[0][0]
fg = user32.GetForegroundWindow()
tid_fg = user32.GetWindowThreadProcessId(fg, None)
tid_me = ctypes.windll.kernel32.GetCurrentThreadId()
user32.AttachThreadInput(tid_me, tid_fg, True)
user32.ShowWindow(game, 9)
user32.SetForegroundWindow(game)
user32.AttachThreadInput(tid_me, tid_fg, False)
time.sleep(1.0)
print("game in front (%r): Ctrl+Shift+R free for other programs = %s" % (foreground_title(), hotkey_free()))
alt_tab()
time.sleep(1.5)
print("after Alt+Tab (%r): free = %s" % (foreground_title(), hotkey_free()))
alt_tab()
time.sleep(1.5)
print("back in the game (%r): free = %s" % (foreground_title(), hotkey_free()))
