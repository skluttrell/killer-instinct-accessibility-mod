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
    std::string skip_value_once_;
    bool hasSkip_ = false;
    std::vector<std::string> mm_state_;
    json fighter_names_ = json::object();
    json fighter_appearance_ = json::object();   // code -> {"default": text, "retro": text} (data\fighter_appearance.json)
    std::wstring names_path_;

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
