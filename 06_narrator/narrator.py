"""KI accessibility narrator v0.1 (Python + Frida + Prism). Speaks menu focus, screen changes, popups and toggles.
Usage: python narrator.py [--log speech.log] [--verbosity 0|1|2] [--no-speech]
Hotkeys (global): Ctrl+Shift+R repeat last, Ctrl+Shift+D read description, Ctrl+Shift+T read ticker/MOTD,
                  Ctrl+Shift+V cycle verbosity, Ctrl+Shift+Q quit.
Read-only with respect to the game (see agent.js). Run with plain `python` (needs user-site frida).
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import datetime
import glob
import json
import os
import queue
import re
import sys
import threading
import time
import zlib

import frida

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "05_tools"))
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# ---------------- localization ----------------
class Strings:
    """hash -> text table (data/strings_en.tsv); keys hash as CRC32(lowercase key)."""

    def __init__(self, path):
        self.by_hash = {}
        with open(path, encoding="utf-8") as f:
            next(f)
            for line in f:
                parts = line.rstrip("\n").split("\t", 2)
                if len(parts) == 3:
                    self.by_hash[int(parts[0], 16)] = parts[2]

    def resolve(self, key):
        if key is None:
            return None
        if not isinstance(key, str):
            return str(key)
        if key.startswith("&"):
            return key[1:]
        if key.startswith("#") and re.fullmatch(r"-?\d+", key[1:]):  # pre-hashed key: "#<decimal crc32>", may be signed
            return self.by_hash.get(int(key[1:]) & 0xFFFFFFFF, key)
        return self.by_hash.get(zlib.crc32(key.lower().encode("utf-8")) & 0xFFFFFFFF, key)


SCREEN_NAMES = {
    "MainMenu.swf": "Main menu", "OptionsMenu.swf": "Options", "LandingPage.swf": "Landing page", "Popup.swf": "Popup",
    "ShadowPopup.swf": "Popup", "CharacterSelect.swf": "Character select", "StoreMain.swf": "Store", "ControllerConfig.swf": "Controller",
    "FightArchiveMain.swf": "Fight archive", "StartScreen.swf": "Start screen", "MultiplayerLobby.swf": "Lobby", "StageSelect.swf": "Stage select",
    "PauseMenu.swf": "Pause menu", "PracticePauseMenu.swf": "Practice menu", "BlockList.swf": "Block list",
    "CommandList.swf": "Command list", "Dojo.swf": "Dojo", "Trials.swf": "Trials", "SinglePlayerLadder.swf": "Arcade ladder",
    "MatchOutcome.swf": "Match results", "LoadingScreen.swf": "Loading", "StoreFighters.swf": "Store, fighters",
    "StoreBundles.swf": "Store, bundles", "StoreContent.swf": "Store, content", "Leaderboards.swf": "Leaderboards", "Stats.swf": "Stats",
    "Replays.swf": "Replays", "ComboBreakerOptions.swf": "Combo breaker options", "ShadowHub.swf": "Shadow Lab",
    "MultiplayerRanked.swf": "Ranked", "MultiplayerExhibition.swf": "Exhibition", "Story.swf": "Story", "GA_HUB.swf": "Shadow Lords",
}

# Command-list notation: "<TT>DOWN</TT><TT>DOWNFORWARD</TT><TT>FORWARD</TT> + <TT>LOWPUNCH</TT>" -> words
CMD_TOKENS = {
    "DOWN": "down", "DOWNBACK": "down-back", "BACK": "back", "UPBACK": "up-back", "UP": "up", "UPFORWARD": "up-forward",
    "FORWARD": "forward", "DOWNFORWARD": "down-forward", "LOWPUNCH": "light punch", "MEDPUNCH": "medium punch",
    "HIGHPUNCH": "heavy punch", "LOWKICK": "light kick", "MEDKICK": "medium kick", "HIGHKICK": "heavy kick",
    "ANYPUNCH": "any punch", "ANYKICK": "any kick", "PLUS": "plus",
}


def humanize_command(cmd):
    if not isinstance(cmd, str) or not cmd:
        return ""
    out = []
    for part in re.split(r"(<TT>[^<]*</TT>)", cmd):
        if not part:
            continue
        if part.startswith("<TT>"):
            tok = part[4:-5].strip()
            out.append(CMD_TOKENS.get(tok.upper(), tok.lower()))
        else:
            t = part.strip()
            if t == "+":
                out.append("plus")
            elif t:
                out.append(t)
    return " ".join(out)


ICON_WORDS = {"ArrowUp": "up", "ArrowDown": "down", "ArrowLeft": "left", "ArrowRight": "right", "Plus": "plus",
              "ArrowLeftDown": "down-back", "ArrowRightDown": "down-forward", "ArrowLeftUp": "up-back", "ArrowRightUp": "up-forward"}


def icon_words(name):
    """ControllerConfig command icons: 'LightPunch' -> 'light punch', 'ArrowUp' -> 'up'."""
    if not isinstance(name, str):
        return ""
    return ICON_WORDS.get(name) or re.sub(r"(?<=[a-z])(?=[A-Z])", " ", name).lower()


def command_words(entries, resolve):
    """A ControllerConfig command row is a run of icons and/or text entries; icons that only illustrate an adjacent text
    entry are dropped (LightPunch icon + 'Light Punch' text -> 'Light Punch'), 'Plus' icons are kept."""
    out = []
    has_text = any(isinstance(e, dict) and e.get("Type") == "String" for e in entries)
    for e in entries:
        if isinstance(e, dict):
            if e.get("Type") == "String":
                out.append(resolve(e.get("Entry")))
            elif e.get("Entry") == "Plus" or not has_text:
                out.append(icon_words(e.get("Entry")))
        elif isinstance(e, str):
            out.append(resolve(e))
    return " ".join(w for w in out if w)


def screen_name(swf):
    return SCREEN_NAMES.get(swf, swf.replace(".swf", "")) if swf else "unknown screen"


# ---------------- model ----------------
class Narrator:
    def __init__(self, strings, speak, log, verbosity=1):
        self.S = strings
        self._speak = speak
        self.log = log
        self.verbosity = verbosity
        self.populate = {}          # swf -> latest Populate JSON (dict)
        for fn in glob.glob(os.path.join(ROOT, "data", "populate_*.json")):  # last known payloads (attach after a screen loaded)
            try:
                name = os.path.basename(fn)[len("populate_"):-len(".json")]
                if ":" not in name and "_entries" not in name:
                    self.populate[name + ".swf"] = json.load(open(fn, encoding="utf-8"))
            except Exception:
                pass
        self.options_state = ["Main"]
        self.last = None            # last spoken focus key
        self.last_text = ""
        self.last_desc = ""
        self.last_focus = None
        self.ticker = ""
        self.motd = ""
        self.current_swf = None
        self.cs_stage = {0: "fighter", 1: "fighter"}   # character select stage per side
        self.cs_colors = {0: [], 1: []}
        self.cs_last = {}
        self.cs_active = 0           # which side Player 1's keys drive on the character select
        self.last_probe = {}         # (swf, probe key) -> last spoken probe text (category, mode, difficulty)
        self.cmdlist = {}            # latest CommandList RefreshPage payload
        self.cmd_group_spoken = False
        self.popup_spoken = False
        self.skip_value_once = None
        self.mm_state = []           # MainMenu state stack (its button text tweens in late, so labels come from the JSON)
        self.names_path = os.path.join(ROOT, "data", "fighter_names.json")
        try:  # codename -> display name (built from MainMenu PopulatePlayerCards; refreshed at runtime)
            self.fighter_names = json.load(open(self.names_path, encoding="utf-8"))
        except Exception:
            self.fighter_names = {}

    def learn_fighter_names(self, data):
        changed = False
        for side in ("Player1Info", "Player2Info"):
            for e in ((data.get(side) or {}).get("Expanded") or {}).get("Entries") or []:
                img, title = e.get("CharacterImage") or "", e.get("Title") or ""
                m = re.search(r"/(\w+)\.dds$", img)
                t = re.match(r"&?(.+?) - Lvl", title)
                if m and t and self.fighter_names.get(m.group(1).lower()) != t.group(1):
                    self.fighter_names[m.group(1).lower()] = t.group(1)
                    changed = True
        if changed:
            try:
                json.dump(dict(sorted(self.fighter_names.items())), open(self.names_path, "w", encoding="utf-8"), indent=1)
            except Exception:
                pass

    def say(self, text, interrupt=True, t_hook=None):
        if not text:
            return
        if "\\n" in text or "\n" in text:
            text = text.replace("\\n", " ").replace("\n", " ")  # literal backslash-n sequences / newlines in tip popups
        if "<" in text:  # inline icon tags in descriptions: <TT>FORWARD</TT>, <img ...>, <font>
            text = re.sub(r"<TT>([^<]*)</TT>", lambda m: " " + CMD_TOKENS.get(m.group(1).strip().upper(), m.group(1).lower()) + " ", text)
            text = re.sub(r"<[^>]+>", " ", text)
            text = re.sub(r"\s+", " ", text).strip()
        lat = ""
        if t_hook:
            lat = " [%d ms]" % int(time.time() * 1000 - t_hook)
        self.log("SAY%s: %s" % (lat, text))
        self.last_text = text
        self._speak(text, interrupt)

    # ---- events from the agent ----
    def on_inv(self, swf, fn, js):
        data = None
        if js:
            try:
                data = json.loads(js, strict=False)   # popup bodies carry raw newlines
            except Exception as e:
                data = None
                if fn.startswith("Populate") or fn.startswith("Lua_Populate") or fn == "RefreshPage":
                    self.log("json parse failed for %s.%s: %r (%s)" % (swf, fn, e, js[:120]))
        if fn in ("Populate", "Lua_Populate", "PopulateStatesOnly"):
            if isinstance(data, list):   # Dojo sends an array of modes
                self.populate[swf] = {"Modes": data}
                data = self.populate[swf]
            if isinstance(data, dict):
                self.populate[swf] = data
                try:  # keep the latest payload of each screen for reader development
                    with open(os.path.join(ROOT, "data", "populate_%s.json" % swf.replace(".swf", "")), "w", encoding="utf-8") as f:
                        json.dump(data, f, indent=1)
                except Exception:
                    pass
                if swf == "OptionsMenu.swf":
                    self.options_state = ["Graphics" if data.get("GraphicsOptionOnly") else "Main"]
                if swf == "MainMenu.swf":
                    self.mm_state = [data.get("InitialState") or "SinglePlayer"]
                if swf == "CharacterSelect.swf":
                    self.cs_stage = {0: "fighter", 1: "fighter"}
                    self.cs_active = 0
                    self.cs_last = {}
                    fl = data.get("FighterSelection") or []
                    if fl and isinstance(fl[0], dict):
                        self.log("charselect fighters: %d, entry keys: %s" % (len(fl), sorted(fl[0].keys())))
            if swf in ("Popup.swf", "ShadowPopup.swf"):
                self.popup_spoken = isinstance(data, dict)
                self.say_popup(data)
            elif swf == "Toast.swf" and isinstance(data, dict):
                parts = [self.S.resolve(data.get(k)) for k in ("Title", "Description") if data.get(k)]
                if data.get("XPValue") not in (None, ""):
                    parts.append("%s XP" % self.S.resolve(data.get("XPValue")))
                self.say("Notification. " + ". ".join(p for p in parts if p), interrupt=False)
            elif swf == "LoadingScreen.swf" and isinstance(data, dict):
                names = []
                for k in ("p1Thumb", "p2Thumb"):
                    v = data.get(k)
                    if isinstance(v, str) and v:
                        code = re.sub(r"\.\w+$", "", v.replace("\\", "/").split("/")[-1]).lower().split("_")[0]
                        names.append(self.fighter_names.get(code) or code.capitalize())
                self.say("Loading. " + " versus ".join(names) if names else "Loading", interrupt=True)
            elif swf == "StageSelect.swf":
                self.last_probe.pop(("StageSelect.swf", "stage"), None)
            elif swf == "MatchOutcome.swf" and isinstance(data, dict):
                self.say_match_outcome(data)
        elif fn == "RefreshPage" and swf == "CommandList.swf" and isinstance(data, dict):
            self.cmdlist = data
            group = self.S.resolve("#%s" % data.get("GroupNameCRC")) if data.get("GroupNameCRC") is not None else ""
            text = ". ".join(t for t in (group, self.S.resolve("&" + str(data.get("CharacterName") or "")) if data.get("CharacterName") else "") if t)
            if text:
                self.say("%s, %d moves" % (text, int(data.get("NumMoves") or 0)), interrupt=True)
                self.cmd_group_spoken = True
        elif fn == "PopulateEntries" and swf == "PracticePauseMenu.swf":
            self.populate["PracticePauseMenu.swf:entries"] = data
            try:
                with open(os.path.join(ROOT, "data", "populate_PracticePauseMenu_entries.json"), "a", encoding="utf-8") as f:
                    f.write(json.dumps(data) + chr(10))
            except Exception:
                pass
        elif fn == "Lua_ReceiveAnyKeyPress" and swf == "ControllerConfig.swf" and isinstance(data, dict):
            # a key was assigned to the row that was waiting; mirror it into our copy of the payload
            disp = self.S.resolve(data.get("KeyDisplayString")) or data.get("KeyBinding") or "key"
            lf = self.last_focus or {}
            side = 1 if str(lf.get("label", "")).startswith("Player 2") else 0
            i = lf.get("index")
            cmds = (self.populate.get("ControllerConfig.swf") or {}).get("Commands") or []
            if isinstance(i, int) and 0 <= i < len(cmds) and isinstance(cmds[i], dict):
                btn = cmds[i].setdefault("Button", [{}, {}])
                if isinstance(btn, list) and side < len(btn):
                    btn[side] = {"binding": data.get("KeyBinding"), "displayString": data.get("KeyDisplayString")}
            self.say("%s assigned" % disp, interrupt=True)
        elif fn == "PopulatePlayerCards" and isinstance(data, dict):
            self.learn_fighter_names(data)
        elif fn == "PopulateTicker" and isinstance(data, dict):
            self.ticker = self.S.resolve(data.get("motdString", "")) or ""
        elif fn == "PopulateMOTD" and isinstance(data, dict):
            motd = "%s. %s" % (self.S.resolve(data.get("Title", "")), self.S.resolve(data.get("Message", "")))
            if motd != self.motd:  # the game re-sends it every 30 s
                self.motd = motd
                if self.verbosity >= 1:
                    self.say(motd, interrupt=False)
        elif swf == "CharacterSelect.swf" and fn == "Lua_PopulateColorData" and isinstance(data, dict):
            self.cs_colors[int(data.get("PlayerIndex", 0))] = data.get("ColorList") or []
        elif fn == "Lua_PopulateDailyLootExpanded" and isinstance(data, dict):
            days = [self.S.resolve(d.get("name")) for d in data.get("previewRewardData", []) if isinstance(d, dict)]
            self.say("Daily rewards panel. %s. %s. Escape to close" % (self.S.resolve(data.get("resetText", "")), ", ".join(d for d in days if d)), interrupt=True)

    def say_match_outcome(self, data):
        w = data.get("Winner")
        parts = ["Match results"]
        if w in (0, 1):
            parts.append("Player %d wins" % (w + 1))
        for side in (0, 1):
            st = data.get("Player%dState" % (side + 1)) or {}
            if not data.get("p%dShowStats" % (side + 1)) or not isinstance(st, dict):
                continue
            streak = self.S.resolve(st.get("WinStreak"))
            if streak and streak.strip():
                parts.append(streak.strip())
            if self.verbosity >= 2:
                for cat in ("Hero", "Offense", "Defense", "Combos", "Variety"):
                    c = st.get(cat)
                    if isinstance(c, dict):
                        ms = ["%s %s" % (self.S.resolve(m.get("key")), self.S.resolve(m.get("stat"))) for m in c.get("metrics") or [] if isinstance(m, dict)]
                        if ms:
                            parts.append("%s: %s" % (self.S.resolve(c.get("key")) or cat, ", ".join(ms)))
        self.say(". ".join(p for p in parts if p), interrupt=True)

    def say_popup(self, data):
        if not isinstance(data, dict):
            return
        parts = []
        for k in ("Title", "title", "Header", "Message", "message", "Body", "Text", "text", "Description"):
            v = data.get(k)
            if isinstance(v, str) and v:
                parts.append(self.S.resolve(v))
        buttons = []
        for k, keyname in (("ABUTTON", "Enter"), ("BBUTTON", "Escape"), ("XBUTTON", "X"), ("YBUTTON", "Y")):
            b = data.get(k)
            if isinstance(b, dict) and b.get("visible") and b.get("text"):
                buttons.append("%s: %s" % (keyname, self.S.resolve(b["text"])))
        for k in ("OptionsButtons", "Buttons", "buttons", "Options", "options", "Entries"):
            v = data.get(k)
            if isinstance(v, list):
                for b in v:
                    if isinstance(b, dict):
                        lab = b.get("key") or b.get("text") or b.get("Text") or b.get("label")
                        if lab:
                            buttons.append(self.S.resolve(lab))
                    elif isinstance(b, str):
                        buttons.append(self.S.resolve(b))
        if not parts:  # unknown shape: speak every string value
            parts = [self.S.resolve(v) for v in data.values() if isinstance(v, str) and v]
        text = "Popup. " + ". ".join(p for p in parts if p)
        if buttons:
            text += ". Buttons: " + ", ".join(buttons)
        self.say(text, interrupt=True)

    def on_ei(self, script, fn, js, t):
        if fn == "LoadDestination":
            try:
                dest = json.loads(js).get("Destination")
            except Exception:
                dest = None
            if dest:
                self.current_swf = dest
                self.last = None
                self.say(screen_name(dest), interrupt=True, t_hook=t)
        elif fn == "SetVariables" and script == "OptionsMenu" and self.current_swf != "OptionsMenu.swf":
            # shared toggle component on another screen (practice menu): the new value is in the event itself
            try:
                v = self.S.resolve((json.loads(js) if js else {}).get("value"))
            except Exception:
                v = None
            if v:
                self.skip_value_once = v
                self.say(v, interrupt=True, t_hook=t)
        elif fn == "PlayAccept" and self.current_swf == "MainMenu.swf":
            e = self.mm_entry((self.last_focus or {}).get("index"))
            if e and isinstance(e.get("nextState"), str):
                self.mm_state.append(e["nextState"])
        elif fn == "PlayBack" and self.current_swf == "MainMenu.swf":
            if len(self.mm_state) > 1:
                self.mm_state.pop()
        elif fn == "PlayAccept" and self.current_swf == "OptionsMenu.swf":
            self.options_select()
        elif fn == "PlayBack" and self.current_swf == "OptionsMenu.swf":
            if len(self.options_state) > 1:
                self.options_state.pop()
                self.last = None
                self.say(self.S.resolve((self.options_entries()[0] or {}).get("Title")) or "Options", interrupt=True, t_hook=t)
        elif script == "CharSelectMenu" or (self.current_swf == "CharacterSelect.swf" and fn in ("PlayBackP1", "PlayBackP2")):
            self.on_charselect_ei(fn, js, t)
        elif script == "StageSelect" and fn == "StageSelectionChanged":
            self.on_stage_changed(js, t)
        elif script == "CommandList" and fn == "EntrySelected":
            self.on_move_selected(js, t)

    # ---- stage select (event-driven: the screen tells Lua the 1-based index on every move) ----
    def on_stage_changed(self, js, t):
        try:
            n = int(float(js))
        except Exception:
            return
        stages = (self.populate.get("StageSelect.swf") or {}).get("Stages") or []
        if 1 <= n <= len(stages) and isinstance(stages[n - 1], dict):
            st = stages[n - 1]
            text = self.S.resolve(st.get("name")) or "stage %d" % n
            if st.get("unlocked") is False:
                text += ", locked"
            if st.get("installed") is False:
                text += ", not installed"
            if self.verbosity >= 1:
                text += ", %d of %d" % (n, len(stages))
        else:
            text = "stage %d" % n
        if self.last_probe.get(("StageSelect.swf", "stage")) == text:
            return
        self.last_probe[("StageSelect.swf", "stage")] = text
        self.last_focus = {"swf": "StageSelect.swf", "index": n - 1, "label": text, "desc": "", "value": None}
        self.say(text, interrupt=True, t_hook=t)

    # ---- command list (event-driven: EntrySelected {MoveIndex}; moves from the RefreshPage payload) ----
    def on_move_selected(self, js, t):
        try:
            i = int((json.loads(js) if js else {}).get("MoveIndex", -1))
        except Exception:
            return
        moves = self.cmdlist.get("Moves") or []
        if not (0 <= i < len(moves)) or not isinstance(moves[i], dict):
            return
        m = moves[i]
        name = self.S.resolve("#%s" % m.get("NameCRC")) if m.get("NameCRC") is not None else "move %d" % (i + 1)
        cmd = humanize_command(m.get("Command"))
        text = name
        if cmd:
            text += ". " + cmd
        if m.get("Favorite"):
            text += ", key move"
        if self.verbosity >= 1:
            text += ", %d of %d" % (i + 1, len(moves))
        desc = self.S.resolve("#%s" % m.get("Description")) if m.get("Description") is not None else ""
        if desc and desc != name and self.verbosity >= 2:
            text += ". " + desc
        self.last_focus = {"swf": "CommandList.swf", "index": i, "label": name, "desc": desc, "value": cmd}
        self.say(text, interrupt=not self.cmd_group_spoken, t_hook=t)
        self.cmd_group_spoken = False

    # ---- controller config (scan groups p1/p2 = command rows + 2 toggles, p1exit/p2exit = Save/Default) ----
    def on_controller_focus(self, rec, probe, deferred):
        data = self.populate.get("ControllerConfig.swf") or {}
        groups = rec.get("groups") or {}
        for side in (0, 1):
            pn = "p%d" % (side + 1)
            if probe.get(pn + "panel") is False and probe.get(pn + "start") is True and rec.get("why", "").rstrip("+") == "ScreenShown":
                prompt = probe.get(pn + "prompt") or self.S.resolve(data.get("PressStartText" if side == 0 else "P2PressStartText")) or "Press Enter"
                if prompt != "missing text":
                    self.say("%s %s to edit controls" % ("Player %d:" % (side + 1), prompt), interrupt=side == 0)
                continue
            g = groups.get(pn) or {}
            ex = groups.get(pn + "exit") or {}
            text = None
            key = None
            if g.get("index") is not None:
                i = g.get("index")
                if isinstance(i, int) and i < 13:
                    cmds = data.get("Commands") or []
                    words = []
                    bound = None
                    if 0 <= i < len(cmds) and isinstance(cmds[i], dict):
                        words.append(command_words(cmds[i].get("Entries") or [], self.S.resolve))
                        btn = (cmds[i].get("Button") or [None, None])
                        if isinstance(btn, list) and side < len(btn):
                            b = btn[side]
                            bound = self.S.resolve(b.get("displayString")) if isinstance(b, dict) else (self.S.resolve(b) if isinstance(b, str) else "")
                    text = " ".join(w for w in words if w) or "command %d" % (i + 1)
                    if g.get("state") == "RemappingLoop":
                        text += ". Press a key"
                    elif bound:
                        text += ": " + bound
                    else:
                        text += ": unassigned"
                    if str(g.get("warnState") or "").startswith("ShowMessage") and g.get("warn") not in (None, "", "missing text"):
                        text += ". " + g["warn"]
                    if self.verbosity >= 1 and cmds:
                        text += ", %d of %d" % (i + 1, len(cmds))
                    key = (side, "cmd", i, g.get("state") == "RemappingLoop", bound, str(g.get("warnState") or "").startswith("ShowMessage"))
                else:
                    label, value = g.get("text"), g.get("value")
                    if label in (None, "", "missing text"):
                        continue
                    text = label + (", " + value if value not in (None, "", "missing text") else "")
                    key = (side, "toggle", i, label, value)
            elif ex.get("label") not in (None, "", "missing text"):
                text = ex["label"] + ", button"
                key = (side, "exit", ex.get("index"), ex["label"])
            if text is None:
                continue
            if self.cs_last.get(("cc", side)) == key:
                continue
            self.cs_last[("cc", side)] = key
            self.last_focus = {"swf": "ControllerConfig.swf", "index": g.get("index"), "label": text, "desc": "", "value": None}
            self.say(("Player 2: " if side == 1 else "") + text, interrupt=True, t_hook=rec.get("t"))

    # ---- character select ----
    def cs_fighter(self, idx):
        fl = (self.populate.get("CharacterSelect.swf") or {}).get("FighterSelection") or []
        if isinstance(idx, int) and 0 <= idx < len(fl) and isinstance(fl[idx], dict):
            return fl[idx]
        return None

    def cs_fighter_text(self, e):
        if not e:
            return None
        name = e.get("name") or "?"
        text = self.fighter_names.get(name.lower()) or self.S.resolve("CHARACTER_" + name.upper())
        if text == "CHARACTER_" + name.upper():
            text = name
        if e.get("isRandom"):
            text = "Random"
        flags = []
        for k, word in (("purchased", "not purchased"), ("installed", "not installed"), ("released", "not released"), ("selectable", "locked")):
            if e.get(k) is False:
                flags.append(word)
        if flags:
            text += ", " + ", ".join(flags)
        return text

    def on_charselect_ei(self, fn, js, t):
        try:
            sel = json.loads(js) if js else {}
        except Exception:
            sel = {}
        if not isinstance(sel, dict):
            sel = {}
        if isinstance(sel.get("selectionData"), dict):  # AS_SetSelections wraps the selection
            sel = dict(sel, **sel["selectionData"])
        side = int(sel.get("PlayerIndex", 0) or 0)
        confirmed = bool(sel.get("Confirmed"))
        data = self.populate.get("CharacterSelect.swf") or {}
        fname = sel.get("Name") or ""
        p2 = "Player 2: " if side == 1 else ""
        if fn == "AS_SetSelections" and side == 1 and self.cs_stage.get(0) == "waiting" and self.cs_stage.get(1) == "fighter":
            # Player 1 is ready and now drives the Player 2 (dummy / CPU) cursor
            self.cs_active = 1
            self.cs_last.pop(("fighter", 1), None)
            self.say("Now choosing Player 2's fighter", interrupt=True, t_hook=t)
            return
        if fn == "AS_PlayerPickedFighter":
            self.cs_stage[side] = "costume"
            disp = self.fighter_names.get(fname.lower(), fname) if fname else "Fighter"
            self.say("%s%s chosen. Costume" % (p2, disp), interrupt=True, t_hook=t)
        elif fn == "AS_PlayerPickedCostume":
            costumes = (data.get("CostumeSelection") or {}).get(fname) or []
            ci = sel.get("CostumeIndex")
            text = None
            if isinstance(ci, int) and 0 <= ci < len(costumes) and isinstance(costumes[ci], dict):
                c = costumes[ci]
                text = self.S.resolve(c.get("name") or "costume %d" % (ci + 1))
                if c.get("purchased") is False:
                    text += ", not purchased"
                if c.get("custom"):
                    text += ", custom slot %s" % (int(sel.get("CustomSlot", 0) or 0) + 1)
                text += ", %d of %d" % (ci + 1, len(costumes))
            else:
                text = "costume %s" % ci
            if confirmed:
                self.cs_stage[side] = "color"
                self.say(p2 + text + " chosen. Color", interrupt=True, t_hook=t)
            elif self.cs_last.get(("costume", side)) != text:
                self.say(p2 + text, interrupt=True, t_hook=t)
            self.cs_last[("costume", side)] = text
        elif fn == "AS_PlayerPickedColor":
            colors = self.cs_colors.get(side) or []
            bi = sel.get("ColorIndex")
            entry = next((c for c in colors if isinstance(c, dict) and c.get("backendIndex") == bi), None)
            if entry:
                text = self.S.resolve(entry.get("name") or "color %s" % bi)
                if entry.get("purchased") is False:
                    text += ", not purchased"
                pos = colors.index(entry)
                text += ", %d of %d" % (pos + 1, len(colors))
            else:
                text = "color %s" % bi
            if confirmed:
                self.cs_stage[side] = "waiting"
                self.say(p2 + text + " chosen. Ready", interrupt=True, t_hook=t)
            elif self.cs_last.get(("color", side)) != text:
                self.say(p2 + text, interrupt=True, t_hook=t)
            self.cs_last[("color", side)] = text
        elif fn == "AS_ResetPlayerSelection":
            self.cs_stage[side] = "fighter"
            if side == 0:
                self.cs_active = 0
            self.cs_last.pop(("fighter", side), None)
            self.say(p2 + "Back to fighter select", interrupt=True, t_hook=t)
        elif fn in ("PlayBackP1", "PlayBackP2"):  # stepping back one stage: let the next stage event speak again
            s = 1 if fn.endswith("P2") else 0
            if self.cs_stage.get(s) == "color":
                self.cs_stage[s] = "costume"
            self.cs_last.pop(("costume", s), None)
            self.cs_last.pop(("color", s), None)

    def on_charselect_focus(self, rec):
        cs = rec.get("cs") or {}
        why = rec.get("why", "")
        side = 1 if why.endswith("P2") else 0
        if side == 0 and self.cs_stage.get(0) == "waiting" and self.cs_active == 1:
            side = 1          # Player 1's keys now move the Player 2 cursor
        if self.cs_stage.get(side) != "fighter":
            return
        idx = cs.get("f%d" % side)
        e = self.cs_fighter(idx)
        text = self.cs_fighter_text(e) or ("fighter %s" % idx)
        fl = (self.populate.get("CharacterSelect.swf") or {}).get("FighterSelection") or []
        if fl and isinstance(idx, int) and self.verbosity >= 1:
            text += ", %d of %d" % (idx + 1, len(fl))
        if why == "PlayNegativeP%d" % (side + 1):
            text += ", locked"
        if self.cs_last.get(("fighter", side)) == text:
            return
        self.cs_last[("fighter", side)] = text
        self.last_focus = {"swf": "CharacterSelect.swf", "index": idx, "label": text, "desc": "", "value": None}
        self.say(("Player 2: " if side == 1 else "") + text, interrupt=True, t_hook=rec.get("t"))

    def mm_entries(self):
        data = self.populate.get("MainMenu.swf")
        if not isinstance(data, dict) or not self.mm_state:
            return []
        st = (data.get("States") or {}).get(self.mm_state[-1]) or {}
        return st.get("options") or st.get("Entries") or []

    def mm_entry(self, idx):
        ents = self.mm_entries()
        if isinstance(idx, int) and 0 <= idx < len(ents) and isinstance(ents[idx], dict):
            return ents[idx]
        return None

    def options_entries(self):
        data = self.populate.get("OptionsMenu.swf")
        if not data:
            return None, None
        st = data.get("States", {}).get(self.options_state[-1], {})
        return st, st.get("Entries", [])

    def options_select(self):
        st, entries = self.options_entries()
        if not entries or not self.last_focus:
            return
        label = self.last_focus.get("label")
        for e in entries:
            if self.S.resolve(e.get("key")) == label or e.get("debugText") == label:
                ns = e.get("nextState")
                if isinstance(ns, str) and ns != "Help":
                    self.options_state.append(ns)
                    self.last = None
                    st2 = (self.populate.get("OptionsMenu.swf") or {}).get("States", {}).get(ns, {})
                    self.say(self.S.resolve(st2.get("Title")) or ns, interrupt=True)
                break

    @staticmethod
    def state_of(g):
        st = str(g.get("state") or "")
        if st == "RemappingLoop":
            return "focused"
        if "Focus" in st and not st.startswith("Lose") and not st.startswith("Unfocused"):
            return "focused"
        if st.startswith("Lose") or st.startswith("Unfocused"):
            return "unfocused"
        return "unknown"

    def pick_group(self, groups):
        """(name, group) holding the live cursor: a focused state with a label wins, else the first labelled group."""
        best = None
        for name, g in groups.items():
            if not isinstance(g, dict):
                continue
            has_label = g.get("label") not in (None, "", "missing text")
            if has_label and self.state_of(g) == "focused":
                return name, g
            if best is None and has_label:
                best = (name, g)
        return best or (None, {})

    def on_focus(self, rec):
        swf = rec.get("swf")
        if rec.get("missing"):
            return
        why = rec.get("why", "")
        deferred = why.endswith("+")
        base_why = why.rstrip("+")
        if swf == "CharacterSelect.swf":
            if not deferred:
                self.on_charselect_focus(rec)
            return
        if rec.get("index") is None or swf in ("StageSelect.swf", "CommandList.swf"):
            return
        probe = rec.get("probe") or {}

        def clean(v):
            return None if v in (None, "", "missing text") else v

        if swf == "ControllerConfig.swf":
            self.on_controller_focus(rec, probe, deferred)
            return
        if swf == "Popup.swf" and base_why == "inv:Populate" and not self.popup_spoken:
            # JSON fallback failed: read title and body from the screen itself
            parts = [clean(probe.get("title")), clean(probe.get("body"))]
            if any(parts):
                self.popup_spoken = True
                self.say("Popup. " + ". ".join(p for p in parts if p), interrupt=True, t_hook=rec.get("t"))
        if swf == "Popup.swf" and probe.get("hasButtons") is not True:
            return
        idx = rec["index"]
        groups = rec.get("groups") or {}
        mode = rec.get("mode")
        label = desc = value = None
        count = pos = None
        flags = []
        prefix = ""
        gname = None
        state_of = self.state_of
        if swf == "OptionsMenu.swf":
            st, entries = self.options_entries()
            es = state_of(groups.get("entries") or {})
            tg = groups.get("toggles") or {}
            if es == "focused":
                toggle = False
            elif es == "unfocused" and clean(tg.get("label")) and state_of(tg) != "unfocused":
                toggle = True
            else:
                toggle = mode if isinstance(mode, bool) else (bool(st and st.get("toggle") == "Yes"))
            g = groups.get("toggles" if toggle else "entries") or {}
            label, desc = clean(g.get("label")), clean(g.get("desc") or g.get("desc2"))
            if toggle:
                value = clean(g.get("value")) if g.get("valueShown", True) else clean(g.get("slider"))
                if value is None:
                    value = clean(g.get("slider"))
            if entries:
                count = len(entries)
                pos = idx
                if label is None and 0 <= idx < len(entries):  # current state's entry, e.g. before the screen text is set
                    e = entries[idx]
                    label, desc = e.get("key") or e.get("debugText"), e.get("descKey")
                    if toggle and value is None and isinstance(e.get("textValues"), list):
                        v = e.get("value")
                        if isinstance(v, int) and 0 <= v < len(e["textValues"]):
                            value = e["textValues"][v]
                        elif v is not None:
                            value = str(v)
        elif groups:
            gname, g = self.pick_group(groups)
            label, desc = clean(g.get("label")), clean(g.get("desc") or g.get("desc2"))
            if "value" in g or "slider" in g:
                value = clean(g.get("value")) if g.get("valueShown", True) else clean(g.get("slider"))
                if value is None:
                    value = clean(g.get("slider"))
            if isinstance(g.get("index"), int):
                idx = g["index"]
            if isinstance(g.get("pos"), int) and g.get("count"):
                pos, count = g["pos"], g["count"]
            if g.get("locked") is True:
                flags.append("locked")
            if g.get("done") is True:
                flags.append("completed")
            if swf == "MatchOutcome.swf" and gname in ("p2", "p2stats"):
                prefix = "Player 2: "
        if swf == "MainMenu.swf":
            e = self.mm_entry(idx)
            if e:  # the JSON is authoritative here: the on-screen text lags behind state changes
                label, desc = e.get("key") or e.get("debugText") or label, e.get("description") or desc
                count, pos = len(self.mm_entries()), idx
                gname = self.mm_state[-1]
        if label is None:  # JSON fallback: LandingPage Options[], MainMenu States, generic lists
            label, desc, count = self.label_from_json(swf, idx)
            pos = idx
        if swf == "Dojo.swf":
            title = clean(probe.get("title"))
            if label is None and title is None:
                return  # lesson grid not populated yet
            label = ("Lesson %s: %s" % (label, self.S.resolve(title))) if (label and title) else (label or title)
            if not desc:
                desc = clean(probe.get("desc"))
        if label is None and (idx == -1 or groups):
            return  # no focused item yet (entries not populated)
        if label is None:
            label = "item %d" % (idx + 1)
        probe_new = {}
        for pk in ("category", "mode", "difficulty"):  # spoken once, when they change (committed only when we speak)
            pv = clean(probe.get(pk))
            if pv and pv != self.last_probe.get((swf, pk)):
                probe_new[(swf, pk)] = pv
                prefix += ("%s. " % self.S.resolve(pv)) if pk != "difficulty" else ("Difficulty %s. " % self.S.resolve(pv))
        label = self.S.resolve(label)
        desc = self.S.resolve(desc) if desc else ""
        value = self.S.resolve(value) if value else None
        key = (swf, idx, label, value, self.options_state[-1] if swf == "OptionsMenu.swf" else gname)
        self.last_focus = {"swf": swf, "index": idx, "label": label, "desc": desc, "value": value}
        if base_why in ("PlayScrollLeftRight", "PlayScrollLeftRight2") and not deferred and swf in ("OptionsMenu.swf", "PracticePauseMenu.swf"):
            return  # the sound fires before the text changes; the deferred snapshot (or SetVariables) follows with the new value
        if base_why in ("SetVariables", "PlayNegative") and swf == "OptionsMenu.swf":
            if value is not None and not (deferred and key == self.last):
                self.say(str(value) + (", limit" if base_why == "PlayNegative" else ""), interrupt=True, t_hook=rec.get("t"))
                self.last = key
            return
        if key == self.last:
            return
        value_only = self.last is not None and self.last[:3] == key[:3] and value is not None and not prefix
        self.last = key
        self.last_probe.update(probe_new)
        if value_only:  # same item, new toggle value
            if self.skip_value_once == value:
                self.skip_value_once = None  # already spoken from the SetVariables event
            else:
                self.say(str(value), interrupt=True, t_hook=rec.get("t"))
            return
        text = prefix + label
        if value:
            text += ", " + str(value)
        if flags:
            text += ", " + ", ".join(flags)
        if count and pos is not None and self.verbosity >= 1:
            text += ", %d of %d" % (pos + 1, count)
        if desc and self.verbosity >= 2:
            text += ". " + desc
        self.say(text, interrupt=True, t_hook=rec.get("t"))

    def label_from_json(self, swf, idx):
        data = self.populate.get(swf)
        if not isinstance(data, dict):
            return None, None, None
        lists = []
        if swf == "LandingPage.swf":
            lists.append(data.get("Options"))
        for st in (data.get("States") or {}).values():
            if isinstance(st, dict):
                lists.append(st.get("options") or st.get("Entries"))
        if swf == "PracticePauseMenu.swf":
            ents = self.populate.get("PracticePauseMenu.swf:entries")
            if isinstance(ents, dict):
                ents = list(ents.values())
            if isinstance(ents, list):
                lists.append(ents)
        for lst in lists:
            if isinstance(lst, list) and 0 <= idx < len(lst) and isinstance(lst[idx], dict):
                e = lst[idx]
                return e.get("key") or e.get("debugText"), e.get("description") or e.get("descKey"), len(lst)
        return None, None, None

    # ---- hotkeys ----
    def repeat(self):
        self.say(self.last_text or "nothing to repeat", interrupt=True)

    def read_desc(self):
        d = (self.last_focus or {}).get("desc")
        self.say(d or "no description", interrupt=True)

    def read_ticker(self):
        self.say(self.ticker or self.motd or "no news", interrupt=True)

    def cycle_verbosity(self):
        self.verbosity = (self.verbosity + 1) % 3
        self.say("verbosity %d" % self.verbosity, interrupt=True)


# ---------------- hotkeys (global, main thread message loop) ----------------
MOD_CONTROL, MOD_SHIFT, WM_HOTKEY = 0x0002, 0x0004, 0x0312
HOTKEYS = {1: ord("R"), 2: ord("D"), 3: ord("T"), 4: ord("V"), 5: ord("Q")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", default=os.path.join(ROOT, "data", "narrator_speech.log"))
    ap.add_argument("--verbosity", type=int, default=1)
    ap.add_argument("--no-speech", action="store_true")
    ap.add_argument("--events", action="store_true", help="also log every ei/inv event")
    args = ap.parse_args()

    logf = open(args.log, "a", encoding="utf-8")

    def log(line):
        ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        logf.write("%s %s\n" % (ts, line))
        logf.flush()
        print(ts, line[:160])

    strings = Strings(os.path.join(ROOT, "data", "strings_en.tsv"))
    if args.no_speech:
        speak = lambda text, interrupt=True: None
    else:
        from prism_ctypes import Prism
        prism = Prism()
        log("speech backend: %s" % prism.name)
        speak = prism.speak
    nar = Narrator(strings, speak, log, args.verbosity)

    q = queue.Queue()

    def on_message(msg, data):
        if msg["type"] == "send":
            q.put(msg["payload"])
        else:
            log("AGENT ERROR " + str(msg))

    session = frida.attach("KILLERINSTINCTX64_R.EXE")
    script = session.create_script(open(os.path.join(HERE, "agent.js"), encoding="utf-8").read())
    script.on("message", on_message)
    script.load()
    log("attached")

    user32 = ctypes.windll.user32
    for hid, vk in HOTKEYS.items():
        if not user32.RegisterHotKey(None, hid, MOD_CONTROL | MOD_SHIFT, vk):
            log("hotkey %d registration failed" % hid)
    msg = wt.MSG()
    nar.say("Killer Instinct narrator ready", interrupt=True)
    running = True
    try:
        while running:
            while user32.PeekMessageW(ctypes.byref(msg), None, 0, 0, 1):
                if msg.message == WM_HOTKEY:
                    hid = msg.wParam
                    if hid == 1:
                        nar.repeat()
                    elif hid == 2:
                        nar.read_desc()
                    elif hid == 3:
                        nar.read_ticker()
                    elif hid == 4:
                        nar.cycle_verbosity()
                    elif hid == 5:
                        running = False
                user32.TranslateMessage(ctypes.byref(msg))
                user32.DispatchMessageW(ctypes.byref(msg))
            try:
                ev = q.get(timeout=0.02)
            except queue.Empty:
                continue
            kind = ev.get("ev")
            try:
                if kind == "ei":
                    if args.events:
                        log("ei %s.%s %s" % (ev["script"], ev["fn"], ev["json"][:200]))
                    nar.on_ei(ev["script"], ev["fn"], ev["json"], ev.get("t"))
                elif kind == "inv":
                    if args.events:
                        log("inv %s.%s %s" % (ev["swf"], ev["fn"], (ev["json"] or "")[:300]))
                    nar.on_inv(ev["swf"], ev["fn"], ev.get("json"))
                elif kind == "focus":
                    if args.events:
                        log("focus %s" % json.dumps(ev)[:400])
                    nar.on_focus(ev)
                elif kind in ("ready", "root"):
                    log(str(ev))
            except Exception as e:
                log("handler error: %r (%s)" % (e, json.dumps(ev)[:300]))
    finally:
        for hid in HOTKEYS:
            user32.UnregisterHotKey(None, hid)
        try:
            session.detach()
        except Exception:
            pass
        log("detached")


if __name__ == "__main__":
    main()
