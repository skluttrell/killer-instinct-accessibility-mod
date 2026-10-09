#include "narrator.h"
#include "speech.h"
#include <algorithm>
#include <regex>

namespace ki {

// ---------------- static tables (same as narrator.py) ----------------
static const std::map<std::string, std::string> SCREEN_NAMES = {
    {"MainMenu.swf", "Main menu"}, {"OptionsMenu.swf", "Options"}, {"LandingPage.swf", "Landing page"}, {"Popup.swf", "Popup"},
    {"ShadowPopup.swf", "Popup"}, {"CharacterSelect.swf", "Character select"}, {"StoreMain.swf", "Store"}, {"ControllerConfig.swf", "Controller"},
    {"FightArchiveMain.swf", "Fight archive"}, {"StartScreen.swf", "Start screen"}, {"MultiplayerLobby.swf", "Lobby"}, {"StageSelect.swf", "Stage select"},
    {"PauseMenu.swf", "Pause menu"}, {"PracticePauseMenu.swf", "Practice menu"}, {"BlockList.swf", "Block list"},
    {"CommandList.swf", "Command list"}, {"Dojo.swf", "Dojo"}, {"Trials.swf", "Trials"}, {"SinglePlayerLadder.swf", "Arcade ladder"},
    {"MatchOutcome.swf", "Match results"}, {"LoadingScreen.swf", "Loading"}, {"StoreFighters.swf", "Store, fighters"},
    {"StoreBundles.swf", "Store, bundles"}, {"StoreContent.swf", "Store, content"}, {"Leaderboards.swf", "Leaderboards"}, {"Stats.swf", "Stats"},
    {"Replays.swf", "Replays"}, {"ComboBreakerOptions.swf", "Combo breaker options"}, {"ShadowHub.swf", "Shadow Lab"},
    {"MultiplayerRanked.swf", "Ranked"}, {"MultiplayerExhibition.swf", "Exhibition"}, {"Story.swf", "Story"}, {"GA_HUB.swf", "Shadow Lords"},
};

static const std::map<std::string, std::string> CMD_TOKENS = {
    {"DOWN", "down"}, {"DOWNBACK", "down-back"}, {"BACK", "back"}, {"UPBACK", "up-back"}, {"UP", "up"}, {"UPFORWARD", "up-forward"},
    {"FORWARD", "forward"}, {"DOWNFORWARD", "down-forward"}, {"LOWPUNCH", "light punch"}, {"MEDPUNCH", "medium punch"},
    {"HIGHPUNCH", "heavy punch"}, {"LOWKICK", "light kick"}, {"MEDKICK", "medium kick"}, {"HIGHKICK", "heavy kick"},
    {"ANYPUNCH", "any punch"}, {"ANYKICK", "any kick"}, {"PLUS", "plus"},
};

static const std::map<std::string, std::string> ICON_WORDS = {
    {"ArrowUp", "up"}, {"ArrowDown", "down"}, {"ArrowLeft", "left"}, {"ArrowRight", "right"}, {"Plus", "plus"},
    {"ArrowLeftDown", "down-back"}, {"ArrowRightDown", "down-forward"}, {"ArrowLeftUp", "up-back"}, {"ArrowRightUp", "up-forward"},
};

static std::string upper(std::string s) { for (auto& c : s) c = (char)toupper((unsigned char)c); return s; }
static std::string strip(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
static std::string tokenWord(const std::string& tok) {
    auto it = CMD_TOKENS.find(upper(strip(tok)));
    return it != CMD_TOKENS.end() ? it->second : lower(strip(tok));
}

std::string humanize_command(const json& cmd) {
    if (!cmd.is_string()) return "";
    std::string s = cmd.get<std::string>();
    std::vector<std::string> out;
    size_t p = 0;
    while (p < s.size()) {
        size_t a = s.find("<TT>", p);
        std::string plain = s.substr(p, a == std::string::npos ? std::string::npos : a - p);
        std::string t = strip(plain);
        if (t == "+") out.push_back("plus");
        else if (!t.empty()) out.push_back(t);
        if (a == std::string::npos) break;
        size_t b = s.find("</TT>", a + 4);
        if (b == std::string::npos) { out.push_back(tokenWord(s.substr(a + 4))); break; }
        out.push_back(tokenWord(s.substr(a + 4, b - a - 4)));
        p = b + 5;
    }
    return join(out, " ");
}

std::string icon_words(const json& name) {
    if (!name.is_string()) return "";
    std::string n = name.get<std::string>();
    auto it = ICON_WORDS.find(n);
    if (it != ICON_WORDS.end()) return it->second;
    std::string out;
    for (size_t i = 0; i < n.size(); i++) {
        if (i > 0 && isupper((unsigned char)n[i]) && islower((unsigned char)n[i - 1])) out += ' ';
        out += (char)tolower((unsigned char)n[i]);
    }
    return out;
}

static std::string command_words(const json& entries, const Strings& S) {
    std::vector<std::string> out;
    bool has_text = false;
    if (entries.is_array())
        for (const auto& e : entries) if (e.is_object() && jstr(e, "Type") == "String") has_text = true;
    if (entries.is_array()) {
        for (const auto& e : entries) {
            std::string w;
            if (e.is_object()) {
                if (jstr(e, "Type") == "String") w = S.resolve(jget(e, "Entry"));
                else if (jstr(e, "Entry") == "Plus" || !has_text) w = icon_words(jget(e, "Entry"));
            } else if (e.is_string()) w = S.resolve(e);
            if (!w.empty()) out.push_back(w);
        }
    }
    return join(out, " ");
}

// json "clean": None for missing / "" / "missing text"
static json clean(const json& v) {
    if (v.is_null() || v.is_discarded()) return nullptr;
    if (v.is_string()) {
        const std::string& s = v.get_ref<const std::string&>();
        if (s.empty() || s == "missing text") return nullptr;
    }
    return v;
}
static bool isNone(const json& v) { return v.is_null() || v.is_discarded(); }
static std::string sv(const json& v) { return v.is_string() ? v.get<std::string>() : jtostr(v); }   // str(v)
static std::string keyOf(const std::vector<std::string>& parts) { return join(parts, "\x1f"); }

// ---------------- Narrator ----------------
Narrator::Narrator(Strings& strings) : S(strings) {}

std::string Narrator::screenName(const std::string& swf) {
    if (swf.empty()) return "unknown screen";
    auto it = SCREEN_NAMES.find(swf);
    if (it != SCREEN_NAMES.end()) return it->second;
    return replaceAll(swf, ".swf", "");
}

std::string Narrator::fighterName(const std::string& code, const std::string& def) {
    const json& v = jget(fighter_names_, code.c_str());
    return v.is_string() ? v.get<std::string>() : def;
}

void Narrator::loadData() {
    names_path_ = dataDir() + L"data\\fighter_names.json";
    std::string txt;
    if (readFile(names_path_, txt)) {
        json j = json::parse(txt, nullptr, false);
        if (j.is_object()) fighter_names_ = j;
    }
    if (readFile(dataDir() + L"data\\fighter_appearance.json", txt)) {
        json j = json::parse(txt, nullptr, false);
        if (j.is_object()) fighter_appearance_ = j;
    } else logLine("warning: data\\fighter_appearance.json missing, appearance hotkey disabled");
    // last known Populate payloads (the DLL may be loaded after a screen populated, e.g. when injected)
    WIN32_FIND_DATAW fd;
    std::wstring pat = dataDir() + L"data\\populate_*.json";
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::string name = utf8(fd.cFileName);
            std::string core = name.substr(9, name.size() - 9 - 5);   // populate_<core>.json
            if (core.find(':') != std::string::npos || core.find("_entries") != std::string::npos) continue;
            std::string data;
            if (readFile(dataDir() + L"data\\" + fd.cFileName, data)) {
                json j = json::parse(data, nullptr, false, true);
                if (!j.is_discarded()) populate_[core + ".swf"] = j;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
}

void Narrator::learn_fighter_names(const json& data) {
    bool changed = false;
    static const std::regex reImg(R"(/(\w+)\.dds$)"), reTitle(R"(^&?(.+?) - Lvl)");
    for (const char* side : {"Player1Info", "Player2Info"}) {
        const json& entries = jget(jget(jget(data, side), "Expanded"), "Entries");
        if (!entries.is_array()) continue;
        for (const auto& e : entries) {
            std::string img = jstr(e, "CharacterImage"), title = jstr(e, "Title");
            std::smatch m, t;
            if (std::regex_search(img, m, reImg) && std::regex_search(title, t, reTitle)) {
                std::string code = lower(m[1].str());
                if (fighterName(code, "") != t[1].str()) { fighter_names_[code] = t[1].str(); changed = true; }
            }
        }
    }
    if (changed) writeFile(names_path_, fighter_names_.dump(1));
}

void Narrator::say(const std::string& textIn, bool interrupt, int64_t tHook) {
    std::string text = textIn;
    if (text.empty()) return;
    if (text.find("\\n") != std::string::npos || text.find('\n') != std::string::npos)
        text = replaceAll(replaceAll(text, "\\n", " "), "\n", " ");
    if (text.find('<') != std::string::npos) {   // inline icon tags: <TT>FORWARD</TT>, <img ...>, <font>
        std::string out;
        size_t p = 0;
        while (p < text.size()) {
            if (text.compare(p, 4, "<TT>") == 0) {
                size_t b = text.find("</TT>", p + 4);
                if (b == std::string::npos) { out += ' '; p += 4; continue; }
                out += " " + tokenWord(text.substr(p + 4, b - p - 4)) + " ";
                p = b + 5;
            } else if (text[p] == '<') {
                size_t b = text.find('>', p);
                if (b == std::string::npos) { out += text.substr(p); break; }
                out += ' ';
                p = b + 1;
            } else out += text[p++];
        }
        std::string sq;
        bool ws = false;
        for (char c : out) {
            if (isspace((unsigned char)c)) { if (!ws && !sq.empty()) sq += ' '; ws = true; }
            else { sq += c; ws = false; }
        }
        text = strip(sq);
    }
    std::string lat;
    if (tHook) lat = " [" + std::to_string(nowMs() - tHook) + " ms]";
    logLine("SAY" + lat + ": " + text);
    last_text_ = text;
    if (g_cfg.speech) speech::speak(text, interrupt);
}

// ---------------- events ----------------
void Narrator::on_inv(const std::string& swf, const std::string& fn, const std::string& js) {
    json data;
    if (!js.empty()) {
        data = json::parse(js, nullptr, false, true);
        if (data.is_discarded()) {
            // nlohmann rejects raw control characters in strings; popup bodies carry raw newlines
            std::string fixed;
            fixed.reserve(js.size() + 16);
            for (char c : js) { if (c == '\n') fixed += "\\n"; else if (c == '\r') fixed += "\\r"; else if (c == '\t') fixed += "\\t"; else fixed += c; }
            data = json::parse(fixed, nullptr, false, true);
            if (data.is_discarded() && (startsWith(fn, "Populate") || startsWith(fn, "Lua_Populate") || fn == "RefreshPage"))
                logLine("json parse failed for " + swf + "." + fn + " (" + js.substr(0, 120) + ")");
        }
    }
    if (fn == "Populate" || fn == "Lua_Populate" || fn == "PopulateStatesOnly") {
        if (data.is_array()) data = json{{"Modes", data}};   // Dojo sends an array of modes
        if (data.is_object()) {
            populate_[swf] = data;
            if (g_cfg.dumpPopulate) writeFile(dataDir() + L"data\\populate_" + wide(replaceAll(swf, ".swf", "")) + L".json", data.dump(1));
            if (swf == "OptionsMenu.swf") options_state_ = {jtruthy(jget(data, "GraphicsOptionOnly")) ? "Graphics" : "Main"};
            if (swf == "MainMenu.swf") { std::string st = jstr(data, "InitialState"); mm_state_ = {st.empty() ? "SinglePlayer" : st}; }
            if (swf == "CharacterSelect.swf") {
                cs_stage_ = {{0, "fighter"}, {1, "fighter"}};
                cs_fighter_code_.clear();
                cs_costume_.clear();
                cs_desc_side_ = 0;
                cs_active_ = 0;
                cs_last_.clear();
            }
        }
        if (swf == "Popup.swf" || swf == "ShadowPopup.swf") {
            popup_spoken_ = data.is_object();
            say_popup(data);
        } else if (swf == "Toast.swf" && data.is_object()) {
            std::vector<std::string> parts;
            for (const char* k : {"Title", "Description"}) if (jtruthy(jget(data, k))) { std::string p = S.resolve(jget(data, k)); if (!p.empty()) parts.push_back(p); }
            const json& xp = jget(data, "XPValue");
            if (!isNone(xp) && !(xp.is_string() && xp.get<std::string>().empty())) parts.push_back(S.resolve(xp) + " XP");
            say("Notification. " + join(parts, ". "), false);
        } else if (swf == "LoadingScreen.swf" && data.is_object()) {
            std::vector<std::string> names;
            for (const char* k : {"p1Thumb", "p2Thumb"}) {
                std::string v = jstr(data, k);
                if (v.empty()) continue;
                std::string f = replaceAll(v, "\\", "/");
                f = f.substr(f.find_last_of('/') == std::string::npos ? 0 : f.find_last_of('/') + 1);
                size_t dot = f.find_last_of('.');
                if (dot != std::string::npos) f = f.substr(0, dot);
                std::string code = lower(f.substr(0, f.find('_')));
                std::string disp = fighterName(code, "");
                if (disp.empty()) { disp = code; if (!disp.empty()) disp[0] = (char)toupper((unsigned char)disp[0]); }
                names.push_back(disp);
            }
            say(names.empty() ? "Loading" : "Loading. " + join(names, " versus "), true);
        } else if (swf == "StageSelect.swf") {
            last_probe_.erase("StageSelect.swf|stage");
        } else if (swf == "MatchOutcome.swf" && data.is_object()) {
            say_match_outcome(data);
        }
    } else if (fn == "RefreshPage" && swf == "CommandList.swf" && data.is_object()) {
        cmdlist_ = data;
        std::vector<std::string> parts;
        const json& gcrc = jget(data, "GroupNameCRC");
        if (!isNone(gcrc)) { std::string g = S.resolve("#" + jtostr(gcrc)); if (!g.empty()) parts.push_back(g); }
        if (jtruthy(jget(data, "CharacterName"))) { std::string c = S.resolve("&" + jtostr(jget(data, "CharacterName"))); if (!c.empty()) parts.push_back(c); }
        std::string text = join(parts, ". ");
        if (!text.empty()) {
            say(text + ", " + std::to_string(jint(jget(data, "NumMoves"))) + " moves", true);
            cmd_group_spoken_ = true;
        }
    } else if (fn == "PopulateEntries" && swf == "PracticePauseMenu.swf") {
        populate_["PracticePauseMenu.swf:entries"] = data;
    } else if (fn == "Lua_ReceiveAnyKeyPress" && swf == "ControllerConfig.swf" && data.is_object()) {
        std::string disp = S.resolve(jget(data, "KeyDisplayString"));
        if (disp.empty()) disp = jstr(data, "KeyBinding");
        if (disp.empty()) disp = "key";
        int side = startsWith(jstr(last_focus_, "label"), "Player 2") ? 1 : 0;
        const json& iv = jget(last_focus_, "index");
        json& cmds = populate_["ControllerConfig.swf"]["Commands"];
        if (jisInt(iv) && cmds.is_array()) {
            int i = iv.get<int>();
            if (i >= 0 && i < (int)cmds.size() && cmds[i].is_object()) {
                json& btn = cmds[i]["Button"];
                if (!btn.is_array()) btn = json::array({json::object(), json::object()});
                if (side < (int)btn.size()) btn[side] = json{{"binding", jget(data, "KeyBinding")}, {"displayString", jget(data, "KeyDisplayString")}};
            }
        }
        say(disp + " assigned", true);
    } else if (fn == "PopulatePlayerCards" && data.is_object()) {
        learn_fighter_names(data);
    } else if (fn == "PopulateTicker" && data.is_object()) {
        ticker_ = S.resolve(jget(data, "motdString"));
    } else if (fn == "PopulateMOTD" && data.is_object()) {
        std::string motd = S.resolve(jget(data, "Title")) + ". " + S.resolve(jget(data, "Message"));
        if (motd != motd_) {   // the game re-sends it every 30 s
            motd_ = motd;
            if (verbosity_ >= 1) say(motd, false);
        }
    } else if (swf == "CharacterSelect.swf" && fn == "Lua_PopulateColorData" && data.is_object()) {
        cs_colors_[jint(jget(data, "PlayerIndex"))] = jget(data, "ColorList").is_array() ? jget(data, "ColorList") : json::array();
    } else if (fn == "Lua_PopulateDailyLootExpanded" && data.is_object()) {
        std::vector<std::string> days;
        const json& pr = jget(data, "previewRewardData");
        if (pr.is_array()) for (const auto& d : pr) if (d.is_object()) { std::string n = S.resolve(jget(d, "name")); if (!n.empty()) days.push_back(n); }
        say("Daily rewards panel. " + S.resolve(jget(data, "resetText")) + ". " + join(days, ", ") + ". Escape to close", true);
    }
}

void Narrator::say_match_outcome(const json& data) {
    std::vector<std::string> parts{"Match results"};
    const json& w = jget(data, "Winner");
    if (jisInt(w) && (w.get<int>() == 0 || w.get<int>() == 1)) parts.push_back("Player " + std::to_string(w.get<int>() + 1) + " wins");
    for (int side = 0; side < 2; side++) {
        std::string pk = "Player" + std::to_string(side + 1) + "State", sk = "p" + std::to_string(side + 1) + "ShowStats";
        const json& st = jget(data, pk.c_str());
        if (!jtruthy(jget(data, sk.c_str())) || !st.is_object()) continue;
        std::string streak = strip(S.resolve(jget(st, "WinStreak")));
        if (!streak.empty()) parts.push_back(streak);
        if (verbosity_ >= 2) {
            for (const char* cat : {"Hero", "Offense", "Defense", "Combos", "Variety"}) {
                const json& c = jget(st, cat);
                if (!c.is_object()) continue;
                std::vector<std::string> ms;
                const json& metrics = jget(c, "metrics");
                if (metrics.is_array()) for (const auto& m : metrics) if (m.is_object()) ms.push_back(S.resolve(jget(m, "key")) + " " + S.resolve(jget(m, "stat")));
                if (!ms.empty()) { std::string ck = S.resolve(jget(c, "key")); parts.push_back((ck.empty() ? std::string(cat) : ck) + ": " + join(ms, ", ")); }
            }
        }
    }
    say(join(parts, ". "), true);
}

void Narrator::say_popup(const json& data) {
    if (!data.is_object()) return;
    std::vector<std::string> parts;
    for (const char* k : {"Title", "title", "Header", "Message", "message", "Body", "Text", "text", "Description"}) {
        const json& v = jget(data, k);
        if (v.is_string() && !v.get<std::string>().empty()) parts.push_back(S.resolve(v));
    }
    std::vector<std::string> buttons;
    const std::pair<const char*, const char*> keys[] = {{"ABUTTON", "Enter"}, {"BBUTTON", "Escape"}, {"XBUTTON", "X"}, {"YBUTTON", "Y"}};
    for (auto& kv : keys) {
        const json& b = jget(data, kv.first);
        if (b.is_object() && jtruthy(jget(b, "visible")) && jtruthy(jget(b, "text"))) buttons.push_back(std::string(kv.second) + ": " + S.resolve(jget(b, "text")));
    }
    for (const char* k : {"OptionsButtons", "Buttons", "buttons", "Options", "options", "Entries"}) {
        const json& v = jget(data, k);
        if (!v.is_array()) continue;
        for (const auto& b : v) {
            if (b.is_object()) {
                json lab;
                for (const char* lk : {"key", "text", "Text", "label"}) if (jtruthy(jget(b, lk))) { lab = jget(b, lk); break; }
                if (!isNone(lab)) buttons.push_back(S.resolve(lab));
            } else if (b.is_string()) buttons.push_back(S.resolve(b));
        }
    }
    if (parts.empty())   // unknown shape: speak every string value
        for (auto it = data.begin(); it != data.end(); ++it) if (it.value().is_string() && !it.value().get<std::string>().empty()) parts.push_back(S.resolve(it.value()));
    std::vector<std::string> nonEmpty;
    for (auto& p : parts) if (!p.empty()) nonEmpty.push_back(p);
    std::string text = "Popup. " + join(nonEmpty, ". ");
    if (!buttons.empty()) text += ". Buttons: " + join(buttons, ", ");
    say(text, true);
}

void Narrator::on_ei(const std::string& script, const std::string& fn, const std::string& js, int64_t t) {
    if (fn == "LoadDestination") {
        json j = json::parse(js, nullptr, false, true);
        std::string dest = jstr(j, "Destination");
        if (!dest.empty()) {
            current_swf_ = dest;
            hasLast_ = false;
            say(screenName(dest), true, t);
        }
    } else if (fn == "SetVariables" && script == "OptionsMenu" && current_swf_ != "OptionsMenu.swf") {
        // shared toggle component on another screen (practice menu): the new value is in the event itself
        json j = json::parse(js, nullptr, false, true);
        std::string v = S.resolve(jget(j, "value"));
        if (!v.empty()) {
            skip_value_once_ = v;
            hasSkip_ = true;
            say(v, true, t);
        }
    } else if (fn == "PlayAccept" && current_swf_ == "MainMenu.swf") {
        json e = mm_entry(jget(last_focus_, "index"));
        if (e.is_object() && jget(e, "nextState").is_string()) mm_state_.push_back(jstr(e, "nextState"));
    } else if (fn == "PlayBack" && current_swf_ == "MainMenu.swf") {
        if (mm_state_.size() > 1) mm_state_.pop_back();
    } else if (fn == "PlayAccept" && current_swf_ == "OptionsMenu.swf") {
        options_select();
    } else if (fn == "PlayBack" && current_swf_ == "OptionsMenu.swf") {
        if (options_state_.size() > 1) {
            options_state_.pop_back();
            hasLast_ = false;
            std::string title = S.resolve(jget(options_entries().first, "Title"));
            say(title.empty() ? "Options" : title, true, t);
        }
    } else if (script == "CharSelectMenu" || (current_swf_ == "CharacterSelect.swf" && (fn == "PlayBackP1" || fn == "PlayBackP2"))) {
        on_charselect_ei(fn, js, t);
    } else if (script == "StageSelect" && fn == "StageSelectionChanged") {
        on_stage_changed(js, t);
    } else if (script == "CommandList" && fn == "EntrySelected") {
        on_move_selected(js, t);
    }
}

// ---- stage select (event-driven: the screen tells Lua the 1-based index on every move) ----
void Narrator::on_stage_changed(const std::string& js, int64_t t) {
    char* end = nullptr;
    double d = strtod(js.c_str(), &end);
    if (end == js.c_str()) return;
    int n = (int)d;
    const json& stages = jget(jget(populate_, "StageSelect.swf"), "Stages");
    std::string text;
    int count = stages.is_array() ? (int)stages.size() : 0;
    if (n >= 1 && n <= count && stages[n - 1].is_object()) {
        const json& st = stages[n - 1];
        text = S.resolve(jget(st, "name"));
        if (text.empty()) text = "stage " + std::to_string(n);
        if (jget(st, "unlocked").is_boolean() && !jget(st, "unlocked").get<bool>()) text += ", locked";
        if (jget(st, "installed").is_boolean() && !jget(st, "installed").get<bool>()) text += ", not installed";
        if (verbosity_ >= 1) text += ", " + std::to_string(n) + " of " + std::to_string(count);
    } else text = "stage " + std::to_string(n);
    if (last_probe_["StageSelect.swf|stage"] == text) return;
    last_probe_["StageSelect.swf|stage"] = text;
    last_focus_ = json{{"swf", "StageSelect.swf"}, {"index", n - 1}, {"label", text}, {"desc", ""}, {"value", nullptr}};
    say(text, true, t);
}

// ---- command list (event-driven: EntrySelected {MoveIndex}; moves from the RefreshPage payload) ----
void Narrator::on_move_selected(const std::string& js, int64_t t) {
    json j = json::parse(js, nullptr, false, true);
    const json& mi = jget(j, "MoveIndex");
    if (!mi.is_number()) return;
    int i = (int)mi.get<double>();
    const json& moves = jget(cmdlist_, "Moves");
    if (!moves.is_array() || i < 0 || i >= (int)moves.size() || !moves[i].is_object()) return;
    const json& m = moves[i];
    std::string name = isNone(jget(m, "NameCRC")) ? "move " + std::to_string(i + 1) : S.resolve("#" + jtostr(jget(m, "NameCRC")));
    std::string cmd = humanize_command(jget(m, "Command"));
    std::string text = name;
    if (!cmd.empty()) text += ". " + cmd;
    if (jtruthy(jget(m, "Favorite"))) text += ", key move";
    if (verbosity_ >= 1) text += ", " + std::to_string(i + 1) + " of " + std::to_string(moves.size());
    std::string desc = isNone(jget(m, "Description")) ? "" : S.resolve("#" + jtostr(jget(m, "Description")));
    if (!desc.empty() && desc != name && verbosity_ >= 2) text += ". " + desc;
    last_focus_ = json{{"swf", "CommandList.swf"}, {"index", i}, {"label", name}, {"desc", desc}, {"value", cmd}};
    say(text, !cmd_group_spoken_, t);
    cmd_group_spoken_ = false;
}

// ---- controller config (scan groups p1/p2 = command rows + 2 toggles, p1exit/p2exit = Save/Default) ----
void Narrator::on_controller_focus(const json& rec, const json& probe, bool) {
    const json& data = jget(populate_, "ControllerConfig.swf");
    const json& groups = jget(rec, "groups");
    std::string why = jstr(rec, "why");
    while (!why.empty() && why.back() == '+') why.pop_back();
    int64_t t = jget(rec, "t").is_number() ? (int64_t)jget(rec, "t").get<double>() : 0;
    for (int side = 0; side < 2; side++) {
        std::string pn = "p" + std::to_string(side + 1);
        const json& panel = jget(probe, (pn + "panel").c_str());
        const json& start = jget(probe, (pn + "start").c_str());
        if (panel.is_boolean() && !panel.get<bool>() && start.is_boolean() && start.get<bool>() && why == "ScreenShown") {
            std::string prompt = jstr(probe, (pn + "prompt").c_str());
            if (prompt.empty()) prompt = S.resolve(jget(data, side == 0 ? "PressStartText" : "P2PressStartText"));
            if (prompt.empty()) prompt = "Press Enter";
            if (prompt != "missing text") say("Player " + std::to_string(side + 1) + ": " + prompt + " to edit controls", side == 0);
            continue;
        }
        const json& g = jget(groups, pn.c_str());
        const json& ex = jget(groups, (pn + "exit").c_str());
        std::string text, key;
        bool have = false;
        const json& gi = jget(g, "index");
        if (!isNone(gi)) {
            int i = jint(gi, -1);
            if (jisInt(gi) && i < 13) {
                const json& cmds = jget(data, "Commands");
                std::string words, bound;
                bool haveBound = false;
                int ncmd = cmds.is_array() ? (int)cmds.size() : 0;
                if (i >= 0 && i < ncmd && cmds[i].is_object()) {
                    words = command_words(jget(cmds[i], "Entries"), S);
                    const json& btn = jget(cmds[i], "Button");
                    if (btn.is_array() && side < (int)btn.size()) {
                        const json& b = btn[side];
                        if (b.is_object()) { bound = S.resolve(jget(b, "displayString")); haveBound = true; }
                        else if (b.is_string()) { bound = S.resolve(b); haveBound = true; }
                    }
                }
                text = words.empty() ? "command " + std::to_string(i + 1) : words;
                bool remapping = jstr(g, "state") == "RemappingLoop";
                if (remapping) text += ". Press a key";
                else if (haveBound && !bound.empty()) text += ": " + bound;
                else text += ": unassigned";
                bool warn = startsWith(jstr(g, "warnState"), "ShowMessage") && !isNone(clean(jget(g, "warn")));
                if (warn) text += ". " + jstr(g, "warn");
                if (verbosity_ >= 1 && ncmd) text += ", " + std::to_string(i + 1) + " of " + std::to_string(ncmd);
                key = keyOf({std::to_string(side), "cmd", std::to_string(i), remapping ? "1" : "0", bound, warn ? "1" : "0"});
                have = true;
            } else {
                json label = clean(jget(g, "text")), value = clean(jget(g, "value"));
                if (isNone(label)) continue;
                text = sv(label) + (isNone(value) ? "" : ", " + sv(value));
                key = keyOf({std::to_string(side), "toggle", std::to_string(i), sv(label), sv(value)});
                have = true;
            }
        } else if (!isNone(clean(jget(ex, "label")))) {
            text = jstr(ex, "label") + ", button";
            key = keyOf({std::to_string(side), "exit", jtostr(jget(ex, "index")), jstr(ex, "label")});
            have = true;
        }
        if (!have) continue;
        std::string lk = "cc|" + std::to_string(side);
        if (cs_last_.count(lk) && cs_last_[lk] == key) continue;
        cs_last_[lk] = key;
        last_focus_ = json{{"swf", "ControllerConfig.swf"}, {"index", jget(g, "index")}, {"label", text}, {"desc", ""}, {"value", nullptr}};
        say((side == 1 ? "Player 2: " : "") + text, true, t);
    }
}

// ---- character select ----
json Narrator::cs_fighter(const json& idx) {
    const json& fl = jget(jget(populate_, "CharacterSelect.swf"), "FighterSelection");
    if (jisInt(idx) && fl.is_array()) {
        int i = idx.get<int>();
        if (i >= 0 && i < (int)fl.size() && fl[i].is_object()) return fl[i];
    }
    return nullptr;
}

std::string Narrator::cs_fighter_text(const json& e) {
    if (!e.is_object()) return "";
    std::string name = jstr(e, "name");
    if (name.empty()) name = "?";
    std::string text = fighterName(lower(name), "");
    if (text.empty()) {
        text = S.resolve("CHARACTER_" + upper(name));
        if (text == "CHARACTER_" + upper(name)) text = name;
    }
    if (jtruthy(jget(e, "isRandom"))) text = "Random";
    std::vector<std::string> flags;
    const std::pair<const char*, const char*> fk[] = {{"purchased", "not purchased"}, {"installed", "not installed"}, {"released", "not released"}, {"selectable", "locked"}};
    for (auto& kv : fk) { const json& v = jget(e, kv.first); if (v.is_boolean() && !v.get<bool>()) flags.push_back(kv.second); }
    if (!flags.empty()) text += ", " + join(flags, ", ");
    return text;
}

void Narrator::on_charselect_ei(const std::string& fn, const std::string& js, int64_t t) {
    json sel = js.empty() ? json::object() : json::parse(js, nullptr, false, true);
    if (!sel.is_object()) sel = json::object();
    if (jget(sel, "selectionData").is_object()) {   // AS_SetSelections wraps the selection
        json merged = sel;
        for (auto it = sel["selectionData"].begin(); it != sel["selectionData"].end(); ++it) merged[it.key()] = it.value();
        sel = merged;
    }
    int side = jint(jget(sel, "PlayerIndex"));
    bool confirmed = jtruthy(jget(sel, "Confirmed"));
    const json& data = jget(populate_, "CharacterSelect.swf");
    std::string fname = jstr(sel, "Name");
    std::string p2 = side == 1 ? "Player 2: " : "";
    if (fn == "AS_SetSelections" && side == 1 && cs_stage_[0] == "waiting" && cs_stage_[1] == "fighter") {
        cs_active_ = 1;   // Player 1 is ready and now drives the Player 2 (dummy / CPU) cursor
        cs_last_.erase("fighter|1");
        say("Now choosing Player 2's fighter", true, t);
        return;
    }
    if (fn == "AS_PlayerPickedFighter") {
        cs_stage_[side] = "costume";
        if (!fname.empty()) cs_fighter_code_[side] = lower(fname);
        cs_costume_.erase(side);
        cs_desc_side_ = side;
        std::string disp = fname.empty() ? "Fighter" : fighterName(lower(fname), fname);
        say(p2 + disp + " chosen. Costume", true, t);
    } else if (fn == "AS_PlayerPickedCostume") {
        const json& costumes = jget(jget(data, "CostumeSelection"), fname.c_str());
        const json& ci = jget(sel, "CostumeIndex");
        if (jisInt(ci)) cs_costume_[side] = ci.get<int>();
        cs_desc_side_ = side;
        std::string text;
        int n = costumes.is_array() ? (int)costumes.size() : 0;
        if (jisInt(ci) && ci.get<int>() >= 0 && ci.get<int>() < n && costumes[ci.get<int>()].is_object()) {
            const json& c = costumes[ci.get<int>()];
            text = S.resolve(jget(c, "name"));
            if (text.empty()) text = "costume " + std::to_string(ci.get<int>() + 1);
            if (jget(c, "purchased").is_boolean() && !jget(c, "purchased").get<bool>()) text += ", not purchased";
            if (jtruthy(jget(c, "custom"))) text += ", custom slot " + std::to_string(jint(jget(sel, "CustomSlot")) + 1);
            text += ", " + std::to_string(ci.get<int>() + 1) + " of " + std::to_string(n);
        } else text = "costume " + jtostr(ci);
        std::string lk = "costume|" + std::to_string(side);
        if (confirmed) { cs_stage_[side] = "color"; say(p2 + text + " chosen. Color", true, t); }
        else if (!cs_last_.count(lk) || cs_last_[lk] != text) say(p2 + text, true, t);
        cs_last_[lk] = text;
    } else if (fn == "AS_PlayerPickedColor") {
        const json& colors = cs_colors_[side];
        const json& bi = jget(sel, "ColorIndex");
        std::string text;
        int pos = -1;
        if (colors.is_array()) for (int k = 0; k < (int)colors.size(); k++) if (colors[k].is_object() && jget(colors[k], "backendIndex") == bi) { pos = k; break; }
        if (pos >= 0) {
            const json& entry = colors[pos];
            text = S.resolve(jget(entry, "name"));
            if (text.empty()) text = "color " + jtostr(bi);
            if (jget(entry, "purchased").is_boolean() && !jget(entry, "purchased").get<bool>()) text += ", not purchased";
            text += ", " + std::to_string(pos + 1) + " of " + std::to_string(colors.size());
        } else text = "color " + jtostr(bi);
        std::string lk = "color|" + std::to_string(side);
        if (confirmed) { cs_stage_[side] = "waiting"; say(p2 + text + " chosen. Ready", true, t); }
        else if (!cs_last_.count(lk) || cs_last_[lk] != text) say(p2 + text, true, t);
        cs_last_[lk] = text;
    } else if (fn == "AS_ResetPlayerSelection") {
        cs_stage_[side] = "fighter";
        cs_costume_.erase(side);
        if (side == 0) cs_active_ = 0;
        cs_last_.erase("fighter|" + std::to_string(side));
        say(p2 + "Back to fighter select", true, t);
    } else if (fn == "PlayBackP1" || fn == "PlayBackP2") {   // stepping back one stage: let the next stage event speak again
        int s = endsWith(fn, "P2") ? 1 : 0;
        if (cs_stage_[s] == "color") cs_stage_[s] = "costume";
        cs_last_.erase("costume|" + std::to_string(s));
        cs_last_.erase("color|" + std::to_string(s));
    }
}

void Narrator::on_charselect_focus(const json& rec) {
    const json& cs = jget(rec, "cs");
    std::string why = jstr(rec, "why");
    int side = endsWith(why, "P2") ? 1 : 0;
    if (side == 0 && cs_stage_[0] == "waiting" && cs_active_ == 1) side = 1;   // Player 1's keys now move the Player 2 cursor
    if (cs_stage_[side] != "fighter") return;
    const json& idx = jget(cs, ("f" + std::to_string(side)).c_str());
    json e = cs_fighter(idx);
    std::string text = cs_fighter_text(e);
    if (text.empty()) text = "fighter " + jtostr(idx);
    if (e.is_object()) { cs_fighter_code_[side] = lower(jstr(e, "name")); cs_desc_side_ = side; }
    const json& fl = jget(jget(populate_, "CharacterSelect.swf"), "FighterSelection");
    if (fl.is_array() && !fl.empty() && jisInt(idx) && verbosity_ >= 1) text += ", " + std::to_string(idx.get<int>() + 1) + " of " + std::to_string(fl.size());
    if (why == "PlayNegativeP" + std::to_string(side + 1)) text += ", locked";
    std::string lk = "fighter|" + std::to_string(side);
    if (cs_last_.count(lk) && cs_last_[lk] == text) return;
    cs_last_[lk] = text;
    last_focus_ = json{{"swf", "CharacterSelect.swf"}, {"index", idx}, {"label", text}, {"desc", ""}, {"value", nullptr}};
    int64_t t = jget(rec, "t").is_number() ? (int64_t)jget(rec, "t").get<double>() : 0;
    say((side == 1 ? "Player 2: " : "") + text, true, t);
}

json Narrator::mm_entries() {
    const json& data = jget(populate_, "MainMenu.swf");
    if (!data.is_object() || mm_state_.empty()) return json::array();
    const json& st = jget(jget(data, "States"), mm_state_.back().c_str());
    const json& opts = jget(st, "options");
    if (opts.is_array()) return opts;
    const json& ents = jget(st, "Entries");
    return ents.is_array() ? ents : json::array();
}

json Narrator::mm_entry(const json& idx) {
    json ents = mm_entries();
    if (jisInt(idx)) {
        int i = idx.get<int>();
        if (i >= 0 && i < (int)ents.size() && ents[i].is_object()) return ents[i];
    }
    return nullptr;
}

std::pair<json, json> Narrator::options_entries() {
    const json& data = jget(populate_, "OptionsMenu.swf");
    if (!data.is_object()) return {nullptr, nullptr};
    const json& st = jget(jget(data, "States"), options_state_.back().c_str());
    const json& ents = jget(st, "Entries");
    return {st.is_object() ? st : json::object(), ents.is_array() ? ents : json::array()};
}

void Narrator::options_select() {
    auto se = options_entries();
    const json& entries = se.second;
    if (!entries.is_array() || entries.empty() || isNone(last_focus_)) return;
    std::string label = jstr(last_focus_, "label");
    for (const auto& e : entries) {
        if (S.resolve(jget(e, "key")) == label || jstr(e, "debugText") == label) {
            const json& ns = jget(e, "nextState");
            if (ns.is_string() && ns.get<std::string>() != "Help") {
                options_state_.push_back(ns.get<std::string>());
                hasLast_ = false;
                const json& st2 = jget(jget(jget(populate_, "OptionsMenu.swf"), "States"), ns.get<std::string>().c_str());
                std::string title = S.resolve(jget(st2, "Title"));
                say(title.empty() ? ns.get<std::string>() : title, true);
            }
            break;
        }
    }
}

std::string Narrator::state_of(const json& g) {
    std::string st = sv(jget(g, "state"));
    if (st == "RemappingLoop") return "focused";
    if (st.find("Focus") != std::string::npos && !startsWith(st, "Lose") && !startsWith(st, "Unfocused")) return "focused";
    if (startsWith(st, "Lose") || startsWith(st, "Unfocused")) return "unfocused";
    return "unknown";
}

std::pair<std::string, json> Narrator::pick_group(const json& groups) {
    std::pair<std::string, json> best{"", json::object()};
    bool haveBest = false;
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const json& g = it.value();
        if (!g.is_object()) continue;
        bool has_label = !isNone(clean(jget(g, "label")));
        if (has_label && state_of(g) == "focused") return {it.key(), g};
        if (!haveBest && has_label) { best = {it.key(), g}; haveBest = true; }
    }
    return best;
}

std::tuple<json, json, json> Narrator::label_from_json(const std::string& swf, int idx) {
    const json& data = jget(populate_, swf.c_str());
    if (!data.is_object()) return {nullptr, nullptr, nullptr};
    std::vector<json> lists;
    if (swf == "LandingPage.swf") lists.push_back(jget(data, "Options"));
    const json& states = jget(data, "States");
    if (states.is_object())
        for (auto it = states.begin(); it != states.end(); ++it)
            if (it.value().is_object()) { const json& o = jget(it.value(), "options"); lists.push_back(o.is_array() ? o : jget(it.value(), "Entries")); }
    if (swf == "PracticePauseMenu.swf") {
        json ents = jget(populate_, "PracticePauseMenu.swf:entries");
        if (ents.is_object()) { json arr = json::array(); for (auto& v : ents) arr.push_back(v); ents = arr; }
        if (ents.is_array()) lists.push_back(ents);
    }
    for (const auto& lst : lists) {
        if (lst.is_array() && idx >= 0 && idx < (int)lst.size() && lst[idx].is_object()) {
            const json& e = lst[idx];
            json label = jtruthy(jget(e, "key")) ? jget(e, "key") : jget(e, "debugText");
            json desc = jtruthy(jget(e, "description")) ? jget(e, "description") : jget(e, "descKey");
            return {label, desc, (int)lst.size()};
        }
    }
    return {nullptr, nullptr, nullptr};
}

void Narrator::on_focus(const json& rec) {
    std::string swf = jstr(rec, "swf");
    if (jtruthy(jget(rec, "missing"))) return;
    std::string why = jstr(rec, "why");
    bool deferred = endsWith(why, "+");
    std::string base_why = why;
    while (!base_why.empty() && base_why.back() == '+') base_why.pop_back();
    int64_t t = jget(rec, "t").is_number() ? (int64_t)jget(rec, "t").get<double>() : 0;
    if (swf == "CharacterSelect.swf") {
        if (!deferred) on_charselect_focus(rec);
        return;
    }
    if (isNone(jget(rec, "index")) || swf == "StageSelect.swf" || swf == "CommandList.swf") return;
    const json& probe = jget(rec, "probe");
    if (swf == "ControllerConfig.swf") { on_controller_focus(rec, probe, deferred); return; }
    if (swf == "Popup.swf" && base_why == "inv:Populate" && !popup_spoken_) {
        // JSON fallback failed: read title and body from the screen itself
        std::vector<std::string> parts;
        for (const char* k : {"title", "body"}) { json v = clean(jget(probe, k)); if (!isNone(v)) parts.push_back(sv(v)); }
        if (!parts.empty()) { popup_spoken_ = true; say("Popup. " + join(parts, ". "), true, t); }
    }
    if (swf == "Popup.swf" && !(jget(probe, "hasButtons").is_boolean() && jget(probe, "hasButtons").get<bool>())) return;
    int idx = jint(jget(rec, "index"), -1);
    const json& groups = jget(rec, "groups");
    const json& mode = jget(rec, "mode");
    json label, desc, value;      // null = None
    json count, pos;              // null = None
    std::vector<std::string> flags;
    std::string prefix, gname;
    bool haveGname = false;
    if (swf == "OptionsMenu.swf") {
        auto se = options_entries();
        const json& st = se.first;
        const json& entries = se.second;
        std::string es = state_of(jget(groups, "entries"));
        const json& tg = jget(groups, "toggles");
        bool toggle;
        if (es == "focused") toggle = false;
        else if (es == "unfocused" && !isNone(clean(jget(tg, "label"))) && state_of(tg) != "unfocused") toggle = true;
        else toggle = mode.is_boolean() ? mode.get<bool>() : (st.is_object() && jstr(st, "toggle") == "Yes");
        const json& g = jget(groups, toggle ? "toggles" : "entries");
        label = clean(jget(g, "label"));
        desc = clean(jtruthy(jget(g, "desc")) ? jget(g, "desc") : jget(g, "desc2"));
        if (toggle) {
            bool shown = !(jget(g, "valueShown").is_boolean() && !jget(g, "valueShown").get<bool>());
            value = shown ? clean(jget(g, "value")) : clean(jget(g, "slider"));
            if (isNone(value)) value = clean(jget(g, "slider"));
        }
        if (entries.is_array() && !entries.empty()) {
            count = (int)entries.size();
            pos = idx;
            if (isNone(label) && idx >= 0 && idx < (int)entries.size()) {   // current state's entry, e.g. before the screen text is set
                const json& e = entries[idx];
                label = jtruthy(jget(e, "key")) ? jget(e, "key") : jget(e, "debugText");
                desc = jget(e, "descKey");
                if (toggle && isNone(value) && jget(e, "textValues").is_array()) {
                    const json& v = jget(e, "value");
                    if (jisInt(v) && v.get<int>() >= 0 && v.get<int>() < (int)jget(e, "textValues").size()) value = jget(e, "textValues")[v.get<int>()];
                    else if (!isNone(v)) value = jtostr(v);
                }
            }
        }
    } else if (groups.is_object() && !groups.empty()) {
        auto pg = pick_group(groups);
        gname = pg.first;
        haveGname = !gname.empty();
        const json& g = pg.second;
        label = clean(jget(g, "label"));
        desc = clean(jtruthy(jget(g, "desc")) ? jget(g, "desc") : jget(g, "desc2"));
        if (g.contains("value") || g.contains("slider")) {
            bool shown = !(jget(g, "valueShown").is_boolean() && !jget(g, "valueShown").get<bool>());
            value = shown ? clean(jget(g, "value")) : clean(jget(g, "slider"));
            if (isNone(value)) value = clean(jget(g, "slider"));
        }
        if (jisInt(jget(g, "index"))) idx = jget(g, "index").get<int>();
        if (jisInt(jget(g, "pos")) && jtruthy(jget(g, "count"))) { pos = jget(g, "pos"); count = jget(g, "count"); }
        if (jget(g, "locked").is_boolean() && jget(g, "locked").get<bool>()) flags.push_back("locked");
        if (jget(g, "done").is_boolean() && jget(g, "done").get<bool>()) flags.push_back("completed");
        if (swf == "MatchOutcome.swf" && (gname == "p2" || gname == "p2stats")) prefix = "Player 2: ";
    }
    if (swf == "MainMenu.swf") {
        json e = mm_entry(idx);
        if (e.is_object()) {   // the JSON is authoritative here: the on-screen text lags behind state changes
            json l = jtruthy(jget(e, "key")) ? jget(e, "key") : jget(e, "debugText");
            if (jtruthy(l)) label = l;
            if (jtruthy(jget(e, "description"))) desc = jget(e, "description");
            count = (int)mm_entries().size();
            pos = idx;
            gname = mm_state_.back();
            haveGname = true;
        }
    }
    if (isNone(label)) {   // JSON fallback: LandingPage Options[], MainMenu States, generic lists
        auto lf = label_from_json(swf, idx);
        label = std::get<0>(lf);
        desc = std::get<1>(lf);
        count = std::get<2>(lf);
        pos = idx;
    }
    if (swf == "Dojo.swf") {
        json title = clean(jget(probe, "title"));
        if (isNone(label) && isNone(title)) return;   // lesson grid not populated yet
        if (!isNone(label) && !isNone(title)) label = "Lesson " + sv(label) + ": " + S.resolve(title);
        else if (isNone(label)) label = title;
        if (isNone(desc) || !jtruthy(desc)) desc = clean(jget(probe, "desc"));
    }
    if (isNone(label) && (idx == -1 || (groups.is_object() && !groups.empty()))) return;   // no focused item yet
    if (isNone(label)) label = "item " + std::to_string(idx + 1);
    std::map<std::string, std::string> probe_new;
    for (const char* pk : {"category", "mode", "difficulty"}) {   // spoken once, when they change (committed only when we speak)
        json pv = clean(jget(probe, pk));
        if (!isNone(pv)) {
            std::string k = swf + "|" + pk, s = sv(pv);
            if (!last_probe_.count(k) || last_probe_[k] != s) {
                probe_new[k] = s;
                prefix += std::string(pk) != "difficulty" ? S.resolve(pv) + ". " : "Difficulty " + S.resolve(pv) + ". ";
            }
        }
    }
    std::string labelS = S.resolve(label);
    std::string descS = jtruthy(desc) ? S.resolve(desc) : "";
    bool haveValue = jtruthy(value);
    std::string valueS = haveValue ? S.resolve(value) : "";
    std::string stateKey = swf == "OptionsMenu.swf" ? options_state_.back() : (haveGname ? gname : "");
    std::vector<std::string> keyParts{swf, std::to_string(idx), labelS, haveValue ? valueS : "\x01None", stateKey};
    std::string key = keyOf(keyParts);
    last_focus_ = json{{"swf", swf}, {"index", idx}, {"label", labelS}, {"desc", descS}, {"value", haveValue ? json(valueS) : json(nullptr)}};
    if ((base_why == "PlayScrollLeftRight" || base_why == "PlayScrollLeftRight2") && !deferred && (swf == "OptionsMenu.swf" || swf == "PracticePauseMenu.swf"))
        return;   // the sound fires before the text changes; the deferred snapshot (or SetVariables) follows with the new value
    if ((base_why == "SetVariables" || base_why == "PlayNegative") && swf == "OptionsMenu.swf") {
        if (haveValue && !(deferred && hasLast_ && key == last_)) {
            say(valueS + (base_why == "PlayNegative" ? ", limit" : ""), true, t);
            last_ = key;
            hasLast_ = true;
        }
        return;
    }
    if (hasLast_ && key == last_) return;
    bool value_only = hasLast_ && haveValue && prefix.empty() && lastParts_.size() >= 3 &&
                      lastParts_[0] == keyParts[0] && lastParts_[1] == keyParts[1] && lastParts_[2] == keyParts[2];
    last_ = key;
    lastParts_ = keyParts;
    hasLast_ = true;
    for (auto& kv : probe_new) last_probe_[kv.first] = kv.second;
    if (value_only) {   // same item, new toggle value
        if (hasSkip_ && skip_value_once_ == valueS) hasSkip_ = false;   // already spoken from the SetVariables event
        else say(valueS, true, t);
        return;
    }
    std::string text = prefix + labelS;
    if (haveValue) text += ", " + valueS;
    if (!flags.empty()) text += ", " + join(flags, ", ");
    if (jtruthy(count) && !isNone(pos) && verbosity_ >= 1) text += ", " + std::to_string(jint(pos) + 1) + " of " + std::to_string(jint(count));
    if (!descS.empty() && verbosity_ >= 2) text += ". " + descS;
    say(text, true, t);
}

// ---- hotkeys ----
void Narrator::repeat() { say(last_text_.empty() ? "nothing to repeat" : last_text_, true); }
void Narrator::read_desc() {
    std::string d = jstr(last_focus_, "desc");
    if (d.empty() && current_swf_ == "CharacterSelect.swf") { read_appearance(); return; }
    say(d.empty() ? "no description" : d, true);
}
void Narrator::read_appearance() {
    if (current_swf_ != "CharacterSelect.swf") { say("appearance descriptions are available on the character select screen", true); return; }
    int side = cs_desc_side_;
    auto it = cs_fighter_code_.find(side);
    if (it == cs_fighter_code_.end() || it->second.empty()) { say("no fighter selected", true); return; }
    const std::string& code = it->second;
    const json& entry = jget(fighter_appearance_, code.c_str());
    std::string text, costume = "default";
    if (cs_stage_[side] != "fighter" && cs_costume_.count(side) && cs_costume_[side] == 1) costume = "retro";
    if (entry.is_string()) text = entry.get<std::string>();
    else if (entry.is_object()) {
        text = jstr(entry, costume.c_str());
        if (text.empty() && costume == "retro") { text = jstr(entry, "default"); if (!text.empty()) text += " No description of the retro costume yet."; }
    }
    if (text.empty()) text = "no appearance description for " + fighterName(code, code);
    say((side == 1 ? "Player 2: " : "") + text, true);
}
void Narrator::read_ticker() { say(!ticker_.empty() ? ticker_ : (!motd_.empty() ? motd_ : "no news"), true); }
void Narrator::cycle_verbosity() {
    verbosity_ = (verbosity_ + 1) % 3;
    say("verbosity " + std::to_string(verbosity_), true);
}

}  // namespace ki
