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
    {"MultiplayerRanked.swf", "Ranked"}, {"MultiplayerExhibition.swf", "Exhibition"}, {"Story.swf", "Story"}, {"GA_HUB.swf", "Shadow Lords hub"},
    // Shadow Lords ("GA" = Gargos Assault) screens: shown/hidden by the mode itself, announced from their ScreenShown event
    {"GA_WarRoom.swf", "War Room"}, {"GA_Barracks.swf", "Barracks"}, {"GA_Emporium.swf", "Emporium"}, {"GA_SpiritLair.swf", "Spirit Lair"},
    {"GA_PreLoad.swf", "Versus screen"}, {"GA_MatchRewards.swf", "Match rewards"}, {"GA_Loadout_Popup.swf", "Loadout"}, {"GA_Archives.swf", "Archives"},
    {"GA_WrapUp.swf", "Wrap up"}, {"GA_Leaderboard.swf", "Shadow Lords leaderboard"}, {"GA_Daily_Rewards.swf", "Daily rewards"}, {"GA_FTUE.swf", "Tutorial"},
    {"GameSettingsPopUp.swf", "Game settings"},
};

// Input tokens the game replaces with the player's bound key when it renders a popup ("LIGHT_PUNCH" -> "[J]"); we read the attack name.
static const std::map<std::string, std::string> GLYPH_WORDS = {
    {"LIGHT_PUNCH", "light punch"}, {"MEDIUM_PUNCH", "medium punch"}, {"HEAVY_PUNCH", "heavy punch"},
    {"LIGHT_KICK", "light kick"}, {"MEDIUM_KICK", "medium kick"}, {"HEAVY_KICK", "heavy kick"},
    {"ANY_PUNCH", "any punch"}, {"ANY_KICK", "any kick"}, {"PUNCH_X_3", "triple punch"}, {"KICK_X_3", "triple kick"},
    {"PUNCH_X_2", "double punch"}, {"KICK_X_2", "double kick"}, {"INSTINCT", "instinct"}, {"START", "start"}, {"SELECT", "select"},
};

static std::string glyph_words(const std::string& text) {
    if (text.find('_') == std::string::npos) return text;
    static const std::regex re("\\b[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+\\b");
    std::string out;
    auto begin = std::sregex_iterator(text.begin(), text.end(), re), end = std::sregex_iterator();
    size_t last = 0;
    for (auto it = begin; it != end; ++it) {
        out += text.substr(last, it->position() - last);
        auto w = GLYPH_WORDS.find(it->str());
        out += w != GLYPH_WORDS.end() ? w->second : it->str();
        last = it->position() + it->length();
    }
    out += text.substr(last);
    return out;
}

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
    if (readFile(dataDir() + L"data\\sl_prompts.json", txt)) {
        json j = json::parse(txt, nullptr, false);
        if (j.is_object()) sl_prompts_ = j;
    } else logLine("warning: data\\sl_prompts.json missing, Shadow Lords tutorial prompts will not be read");
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
    static const std::regex reImg(R"(/(\w+)\.dds$)"), reTitle(R"(^&?(.+?) - Lvl\s*(\d+))"), reUnlock(R"(Lvl\s*(\d+))");
    for (const char* side : {"Player1Info", "Player2Info"}) {
        const json& entries = jget(jget(jget(data, side), "Expanded"), "Entries");
        if (!entries.is_array()) continue;
        for (const auto& e : entries) {
            std::string img = jstr(e, "CharacterImage"), title = jstr(e, "Title");
            std::smatch m, t;
            if (std::regex_search(img, m, reImg) && std::regex_search(title, t, reTitle)) {
                std::string code = lower(m[1].str());
                if (fighterName(code, "") != t[1].str()) { fighter_names_[code] = t[1].str(); changed = true; }
                // the fighter's level and next colour / accessory unlock (beta report 2026-10-10: levels were not read)
                std::string lvl = "level " + t[2].str(), nx = jstr(e, "NextUnlockLevel");
                std::smatch u;
                if (std::regex_search(nx, u, reUnlock)) lvl += ", next unlock at level " + u[1].str();
                fighter_levels_[code] = lvl;
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
    if (text.find("COMMAND_UI_") != std::string::npos) {   // <BIND>COMMAND_UI_SELECT</BIND> in tutorial prompts: the key behind the button
        const std::pair<const char*, std::string> binds[] = {{"COMMAND_UI_SELECT", key_labels_["ABUTTON"]}, {"COMMAND_UI_BACK", key_labels_["BBUTTON"]},
            {"COMMAND_UI_X", key_labels_["XBUTTON"]}, {"COMMAND_UI_Y", key_labels_["YBUTTON"]}, {"COMMAND_UI_START", "Space"},
            {"COMMAND_UI_LEFT", "Left"}, {"COMMAND_UI_RIGHT", "Right"}, {"COMMAND_UI_UP", "Up"}, {"COMMAND_UI_DOWN", "Down"},
            {"COMMAND_UI_TABPREV", "the previous-tab key"}, {"COMMAND_UI_TABNEXT", "the next-tab key"}, {"COMMAND_UI_LT", "the left-trigger key"}, {"COMMAND_UI_RT", "the right-trigger key"}};
        for (auto& b : binds) text = replaceAll(text, b.first, b.second);
    }
    text = glyph_words(text);
    std::string lat;
    if (tHook) lat = " [" + std::to_string(nowMs() - tHook) + " ms]";
    logLine("SAY" + lat + ": " + text);
    last_text_ = text;
    if (g_cfg.speech) speech::speak(text, interrupt);
}

// Payloads that nlohmann rejects: raw control characters inside strings (popup bodies carry newlines) and text that is
// not valid UTF-8 (Windows-1252 bytes such as curly quotes). Escape the former, transcode the latter.
static std::string repair_json(const std::string& js) {
    std::string out;
    out.reserve(js.size() + 32);
    bool inStr = false;
    for (size_t i = 0; i < js.size(); i++) {
        unsigned char c = (unsigned char)js[i];
        if (inStr) {
            if (c == '\\') {
                // Lua writes \' which JSON does not allow: keep valid escapes, drop the backslash of any other
                unsigned char nx = i + 1 < js.size() ? (unsigned char)js[i + 1] : 0;
                if (nx && strchr("\"\\/bfnrtu", nx)) { out += '\\'; out += (char)nx; i++; }
                continue;
            }
            if (c == '"') { inStr = false; out += (char)c; continue; }
            if (c < 0x20) {
                if (c == '\n') out += "\\n"; else if (c == '\r') out += "\\r"; else if (c == '\t') out += "\\t"; else out += ' ';
                continue;
            }
        } else if (c == '"') inStr = true;
        if (c < 0x80) { out += (char)c; continue; }
        // multi-byte: accept a well-formed UTF-8 sequence, otherwise treat the byte as Windows-1252 / Latin-1
        int len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
        bool ok = len > 0 && i + len <= js.size();
        for (int k = 1; ok && k < len; k++) if (((unsigned char)js[i + k] & 0xC0) != 0x80) ok = false;
        if (ok) { out.append(js, i, len); i += len - 1; continue; }
        static const unsigned short cp1252[32] = {0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D, 0x017D, 0x8F,
                                                 0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178};
        unsigned cp = c < 0xA0 ? cp1252[c - 0x80] : c;
        if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    return out;
}

// ---------------- events ----------------
void Narrator::on_inv(const std::string& swf, const std::string& fn, const std::string& js) {
    json data;
    if (!js.empty()) {
        data = json::parse(js, nullptr, false, true);
        if (data.is_discarded()) {
            data = json::parse(repair_json(js), nullptr, false, true);
            if (data.is_discarded() && (startsWith(fn, "Populate") || startsWith(fn, "Lua_Populate") || fn == "RefreshPage" || fn == "Refresh")) {
                std::string dumped;
                if (g_cfg.dumpPopulate) {
                    std::wstring p = dataDir() + L"data\\badjson_" + wide(replaceAll(swf, ".swf", "")) + L"_" + wide(fn) + L".txt";
                    writeFile(p, js);
                    dumped = ", raw payload in " + utf8(p);
                }
                logLine("json parse failed for " + swf + "." + fn + " (" + js.substr(0, 120) + ")" + dumped);
            }
        }
    }
    if (fn == "Populate" || fn == "Lua_Populate" || fn == "PopulateStatesOnly" || (fn == "Refresh" && startsWith(swf, "GA_"))) {
        if (data.is_array()) data = json{{"Modes", data}};   // Dojo sends an array of modes
        if (data.is_object()) {
            if (fn == "Refresh" && populate_[swf].is_object()) populate_[swf].update(data);   // Shadow Lords screens refresh parts of their state
            else populate_[swf] = data;
            data = populate_[swf];
            if (g_cfg.dumpPopulate) writeFile(dataDir() + L"data\\populate_" + wide(replaceAll(swf, ".swf", "")) + L".json", data.dump(1));
            if (swf == "OptionsMenu.swf") options_state_ = {jtruthy(jget(data, "GraphicsOptionOnly")) ? "Graphics" : "Main"};
            if (swf == "MainMenu.swf") { std::string st = jstr(data, "InitialState"); mm_state_ = {st.empty() ? "SinglePlayer" : st}; }
            if (swf == "StartScreen.swf") {
                // cold start: the first screen arrives without a LoadDestination, so announce it here (on-screen text: "PRESS MENU / SPACE")
                current_swf_ = swf;
                hasLast_ = false;
                say("Start screen. Press Menu or Space", true);
            }
            if (swf == "GA_PreLoad.swf") preload_dialog_i_ = 0;
            if (swf == "CharacterSelect.swf") {
                cs_stage_ = {{0, "fighter"}, {1, "fighter"}};
                cs_fighter_code_.clear();
                cs_costume_.clear();
                cs_desc_side_ = 0;
                cs_active_ = 0;
                cs_last_.clear();
            }
        }
        if (swf == "GA_FTUE.swf" && data.is_object()) {
            // Shadow Lords tutorial prompt: the overlay picks its text by state name (table from the decompiled FTUE classes)
            std::string state = jstr(jget(data, "promptData"), "state");
            const json& keys = jget(sl_prompts_, state.c_str());
            std::vector<std::string> parts;
            if (keys.is_array()) for (const auto& k : keys) { std::string s = S.resolve(k); if (!s.empty() && s != k.get<std::string>()) parts.push_back(s); }
            if (!parts.empty()) say("Tutorial. " + join(parts, " "), false);
            else if (!state.empty()) logLine("tutorial prompt without text: " + state);
        } else if (swf == "Popup.swf" || swf == "ShadowPopup.swf") {
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
    } else if (swf == "GA_HUB.swf" && fn == "LUA_ShowShadowMissionAnnouncementPopup") {
        // hub alert with fixed text (ShadowMissionAnnouncementPopup); Enter dismisses it
        std::vector<std::string> parts;
        for (const char* k : {"GA_HUB_SHADOWMISSIONANNOUNCEMENT_TITLE", "GA_HUB_SHADOWMISSIONANNOUNCEMENT_SUBTITLE", "GA_HUB_SHADOWMISSIONANNOUNCEMENT_BODY"}) {
            std::string s = S.resolve(std::string(k));
            if (!s.empty() && s != k) parts.push_back(s);
        }
        say("Popup. " + join(parts, ". ") + ". " + key_labels_["ABUTTON"] + ": Continue", true);
    } else if (swf == "StageSelect.swf" && fn == "Lua_PopulateMusicOptions" && data.is_object()) {
        // the music menu (Y on Stage Select): track list with the current choice; the focused track is read from the screen
        music_options_ = jget(data, "MusicOptions");   // kept apart: the screen's own Lua_Populate follows and would replace it
        last_probe_.erase(swf + "|music");
    } else if (swf == "GA_WarRoom.swf" && fn == "ShowDaysPassedPopup" && data.is_object()) {
        std::string d = S.resolve(jget(data, "currentDay"));
        if (!d.empty()) say(d, true);   // "TURN 3"
    } else if (swf == "GA_WarRoom.swf" && fn == "ShowEncountersPopup" && data.is_object()) {
        // War Room encounter popup: a deployment report ("rewards" / "results") or a choice ("selection", Up/Down + Enter)
        encounter_ = data;
        last_probe_.erase("GA_WarRoom.swf|enc");
        std::vector<std::string> parts;
        std::string title = S.resolve(jget(data, "title")), desc = S.resolve(jget(data, "description"));
        parts.push_back(title.empty() ? "Encounter" : title);
        const json& ch = jget(data, "characterData");
        std::string cn = S.resolve(jget(ch, "name")), chh = S.resolve(jget(ch, "healthDesc"));
        if (!cn.empty()) parts.push_back(cn + (chh.empty() ? "" : ", " + lower(chh)));
        if (!desc.empty()) parts.push_back(desc);
        std::vector<std::string> items;
        for (const char* lk : {"rewards", "results"}) {
            const json& list = jget(data, lk);
            if (!list.is_array()) continue;
            for (const auto& r : list) {
                std::string n = S.resolve(jget(r, "name")), q = S.resolve(jget(r, "quantity")), rt = S.resolve(jget(r, "rewardText"));
                if (!rt.empty()) items.push_back(rt);
                else if (!n.empty()) items.push_back(q.empty() || q == "1" ? n : q + " " + n);
            }
        }
        if (!items.empty()) parts.push_back("Rewards: " + join(items, ", "));
        const json& opts = jget(data, "options");
        if (opts.is_array() && !opts.empty()) {
            std::vector<std::string> os;
            for (size_t i = 0; i < opts.size(); i++) os.push_back(std::to_string(i + 1) + ": " + S.resolve(jget(opts[i], "optionText")));
            parts.push_back(std::to_string(opts.size()) + (opts.size() == 1 ? " choice. " : " choices. ") + join(os, ". "));
        }
        say(join(parts, ". "), true);
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
    std::vector<std::string> parts{"Match results"}, stats;
    const json& w = jget(data, "Winner");
    if (jisInt(w) && (w.get<int>() == 0 || w.get<int>() == 1)) parts.push_back("Player " + std::to_string(w.get<int>() + 1) + " wins");
    for (int side = 0; side < 2; side++) {
        std::string pk = "Player" + std::to_string(side + 1) + "State", sk = "p" + std::to_string(side + 1) + "ShowStats";
        const json& st = jget(data, pk.c_str());
        if (!jtruthy(jget(data, sk.c_str())) || !st.is_object()) continue;
        std::string streak = strip(S.resolve(jget(st, "WinStreak")));
        if (!streak.empty()) parts.push_back(streak);
        std::string xp = strip(S.resolve(jget(st, "TotalEarnedXP")));
        if (!xp.empty() && xp != "0") parts.push_back(xp + " XP");
        // the five stat groups (Hero, Offense, Defense, Combos, Style) with their metrics: spoken at verbosity 1 and 2
        // (beta report 2026-10-10 asked for them), and kept for Ctrl+Shift+D at every verbosity
        for (const char* cat : {"Hero", "Offense", "Defense", "Combos", "Variety"}) {
            const json& c = jget(st, cat);
            if (!c.is_object()) continue;
            std::vector<std::string> ms;
            const json& metrics = jget(c, "metrics");
            if (metrics.is_array()) for (const auto& m : metrics) if (m.is_object()) ms.push_back(S.resolve(jget(m, "key")) + " " + S.resolve(jget(m, "stat")));
            if (!ms.empty()) { std::string ck = S.resolve(jget(c, "key")); stats.push_back((ck.empty() ? std::string(cat) : ck) + ": " + join(ms, ", ")); }
        }
    }
    last_focus_ = json{{"swf", "MatchOutcome.swf"}, {"label", parts.size() > 1 ? parts[1] : parts[0]}, {"desc", join(stats, ". ")}};
    if (verbosity_ >= 1) for (auto& s : stats) parts.push_back(s);
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
    for (const char* k : {"ABUTTON", "BBUTTON", "XBUTTON", "YBUTTON"}) {
        const json& b = jget(data, k);
        if (b.is_object() && jtruthy(jget(b, "visible")) && jtruthy(jget(b, "text"))) buttons.push_back(key_labels_[k] + ": " + S.resolve(jget(b, "text")));
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
    // the Shadow Lords tutorial repopulates the same lesson popup every frame for about two seconds: speak it once
    int64_t now = nowMs();
    bool repeat = text == last_popup_text_ && now - last_popup_ms_ < 3000;
    last_popup_text_ = text;
    last_popup_ms_ = now;
    if (!repeat) say(text, true);
}

void Narrator::on_ei(const std::string& script, const std::string& fn, const std::string& js, int64_t t) {
    if (fn == "LoadDestination") {
        json j = json::parse(js, nullptr, false, true);
        std::string dest = jstr(j, "Destination");
        if (!dest.empty()) {
            current_swf_ = dest;
            hasLast_ = false;
            if (!startsWith(dest, "GA_")) ga_announced_.clear();
            say(screenName(dest), true, t);
        }
    } else if (script == "GameSettingsPopUp" && fn == "ScreenShown" && current_swf_ != "GameSettingsPopUp.swf") {
        // the game settings popup (Y on Character Select) has no LoadDestination of its own and stays loaded afterwards
        current_swf_ = "GameSettingsPopUp.swf";
        hasLast_ = false;
        say("Game settings", true, t);
    } else if (fn == "ScreenShown" && startsWith(script, "GA_") && script != "GA_FTUE") {
        // Shadow Lords loads all of its screens at once and switches between them itself (no LoadDestination)
        std::string swf = script + ".swf";
        if (swf != ga_announced_) {
            // the first GA screen also arrives through LoadDestination (name already spoken, payload not yet there):
            // speak the summary alone in that case, name + summary when the mode switched screens by itself
            bool named = swf == current_swf_;
            ga_announced_ = swf;
            current_swf_ = swf;
            hasLast_ = false;
            last_probe_.erase(swf + "|tab");   // re-announce the hub's focused tab each time the hub appears
            last_probe_.erase(swf + "|focus");
            std::string s = ga_summary(swf);
            if (named) { if (s.size() > 2) say(s.substr(2), false, t); }
            else say(screenName(swf) + s, true, t);
        }
    } else if (script == "GA_Barracks" && fn == "AS_ClearAllNotificationsForFighter") {
        // the Barracks rotates its three fighter modules and tells Lua which roster member is now in the centre
        json j = json::parse(js, nullptr, false, true);
        int i = jint(jget(j, "rosterIndex"), -1);
        const json& chars = jget(jget(populate_, "GA_Barracks.swf"), "characterData");
        if (chars.is_array() && i >= 0 && i < (int)chars.size()) {
            const json& c = chars[i];
            std::vector<std::string> parts{S.resolve(jget(c, "name"))};
            std::string hp = S.resolve(jget(c, "healthDesc"));
            if (!hp.empty()) parts.push_back(lower(hp));
            if (verbosity_ >= 1) parts.push_back(std::to_string(i + 1) + " of " + std::to_string(chars.size()));
            if (jtruthy(jget(c, "isPrimary"))) parts.push_back("captain");
            std::string loc = strip(S.resolve(jget(c, "location"))), days = strip(S.resolve(jget(c, "daysRemaining")));
            if (!loc.empty()) parts.push_back(loc);
            if (!days.empty()) parts.push_back(days);
            std::string recd = strip(S.resolve(jget(c, "record")));
            if (jtruthy(jget(c, "showRecord")) && !recd.empty()) parts.push_back(recd);
            last_focus_ = json{{"swf", "GA_Barracks.swf"}, {"label", parts[0]}, {"desc", ""}};
            if (current_swf_ != "GA_Barracks.swf" || ga_announced_ != "GA_Barracks.swf") barracks_pending_ = join(parts, ", ");   // arrives just before ScreenShown
            else say(join(parts, ", "), true, t);
        }
    } else if (script == "GA_WarRoom" && fn == "AS_UpdateMissionSelection") {
        // the mission list reports every cursor move (and its initial position) to Lua
        json j = json::parse(js, nullptr, false, true);
        say_mission(jint(jget(j, "Index"), -1), current_swf_ == "GA_WarRoom.swf" && ga_announced_ == "GA_WarRoom.swf" && hasLast_, t);
        hasLast_ = true;
    } else if (fn == "PlaySoundString" && current_swf_ == "GA_PreLoad.swf") {
        json j = json::parse(js, nullptr, false, true);
        if (startsWith(jstr(j, "Sound"), "Play_SL_PreLoad_FlavorText_")) on_preload_dialogue(t);
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

// ---- stage of the match that just loaded ----
// The level object the GetLevelId Lua binding reads holds CRC32 of the level path (lower case, as in the Stage Select
// payload: "levels\stage_11_maya\stage_11_maya" -> -476706460, measured 2026-10-10). The table below is the Stage
// Select list of that day; a Stage Select payload seen in this session takes precedence.
static const std::pair<const char*, const char*> STAGE_LEVELS[] = {
    {"levels\\stage_01_jago\\stage_01_jago", "STAGE_TIGERSLAIR"}, {"levels\\stage_02_glacius\\stage_02_glacius", "STAGE_CRASHSITE"},
    {"levels\\stage_03_sabrewulf\\stage_03_sabrewulf", "STAGE_ALCHEMICAL_LAB"}, {"levels\\stage_04_thunder\\stage_04_thunder", "STAGE_DEVILSLANDING"},
    {"levels\\stage_05_sadira\\stage_05_sadira", "STAGE_ASSASINSCAVE"}, {"levels\\stage_06_orchid\\stage_06_orchid", "STAGE_REBELOUTPOST"},
    {"levels\\stage_07_spinal\\stage_07_spinal", "STAGE_SHIPWRECKSHORE"}, {"levels\\stage_08_fulgore\\stage_08_fulgore", "STAGE_ULTRATECHINDUSTRIES"},
    {"levels\\stage_09_shadow_jago\\stage_09_shadow_jago", "STAGE_SHADOWTIGERLAIR"}, {"levels\\stage_10_tjcombo\\stage_10_tjcombo", "STAGE_DOWNTOWNDEMOLITION"},
    {"levels\\stage_11_maya\\stage_11_maya", "STAGE_MAYA"}, {"levels\\stage_12_gauze\\stage_12_gauze", "STAGE_GAUZE"},
    {"levels\\stage_13_riptor\\stage_13_riptor", "STAGE_RIPTOR"}, {"levels\\stage_14_aganos\\stage_14_aganos", "STAGE_AGANOS"},
    {"levels\\stage_15_hisako\\stage_15_hisako", "STAGE_HISAKO"}, {"levels\\stage_16_cinder\\stage_16_cinder", "STAGE_CINDER"},
    {"levels\\stage_17_aria\\stage_17_aria", "STAGE_ARIA"}, {"levels\\stage_18_judge\\stage_18_judge", "STAGE_JUDGE"},
    {"levels\\stage_19_kimwu\\stage_19_kimwu", "STAGE_KIMWU"}, {"levels\\stage_20_tusk\\stage_20_tusk", "STAGE_TUSK"},
    {"levels\\stage_21_astrofield\\stage_21_astrofield", "STAGE_ASTROFIELD"}, {"levels\\stage_00\\stage_00", "STAGE_TRAINING"},
    {"levels\\stage_00_black\\stage_00_black", "STAGE_BLACK"},
};

void Narrator::on_match_live(int32_t levelId) {
    if (!levelId) return;
    uint32_t want = (uint32_t)levelId;
    std::string key;
    const json& stages = jget(jget(populate_, "StageSelect.swf"), "Stages");
    if (stages.is_array()) for (const auto& s : stages) {
        std::string lvl = jstr(s, "level");
        if (!lvl.empty() && Strings::crc32(lower(lvl)) == want) { key = jstr(s, "name"); break; }
    }
    if (key.empty()) for (auto& p : STAGE_LEVELS) if (Strings::crc32(p.first) == want) { key = p.second; break; }
    if (key.empty()) { logLine("stage: unknown level id " + std::to_string(levelId)); return; }
    std::string name = S.resolve(key);
    if (name.empty() || name == key) return;
    // say it unless Stage Select already announced this very stage (Random, Shadow Lords and ladders never did)
    std::string chosen = lower(last_probe_["StageSelect.swf|stage"]);
    if (!chosen.empty() && startsWith(chosen, lower(name).c_str())) return;
    say("Stage: " + name, false);
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
    if (!desc.empty() && desc != name && verbosity_ >= 1) text += ". " + desc;   // ender types, instinct effects (beta report 2026-10-10)
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
    else if (verbosity_ >= 1) { auto lv = fighter_levels_.find(lower(name)); if (lv != fighter_levels_.end()) text += ", " + lv->second; }
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
    if (swf == "GameSettingsPopUp.swf" && current_swf_ != swf) return;   // the popup's movie stays loaded after it closes
    if (swf == "StageSelect.swf") {
        // the stage itself is event-driven (StageSelectionChanged); only the music menu (Y) is read from the screen
        const json& mg = jget(jget(rec, "groups"), "music");
        if (!mg.is_object() || !jget(mg, "index").is_number()) return;
        json lab = clean(jget(mg, "label"));
        if (isNone(lab)) return;
        std::string text = sv(lab);
        // position from the option list, not from the 15 on-screen slots (most of them are empty but "visible")
        const json& opts = music_options_;
        std::string pos;
        auto norm = [](std::string s) { std::string o; for (char ch : s) if (isalnum((unsigned char)ch)) o += (char)toupper((unsigned char)ch); return o; };
        if (opts.is_array()) for (size_t i = 0; i < opts.size(); i++) {
            const json& o = opts[i];
            // the option carries a string key ("STAGE_DEFAULT_THEME"); compare it, resolved or not, with the on-screen text
            std::string key = jstr(o, "name"), res = S.resolve(jget(o, "name"));
            std::string k2 = norm(startsWith(key, "STAGE_") ? key.substr(6) : key);
            if (norm(res) != norm(text) && k2 != norm(text)) continue;
            if (jtruthy(jget(o, "isSelected"))) text += ", current";
            if (jget(o, "isEnabled").is_boolean() && !jget(o, "isEnabled").get<bool>()) text += ", locked";
            pos = std::to_string(i + 1) + " of " + std::to_string(opts.size());
            break;
        }
        if (verbosity_ >= 1 && !pos.empty()) text += ", " + pos;
        std::string mk = swf + "|music";
        if (last_probe_[mk] == text) return;
        bool first = last_probe_.find(mk) == last_probe_.end();
        last_probe_[mk] = text;
        last_focus_ = json{{"swf", swf}, {"label", sv(lab)}, {"desc", ""}};
        say((first ? "Music. " : "") + text, true, t);
        return;
    }
    if (isNone(jget(rec, "index")) || swf == "CommandList.swf") return;
    const json& probe = jget(rec, "probe");
    if (swf == "ControllerConfig.swf") { on_controller_focus(rec, probe, deferred); return; }
    if (startsWith(swf, "GA_")) { on_ga_focus(swf, rec, base_why, deferred, t); return; }
    if (swf == "Popup.swf") {   // the legend shows the keyboard key behind each gamepad button ("Enter", "Tab"): remember it
        const std::pair<const char*, const char*> keyProbes[] = {{"keyA", "ABUTTON"}, {"keyB", "BBUTTON"}, {"keyX", "XBUTTON"}, {"keyY", "YBUTTON"}};
        for (auto& kp : keyProbes) {
            const json& v = jget(probe, kp.first);
            if (v.is_string() && !strip(v.get<std::string>()).empty()) key_labels_[kp.second] = strip(v.get<std::string>());
        }
    }
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
// ---------------- Shadow Lords ----------------
// The mode's screens keep their state in the Populate payload and navigate inside ActionScript; the only engine traffic
// is the mode's sound events (PlaySoundString) and a few AS_* calls, so the readers trigger on those and read the
// display objects on the next frame (see 04_notes/research_log_2026-10-09b.md).

static const json& combatant(const json& side) {
    static const json none;
    const json& list = jget(side, "combatants");
    int i = jint(jget(side, "currentCombatantIndex"));
    if (!list.is_array() || i < 0 || i >= (int)list.size()) return none;
    return list[i];
}

std::string Narrator::ga_summary(const std::string& swf) {
    const json& data = jget(populate_, swf.c_str());
    if (!data.is_object()) return "";
    std::vector<std::string> parts;
    if (swf == "GA_PreLoad.swf") {
        // versus screen: "JAGO versus OMEN. Astral Plane. Health 100%"
        const json& p = combatant(jget(data, "playerData"));
        const json& e = combatant(jget(data, "enemyData"));
        std::string pn = S.resolve(jget(p, "name")), en = S.resolve(jget(e, "name"));
        if (jstr(data, "missionState") == "PostMission") {
            const json& won = jget(data, "playerWon");
            parts.push_back(won.is_boolean() ? (won.get<bool>() ? "Mission won" : "Mission lost") : "Mission over");
        } else if (!pn.empty() || !en.empty()) {
            std::string vs = pn + " versus " + en;
            if (jtruthy(jget(e, "isShadowCharacter"))) vs += " (shadow)";
            if (jtruthy(jget(e, "corrupted"))) vs += " (corrupted)";
            parts.push_back(vs);
        }
        std::string arena = S.resolve(jget(jget(data, "arenaData"), "name"));
        if (!arena.empty()) parts.push_back(arena);
        std::string ph = S.resolve(jget(p, "healthDesc")), eh = S.resolve(jget(e, "healthDesc"));
        if (!ph.empty() && ph == eh) parts.push_back(ph);
        else { if (!ph.empty()) parts.push_back("your " + lower(ph)); if (!eh.empty()) parts.push_back("enemy " + lower(eh)); }
        const json& ft = jget(data, "flavorText");
        if (jtruthy(jget(ft, "visible"))) { std::string m = S.resolve(jget(ft, "string")); if (!m.empty()) parts.push_back(m); }
        if (jtruthy(jget(data, "teamMission"))) parts.push_back("team mission");
    } else if (swf == "GA_WarRoom.swf") {
        const json& pt = jget(data, "PlaythroughData");
        if (pt.is_object()) {
            int turn = jint(jget(pt, "TurnCount"), -1), w = jint(jget(pt, "Wins"), -1), l = jint(jget(pt, "Losses"), -1);
            if (turn >= 0) parts.push_back("Turn " + std::to_string(turn));
            if (w >= 0 && l >= 0) parts.push_back(std::to_string(w) + (w == 1 ? " win, " : " wins, ") + std::to_string(l) + (l == 1 ? " loss" : " losses"));
        }
        const json& ms = jget(data, "MissionData");
        if (ms.is_array()) parts.push_back(std::to_string(ms.size()) + (ms.size() == 1 ? " mission" : " missions"));
        const json& th = jget(data, "threatData");
        if (verbosity_ >= 2 && th.is_array()) {
            std::vector<std::string> regions;
            for (const auto& r : th) { int c = jint(jget(r, "corruptionLevel"), -1); if (c > 0) regions.push_back(S.resolve(jget(r, "name")) + " corruption " + std::to_string(c)); }
            if (!regions.empty()) parts.push_back(join(regions, ", "));
        }
    } else if (swf == "GA_Barracks.swf") {
        if (!barracks_pending_.empty()) { parts.push_back(barracks_pending_); barracks_pending_.clear(); }
    } else if (swf == "GA_SpiritLair.swf") {
        int ae = jint(jget(data, "astralEnergyInInventory"), -1);
        if (ae >= 0) parts.push_back(std::to_string(ae) + " astral energy");
        const json& pets = jget(data, "spiritPetData");
        if (pets.is_array()) {
            int owned = 0;
            for (const auto& p : pets) { const json& inv = jget(p, "inventory"); if (inv.is_array()) owned += (int)inv.size(); }
            parts.push_back(std::to_string(owned) + (owned == 1 ? " guardian owned" : " guardians owned"));
        }
    } else if (swf == "GA_MatchRewards.swf") {
        const json& ok = jget(data, "missionSucces");   // sic, the game's spelling
        if (ok.is_boolean()) parts.push_back(ok.get<bool>() ? "Mission complete" : "Mission failed");
        const json& p = combatant(jget(data, "playerData"));
        std::string pn = S.resolve(jget(p, "name")), ph = S.resolve(jget(p, "healthDesc"));
        if (!pn.empty() && !ph.empty()) parts.push_back(pn + " " + lower(ph));
        std::vector<std::string> items;
        const json& rw = jget(jget(data, "rewardsData"), "rewards");
        if (rw.is_array()) for (const auto& r : rw) {
            std::string n = S.resolve(jget(r, "name")), q = S.resolve(jget(r, "quantity"));
            if (n.empty()) continue;
            items.push_back(q.empty() || q == "1" ? n : q + " " + n);
        }
        const json& sp = jget(jget(data, "rewardsData"), "specialReward");
        if (sp.is_array()) for (const auto& r : sp) { std::string n = S.resolve(jget(r, "name")); if (!n.empty()) items.push_back(n + " (special)"); }
        if (!items.empty()) parts.push_back("Rewards: " + join(items, ", "));
    } else if (swf == "GA_Loadout_Popup.swf") {
        parts.push_back("Select loadout");
        const json& chars = jget(data, "characterData");
        if (chars.is_array()) {
            std::vector<std::string> names;
            for (const auto& c : chars) {
                std::string n = S.resolve(jget(c, "name"));
                if (n.empty()) continue;
                int h = jint(jget(c, "health"), -1);
                if (h >= 0) n += " health " + std::to_string(h) + "%";
                if (!jtruthy(jget(c, "canPlayMission"))) n += " (unavailable)";
                if (jtruthy(jget(c, "isAssignedToMission"))) n += " (assigned)";
                names.push_back(n);
            }
            if (!names.empty()) parts.push_back("Fighters: " + join(names, ", "));
        }
    } else if (swf == "GA_HUB.swf") {
        const json& timer = jget(data, "timerData");
        if (timer.is_object()) {
            int days = jint(jget(timer, "daysPassed"), -1), left = jint(jget(timer, "daysTillGargos"), -1);
            if (days >= 0) parts.push_back("Day " + std::to_string(days));
            if (left > 0) parts.push_back(std::to_string(left) + (left == 1 ? " day" : " days") + " until Gargos");
        }
        const json& cur = jget(data, "currency");
        if (cur.is_object()) {
            std::string gold = S.resolve(jget(cur, "kiGoldOwned")), tok = S.resolve(jget(cur, "tokensOwned"));
            if (!gold.empty()) parts.push_back(gold + " KI gold");
            if (!tok.empty()) parts.push_back(tok + " tokens");
        }
    }
    return parts.empty() ? "" : ". " + join(parts, ". ");
}

void Narrator::on_ga_focus(const std::string& swf, const json& rec, const std::string& base_why, bool deferred, int64_t t) {
    (void)deferred;
    const json& probe = jget(rec, "probe");
    if (swf == "GA_Loadout_Popup.swf") {
        // slots (fighter / consumable / guardian sub-buttons), the launch button, or the fighter picker when it is open
        const json& groups = jget(rec, "groups");
        const json& data = jget(populate_, swf.c_str());
        std::string text, key;
        const json& fg = jget(groups, "fighters");
        const json& sg = jget(groups, "slots");
        const json& lg = jget(groups, "launch");
        if (fg.is_object() && !jtruthy(jget(fg, "hidden")) && jget(fg, "index").is_number()) {
            int i = jint(jget(fg, "index"));
            const json& chars = jget(data, "characterData");
            std::vector<std::string> parts;
            if (chars.is_array() && i >= 0 && i < (int)chars.size()) {
                const json& c = chars[i];
                parts.push_back(S.resolve(jget(c, "name")));
                int h = jint(jget(c, "health"), -1);
                if (h >= 0) parts.push_back("health " + std::to_string(h) + "%");
                if (!jtruthy(jget(c, "canPlayMission"))) parts.push_back("unavailable");
                if (jtruthy(jget(c, "isAssignedToMission"))) parts.push_back("assigned");
            } else parts.push_back("fighter " + std::to_string(i + 1));
            if (verbosity_ >= 1) parts.push_back(std::to_string(jint(jget(fg, "pos")) + 1) + " of " + std::to_string(jint(jget(fg, "count"))));
            text = join(parts, ", ");
            key = "fighter|" + std::to_string(i);
        } else if (sg.is_object() && jget(sg, "index").is_number()) {
            int i = jint(jget(sg, "index"));
            std::string sub = jstr(sg, "sub");
            std::vector<std::string> parts;
            json add = clean(jget(sg, "add")), cons = clean(jget(sg, "consumable")), pet = clean(jget(sg, "pet"));
            std::string slot = jint(jget(sg, "count")) > 1 ? "Slot " + std::to_string(jint(jget(sg, "pos")) + 1) + ", " : "";
            // an empty slot shows its generic title ("Consumable", "Guardian"): say "empty" instead
            if (sub == "HLConsumable") parts.push_back(slot + "consumable: " + (isNone(cons) || lower(sv(cons)) == "consumable" ? "empty" : sv(cons)));
            else if (sub == "HLSpiritPet") parts.push_back(slot + "guardian: " + (isNone(pet) || lower(sv(pet)) == "guardian" ? "empty" : sv(pet)));
            else {
                std::string who;
                const json& chars = jget(data, "characterData");
                if (chars.is_array()) for (const auto& c : chars) if (jtruthy(jget(c, "isAssignedToMission"))) { who = S.resolve(jget(c, "name")); break; }
                if (!who.empty()) parts.push_back(slot + "fighter: " + who);
                else parts.push_back(slot + (isNone(add) ? std::string("Add fighter") : sv(add)));
            }
            text = join(parts, ", ");
            key = "slot|" + std::to_string(i) + "|" + sub + "|" + text;
        } else if (lg.is_object() && jget(lg, "index").is_number()) {
            json lab = clean(jget(lg, "label"));
            text = isNone(lab) ? "Launch mission" : sv(lab);
            key = "launch";
        }
        if (text.empty()) return;
        std::string lk = swf + "|focus";
        auto it = last_probe_.find(lk);
        if (it != last_probe_.end() && it->second == key) return;
        last_probe_[lk] = key;
        last_focus_ = json{{"swf", swf}, {"label", text}, {"desc", ""}};
        say(text, base_why != "ScreenShown", t);
        return;
    }
    if (swf == "GA_SpiritLair.swf") {
        // a row of guardian decks (one per guardian type, spiritPetData[i]); Left/Right change deck, Up/Down the card in it
        // (the popups' .visible flags are always true, so they are not used as gates)
        const json& dg = jget(jget(rec, "groups"), "decks");
        if (!dg.is_object() || !jget(dg, "index").is_number()) return;
        int i = jint(jget(dg, "index"));
        const json& pets = jget(jget(populate_, swf.c_str()), "spiritPetData");
        std::vector<std::string> parts;
        std::string name, desc;
        if (pets.is_array() && i >= 0 && i < (int)pets.size()) {
            const json& p = pets[i];
            name = S.resolve(jget(p, "name"));
            parts.push_back(name);
            std::string type = S.resolve(jget(p, "desc"));
            if (!type.empty()) parts.push_back(lower(type));
            const json& inv = jget(p, "inventory");
            if (inv.is_array()) parts.push_back(inv.empty() ? "none owned" : std::to_string(inv.size()) + " owned");
            desc = S.resolve(jget(p, "instructions"));
        } else parts.push_back("guardian " + std::to_string(i + 1));
        if (verbosity_ >= 1) parts.push_back(std::to_string(jint(jget(dg, "pos")) + 1) + " of " + std::to_string(jint(jget(dg, "count"))));
        int card = jint(jget(probe, "cardIdx"), -1);
        if (card > 0) parts.push_back("card " + std::to_string(card + 1));
        if (verbosity_ >= 2 && !desc.empty()) parts.push_back(desc);
        std::string key = std::to_string(i) + "|" + std::to_string(card) + "|" + name;
        std::string fk = swf + "|focus";
        if (last_probe_[fk] == key) return;
        last_probe_[fk] = key;
        last_focus_ = json{{"swf", swf}, {"label", name}, {"desc", desc}};
        say(join(parts, ", "), base_why != "ScreenShown", t);
        return;
    }
    if (swf == "GA_WarRoom.swf") {
        // only the encounter popup's choice list is read here; the mission list reports its cursor through AS_UpdateMissionSelection
        if (!jtruthy(jget(probe, "encounterShown")) || !encounter_.is_object()) return;
        const json& opts = jget(encounter_, "options");
        int idx = jint(jget(probe, "encIdx"), -1);
        if (!opts.is_array() || idx < 0 || idx >= (int)opts.size()) return;
        std::string text = S.resolve(jget(opts[idx], "optionText"));
        if (text.empty()) return;
        std::string key = std::to_string(idx) + "|" + text;
        std::string fk = swf + "|enc";
        if (last_probe_[fk] == key) return;
        bool first = last_probe_.find(fk) == last_probe_.end();
        last_probe_[fk] = key;
        last_focus_ = json{{"swf", swf}, {"label", text}, {"desc", S.resolve(jget(encounter_, "description"))}};
        say(text + (verbosity_ >= 1 ? ", " + std::to_string(idx + 1) + " of " + std::to_string(opts.size()) : ""), !first, t);
        return;
    }
    if (swf == "GA_Emporium.swf") {
        // four tabs (Packs, KI Gold, Craft, Storage), each a carousel; the item lists arrive in the Populate/Refresh payload
        // (the sub-screens' and popups' .visible flags are always true: they hide through frame labels, so only the
        // tab strip's "Active" label tells which screen is up)
        int tab = -1;
        for (int i = 0; i < 4; i++) if (jstr(probe, ("tab" + std::to_string(i)).c_str()) == "Active") { tab = i; break; }
        if (tab < 0) return;
        json tabText = clean(jget(probe, ("tab" + std::to_string(tab) + "Text").c_str()));
        std::string tabName = isNone(tabText) ? std::string(tab == 0 ? "Packs" : tab == 1 ? "KI Gold" : tab == 2 ? "Craft" : "Storage") : sv(tabText);
        const json& tabs = jget(jget(populate_, swf.c_str()), "navigationTabData");
        const json& items = tabs.is_array() && tab < (int)tabs.size() ? jget(tabs[tab], "data") : json();
        const char* idxKey[] = {"packIdx", "goldIdx", nullptr, nullptr};
        int idx = idxKey[tab] ? jint(jget(probe, idxKey[tab]), -1) : -1;
        if (tab == 2) {   // the craft screen's getter does not match its "1/6" counter: use the counter
            json pos = clean(jget(probe, "craftPos"));
            if (!isNone(pos)) idx = atoi(sv(pos).c_str()) - 1;
        }
        std::string name, desc, extra;
        json title = clean(jget(probe, tab == 2 ? "craftTitle" : tab == 3 ? "storageTitle" : "none"));
        if (!isNone(title)) name = sv(title);
        if (items.is_array() && idx >= 0 && idx < (int)items.size()) {
            const json& it = items[idx];
            if (name.empty()) name = S.resolve(jget(it, "name"));
            std::vector<std::string> costs;
            std::string tc = S.resolve(jget(it, "tokenCost")), gc = S.resolve(jget(it, "kiGoldCost")), q = S.resolve(jget(it, "quantity"));
            if (!tc.empty() && tc != "0") costs.push_back(tc + " gems");
            if (!gc.empty() && gc != "0") costs.push_back(gc + " KI gold");
            if (!costs.empty()) extra = join(costs, " or ");
            if (!q.empty()) extra += (extra.empty() ? "" : ", ") + std::string("quantity ") + q;
            const json& cc = jget(it, "canCraft");
            if (cc.is_boolean()) extra += (extra.empty() ? "" : ", ") + std::string(cc.get<bool>() ? "can craft" : "missing materials");
            for (const char* dk : {"description", "description1", "description2"}) { std::string d = S.resolve(jget(it, dk)); if (!d.empty()) desc += (desc.empty() ? "" : " ") + d; }
        }
        if (desc.empty()) {
            const char* dks[] = {"packDesc", "goldDesc", "craftDesc", "storageDesc"};
            json d = clean(jget(probe, dks[tab])); if (!isNone(d)) desc = sv(d);
            json d2 = clean(jget(probe, tab == 2 ? "craftDesc2" : tab == 3 ? "storageDesc2" : "none")); if (!isNone(d2)) desc += (desc.empty() ? "" : " ") + sv(d2);
        }
        std::vector<std::string> parts;
        std::string key = tabName + "|" + std::to_string(idx) + "|" + name;
        std::string tk = swf + "|tab";
        bool tabChanged = last_probe_[tk] != tabName;
        if (tabChanged) { parts.push_back(tabName + " tab, " + std::to_string(tab + 1) + " of 4"); last_probe_[tk] = tabName; }
        if (!name.empty()) {
            parts.push_back(name);
            json pos = clean(jget(probe, "craftPos"));
            if (verbosity_ >= 1) {
                if (items.is_array() && idx >= 0) parts.push_back(std::to_string(idx + 1) + " of " + std::to_string(items.size()));
                else if (tab == 2 && !isNone(pos)) parts.push_back(replaceAll(sv(pos), "/", " of "));
            }
            if (!extra.empty()) parts.push_back(extra);
            if (verbosity_ >= 2 && !desc.empty()) parts.push_back(desc);
        }
        std::string fk = swf + "|focus";
        if (parts.empty() || (!tabChanged && last_probe_[fk] == key)) return;
        if (name.empty() && !tabChanged) return;   // the item text has not been written yet: wait for the next snapshot
        last_probe_[fk] = key;
        last_focus_ = json{{"swf", swf}, {"label", name}, {"desc", desc}};
        say(join(parts, ", "), base_why != "ScreenShown", t);
        return;
    }
    if (swf == "GA_HUB.swf") {
        // tab bar: the focused tab is always slot 3; Left/Right scroll the data under it (sound Play_SL_Global_Toggle)
        json tj = clean(jget(probe, "tab"));
        if (isNone(tj)) return;
        std::string title = sv(tj);
        std::string key = swf + "|tab";
        auto it = last_probe_.find(key);
        if (it != last_probe_.end() && it->second == title) return;
        last_probe_[key] = title;
        std::vector<std::string> parts{title};
        const json& tabs = jget(jget(populate_, swf.c_str()), "navigationTabData");
        if (tabs.is_array()) {
            for (size_t i = 0; i < tabs.size(); i++) {
                if (lower(S.resolve(jget(tabs[i], "key"))) != lower(title)) continue;
                if (verbosity_ >= 1) parts.push_back(std::to_string(i + 1) + " of " + std::to_string(tabs.size()));
                int n = jint(jget(tabs[i], "notifications"));   // the on-screen badge keeps its old number when hidden
                if (n > 0) parts.push_back(std::to_string(n) + (n == 1 ? " notification" : " notifications"));
                break;
            }
        }
        json det = clean(jget(probe, "tabDetails"));
        if (verbosity_ >= 2 && !isNone(det)) parts.push_back(sv(det));
        last_focus_ = json{{"swf", swf}, {"label", title}, {"desc", isNone(det) ? "" : sv(det)}};
        say(join(parts, ", "), base_why != "ScreenShown", t);   // after the screen name, queue; on a scroll, interrupt
    }
}

void Narrator::say_mission(int idx, bool interrupt, int64_t t) {
    const json& ms = jget(jget(populate_, "GA_WarRoom.swf"), "MissionData");
    if (!ms.is_array() || idx < 0 || idx >= (int)ms.size()) return;
    const json& m = ms[idx];
    std::vector<std::string> parts;
    std::string name = S.resolve(jget(m, "key"));
    if (!name.empty()) parts.push_back(name);
    std::string loc = S.resolve(jget(jget(m, "location"), "name"));
    if (!loc.empty()) parts.push_back(loc);
    std::string diff = S.resolve(jget(m, "difficulty"));
    if (!diff.empty() && diff != jstr(m, "difficulty")) parts.push_back(diff);
    if (verbosity_ >= 1) parts.push_back(std::to_string(idx + 1) + " of " + std::to_string(ms.size()));
    if (jtruthy(jget(m, "teamMission"))) parts.push_back("team mission");
    std::string days = strip(S.resolve(jget(m, "days")));
    if (!days.empty()) parts.push_back(days);
    const json& opp = jget(m, "opponents");
    if (opp.is_array() && opp.size() > 1) parts.push_back(std::to_string(opp.size()) + " enemies");
    if (jtruthy(jget(m, "canDeployToOnly"))) parts.push_back("deploy only");
    std::vector<std::string> rewards;
    const json& rd = jget(m, "rewardData");
    if (rd.is_array()) for (const auto& r : rd) { std::string s = S.resolve(jget(r, "text")); if (!s.empty()) rewards.push_back(s); }
    std::string desc = S.resolve(jget(m, "summary"));
    if (!rewards.empty()) desc += (desc.empty() ? "" : " ") + std::string("Rewards: ") + join(rewards, ", ");
    if (verbosity_ >= 2 && !desc.empty()) parts.push_back(desc);
    last_focus_ = json{{"swf", "GA_WarRoom.swf"}, {"label", name}, {"desc", desc}};
    say(join(parts, ", "), interrupt, t);
}

void Narrator::on_preload_dialogue(int64_t t) {
    const json& data = jget(populate_, "GA_PreLoad.swf");
    const json& lines = jget(data, "dialogData");
    if (!lines.is_array() || preload_dialog_i_ >= (int)lines.size()) return;
    const json& line = lines[preload_dialog_i_++];
    int who = jint(jget(line, "player"), 0);
    std::string speaker = S.resolve(jget(combatant(jget(data, who == 0 ? "playerData" : "enemyData")), "name"));
    std::string text = S.resolve(jget(line, "text"));
    if (text.empty()) return;
    say((speaker.empty() ? std::string(who == 0 ? "You" : "Enemy") : speaker) + ": " + text, true, t);
}

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
