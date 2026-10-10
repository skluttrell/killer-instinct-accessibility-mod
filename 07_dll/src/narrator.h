// The narration model: a line-for-line port of 06_narrator/narrator.py (class Narrator) onto nlohmann::json.
// Events arrive from the hooks on the UI thread (on_ei / on_inv / on_focus); hotkeys call repeat/read_desc/... from
// their own thread. All entry points must be called with ki::g_lock held.
#pragma once
#include "common.h"
#include "strings.h"
#include <map>
#include <set>

namespace ki {

class Narrator {
public:
    Narrator(Strings& strings);
    void loadData();                       // fighter names, cached Populate payloads (kiaccess\data)
    void say(const std::string& text, bool interrupt = true, int64_t tHook = 0);

    void on_ei(const std::string& script, const std::string& fn, const std::string& js, int64_t t);
    void on_inv(const std::string& swf, const std::string& fn, const std::string& js);
    void on_focus(const json& rec);

    // hotkeys
    void repeat();
    void read_desc();
    void read_appearance();                // Ctrl+Shift+A: physical appearance of the fighter on Character Select
    void read_ticker();
    void cycle_verbosity();

    int verbosity() const { return verbosity_; }
    void setVerbosity(int v) { verbosity_ = v; }
    std::string currentSwf() const { return current_swf_; }

private:
    Strings& S;
    int verbosity_ = 1;
    json populate_ = json::object();       // swf -> latest Populate JSON
    std::vector<std::string> options_state_{"Main"};
    std::string last_;                     // last spoken focus key ("" = none)
    std::vector<std::string> lastParts_;   // its (swf, index, label, value, state) parts
    bool hasLast_ = false;
    std::string last_text_;
    json last_focus_;                      // {swf,index,label,desc,value} or null
    std::string ticker_, motd_, current_swf_;
    std::map<int, std::string> cs_stage_{{0, "fighter"}, {1, "fighter"}};
    std::map<int, json> cs_colors_;
    std::map<int, std::string> cs_fighter_code_;   // side -> fighter code under the cursor / chosen (lower case)
    std::map<int, int> cs_costume_;                // side -> costume index on the costume stage and after
    int cs_desc_side_ = 0;                         // side the appearance hotkey describes: the one spoken about last
    std::map<std::string, std::string> cs_last_;   // "fighter|0" etc. -> text / key
    int cs_active_ = 0;
    std::map<std::string, std::string> last_probe_; // "swf|probe" -> text
    json cmdlist_ = json::object();
    bool cmd_group_spoken_ = false;
    bool popup_spoken_ = false;
    std::string last_popup_text_;          // popup de-duplication: the tutorial repopulates the same popup every frame
    int64_t last_popup_ms_ = 0;
    std::map<std::string, std::string> key_labels_{{"ABUTTON", "Enter"}, {"BBUTTON", "Escape"}, {"XBUTTON", "X"}, {"YBUTTON", "Y"}};   // learned from the popup legend
    std::string skip_value_once_;
    bool hasSkip_ = false;
    std::vector<std::string> mm_state_;
    json fighter_names_ = json::object();
    std::map<std::string, std::string> fighter_levels_;   // code -> "level 3, next unlock at level 5" (from the player card entries)
    json fighter_appearance_ = json::object();   // code -> {"default": text, "retro": text} (data\fighter_appearance.json)
    std::wstring names_path_;

    // Shadow Lords
    int preload_dialog_i_ = 0;             // next line of the versus-screen dialogue (dialogData index)
    std::string ga_announced_;             // GA screen whose ScreenShown summary was spoken last
    json sl_prompts_ = json::object();     // tutorial prompt state -> string keys (data\sl_prompts.json, from the GA_FTUE classes)
    json encounter_;                       // last War Room encounter popup payload (title, description, options / rewards / results)
    std::string barracks_pending_;         // centre fighter reported before the Barracks screen was shown (spoken with the screen name)
    json music_options_;                   // Stage Select music menu options (Lua_PopulateMusicOptions arrives before the screen's own Populate)
    std::string ga_summary(const std::string& swf);   // what to say after the screen name
    void on_ga_focus(const std::string& swf, const json& rec, const std::string& base_why, bool deferred, int64_t t);
    void on_preload_dialogue(int64_t t);
    void say_mission(int idx, bool interrupt, int64_t t);   // War Room mission list entry (MissionData[idx])

    void learn_fighter_names(const json& data);
    void say_popup(const json& data);
    void say_match_outcome(const json& data);
    void on_stage_changed(const std::string& js, int64_t t);
    void on_move_selected(const std::string& js, int64_t t);
    void on_controller_focus(const json& rec, const json& probe, bool deferred);
    json cs_fighter(const json& idx);
    std::string cs_fighter_text(const json& e);
    void on_charselect_ei(const std::string& fn, const std::string& js, int64_t t);
    void on_charselect_focus(const json& rec);
    json mm_entries();
    json mm_entry(const json& idx);
    std::pair<json, json> options_entries();   // (state, entries)
    void options_select();
    static std::string state_of(const json& g);
    std::pair<std::string, json> pick_group(const json& groups);
    std::tuple<json, json, json> label_from_json(const std::string& swf, int idx);   // (label, desc, count)
    std::string fighterName(const std::string& code, const std::string& def);
    std::string screenName(const std::string& swf);
};

std::string humanize_command(const json& cmd);
std::string icon_words(const json& name);

}  // namespace ki
