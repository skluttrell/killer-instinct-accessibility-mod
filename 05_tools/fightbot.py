"""Crude in-match driver for test runs that must be won (the Shadow Lords tutorial repeats a lost mission).
Plays Jago as Player 1 on the left: block, then quarter-circle-forward punch (Endokuken) twice, sometimes a forward
heavy kick. Uses the user's keyboard binds (back A, forward D, down S, LP 2, HK E) with fighting-game timing (50 ms
taps), which sendkeys.py cannot do (200 ms holds).
Usage: python fightbot.py <seconds>
Read-only w.r.t. the game; it only synthesizes key presses into the game window.
"""
import ctypes
import sys
import time

sys.path.insert(0, __file__.rsplit("\\", 1)[0] if "\\" in __file__ else ".")
from sendkeys import INPUT, KEYBDINPUT, VK, find_window, user32  # noqa: E402

sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def key(vk, down):
    scan = user32.MapVirtualKeyW(vk, 0)
    flags = 0x0008 | (0 if down else 0x0002)
    user32.SendInput(1, ctypes.byref(INPUT(type=1, ki=KEYBDINPUT(vk, scan, flags, 0, None))), ctypes.sizeof(INPUT))


def tap(name, hold=0.05):
    key(VK[name], True)
    time.sleep(hold)
    key(VK[name], False)


def hold(name, secs):
    key(VK[name], True)
    time.sleep(secs)
    key(VK[name], False)


def qcf_punch(fwd="d"):
    key(VK["s"], True)
    time.sleep(0.05)
    key(VK[fwd], True)
    time.sleep(0.05)
    key(VK["s"], False)
    time.sleep(0.05)
    key(VK["2"], True)
    time.sleep(0.05)
    key(VK["2"], False)
    key(VK[fwd], False)


def foreground():
    wins = find_window()
    if not wins:
        sys.exit("no Killer Instinct window found")
    hwnd, _ = wins[0]
    fg = user32.GetForegroundWindow()
    tid_fg = user32.GetWindowThreadProcessId(fg, None)
    tid_me = ctypes.windll.kernel32.GetCurrentThreadId()
    user32.AttachThreadInput(tid_me, tid_fg, True)
    user32.ShowWindow(hwnd, 9)
    user32.SetForegroundWindow(hwnd)
    user32.AttachThreadInput(tid_me, tid_fg, False)
    time.sleep(0.3)


if __name__ == "__main__":
    secs = float(sys.argv[1]) if len(sys.argv) > 1 else 30
    foreground()
    t0 = time.time()
    n = 0
    while time.time() - t0 < secs:
        # sides swap during a fight and we cannot see them: alternate the forward key, one of the two is right
        fwd, back = ("d", "a") if n % 2 == 0 else ("a", "d")
        hold(back, 0.5)         # block
        qcf_punch(fwd)
        time.sleep(0.35)
        qcf_punch(fwd)
        time.sleep(0.25)
        key(VK[fwd], True)
        tap("e", 0.06)          # forward heavy kick
        key(VK[fwd], False)
        time.sleep(0.25)
        n += 1
    for k in ("a", "s", "d", "2", "e"):
        key(VK[k], False)
    print("done", n, "cycles")
