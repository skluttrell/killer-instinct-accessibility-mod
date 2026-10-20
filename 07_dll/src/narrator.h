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
    void on_locstring(const std::string& text);   // from the localization hook while g_locArmed (arcade ladder ending text)
    void on_match_live(int32_t levelId);   // from the radar thread (under g_lock): announce the stage when it was not chosen explicitly
    void on_loot(int ev, int count);       // from the radar thread (under g_lock): Shadow Lords loot-collect phase start (ev 0) / end (ev 1)

    // hotkeys
    void repeat();
    void review_step(int dir);             // Ctrl+Shift+Up / Down: step through the last announcement line by line
    void read_desc();
    void read_status();
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
    std::string review_src_;               // the announcement the review cursor was built from (beta report 2026-10-16 item 5)
    std::vector<std::string> review_lines_;
    int review_idx_ = -1;
    bool review_is_command_ = false;       // the review cursor holds a move's input tokens, not sentences (Command List)
    std::vector<std::string> cmd_tokens_;  // the focused move's input tokens ("down", "down-forward", "heavy punch", ...)
    std::string cmd_tokens_src_;           // the announcement those tokens belong to (ties them to the current move)
    json last_focus_;                      // {swf,index,label,desc,value} or null
    std::string ticker_, motd_, current_swf_;
    std::map<int, std::string> cs_stage_{{0, "fighter"}, {1, "fighter"}};
    std::map<int, json> cs_colors_;
    std::map<int, std::string> cs_fighter_code_;   // side -> fighter code under the cursor / chosen (lower case)
    std::map<int, int> cs_costume_;                // side -> costume index on the costume stage and after
    int cs_desc_side_ = 0;                         // side the appearance hotkey describes: the one spoken about last
    std::map<std::string, std::string> cs_last_;   // "fighter|0" etc. -> text / key
    int cs_active_ = 0;
    bool cc_hinted_[2] = {false, false};           // per side: the "press Up for Save/Default" hint has been spoken this visit to the controls panel
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
    std::string mp_screen_name_;   // MultiplayerExhibition entry name ("Exhibition" / "Shadow Lords MP"), spoken as a prefix on the first focus read
    std::string lobby_sub_;        // MultiplayerLobby active subscreen last read ("main"/"create"/"search"/"lobby"), to name it once on entry
    bool lobby_seen_ = false;      // LobbyInside: baseline of the UpdateLobbyInfo stream captured (joins/leaves/toggle announce only changes)
    int lobby_toggle_ = -1;        // LobbyInside: LocalPlayerToggled last seen (1 = queued to battle, 0 = spectating, -1 = no baseline)
    std::set<std::string> lobby_names_;   // LobbyInside: visible player names, to announce who joined or left
    std::string ladder_sub_;       // MultiplayerLadder active subscreen last read ("menu"/"lb"), to name the leaderboards once on entry
    std::string ladder_rank_said_; // card rank already announced this Ranked hub visit (one-shot)
    std::string ladder_cat_;       // leaderboard category text on screen (selects the column headers from the Populate payload)
    std::string ladder_prefix_;    // one-shot prefix ("Rank Gold. ") for the next generic menu read on the Ranked hub
    json fighter_names_ = json::object();
    std::map<std::string, std::string> fighter_levels_;   // code -> "level 3, next unlock at level 5" (from the player card entries)
    json fighter_appearance_ = json::object();   // code -> {"default": text, "retro": text} (data\fighter_appearance.json)
    json costume_colors_ = json::object();       // code -> {"default": {"2": text, ...}, "retro": {...}} (data\costume_colors.json, generated from the textures)
    std::map<int, int> cs_color_;                // side -> colour number (1-based) under the cursor / chosen on the colour stage
    std::map<int, std::string> cs_color_name_;   // side -> that entry's name ("Color 3", "Mimic Skin")
    std::wstring names_path_;

    // Shadow Lords
    int preload_dialog_i_ = 0;             // next line of the versus-screen dialogue (dialogData index)
    std::string ga_announced_;             // GA screen whose ScreenShown summary was spoken last
    std::string ga_summary_pending_;       // GA screen announced before its Populate arrived: summary owed
    json sl_prompts_ = json::object();     // tutorial prompt state -> string keys (data\sl_prompts.json, from the GA_FTUE classes)
    json encounter_;                       // last War Room encounter popup payload (title, description, options / rewards / results)
    int mission_idx_ = -1;                 // War Room mission under the cursor (AS_UpdateMissionSelection), re-read when the loadout popup closes
    std::string barracks_pending_;         // centre fighter reported before the Barracks screen was shown (spoken with the screen name)
    std::string barracks_roster_line_;     // the centre fighter's full roster line, re-announced when the artifact popup closes (beta report 2026-10-19)
    std::string barracks_roster_name_;     // that fighter's name alone, to restore last_focus_ after the popup closes
    json music_options_;                   // Stage Select music menu options (Lua_PopulateMusicOptions arrives before the screen's own Populate)
    std::string last_sound_;               // last PlaySoundString name: the Shadow Lords popups navigate in AS3 only, their sounds tell what moved
    json pack_items_;                      // Emporium: the cards of the pack bought last (PackPurchased payload), revealed one by one
    int pack_card_ = 0;                    // index of the card under the cursor in that reveal
    bool hub_daily_open_ = false;          // the hub's daily rewards overlay is on screen (it swallows the hub's keys)

    // Arcade ladder story text (Story mode S1/S2): the Rivals pre-fight text is engine-drawn over the loading
    // screen (keys from gameinfo\StoryModeS2Definition), the S1 endings are engine-drawn slides after the ladder
    json s2_story_ = json::object();   // data\s2_story.json: fighter code -> [{vs: opponent code, key: text key}] per match
    bool story_mode_ = false;          // inside the arcade ladder flow (SinglePlayerLadder and its fights)
    bool last_match_p1win_ = false;    // last MatchOutcome winner was Player 1 (with the final rung, arms the ending window)
    std::set<std::string> loc_spoken_; // engine strings spoken in the current ending window (dedup + cap)
    bool ladder_final_fight();         // the stored tower payload says the fight just played was the last rung

    // Store content pages (StoreContentOverview.swf: Fighters, Skins, Boosters, ...)
    json store_page_items_;                // current window of PopulateItems
    int store_page_min_ = 0;               // MinIndex of that window (GetItemsInCategory)
    std::string store_page_cat_;           // last announced category name

    // Fight Archive content pages (Stats, FightTitles, Leaderboards, ShadowLeaderboards, Replays)
    json lb_rows_;                         // Leaderboards: the visible window of ReceiveLeaderboardRecords (8 rows)
    int lb_top_ = 0;                       // its topIndex (global index of row 0)
    json lb_headers_;                      // resolved column header texts (PopulateColumnHeaders)
    bool titles_details_ = false;          // FightTitles: a category's challenge list is open (list spoken from its payload)
    int stats_cat_ = 0, stats_sub_ = 0;    // Stats details: category/sub of the last GetStatsByCategoryAndSubIndex request
    bool stats_details_ = false;           // the All Stats detail view is open (Y from the summary)
    bool replays_fav_ = false;             // Replays: the favorites filter is active (X); we mirror the game's filtered list
    void say_stats_summary(const json& data);                    // the Fight Stats landing page
    void say_stats_category(const std::string& fn, const json& data);   // one All Stats page (PopulateOverall, ...)
    json replays_list() const;             // the list the cursor moves in (all replays, or favorites only)
    json ladder_lb_cols();                 // Ranked Leagues leaderboards: resolved column headers of the category on screen
    std::string ga_summary(const std::string& swf);   // what to say after the screen name
    void on_ga_focus(const std::string& swf, const json& rec, const std::string& base_why, bool deferred, int64_t t);
    void on_preload_dialogue(int64_t t);
    void say_mission(int idx, bool interrupt, int64_t t);   // War Room mission list entry (MissionData[idx])

    void learn_fighter_names(const json& data);
    void say_popup(const json& data);
    void say_match_outcome(const json& data);
    void say_perf_result(const json& data);   // online performance test results (PerfResult_Popup.swf)
    void on_stage_changed(const std::string& js, int64_t t);
    void on_story_focus(const json& rec, int64_t t);
    void on_store_preview(const std::string& js, int64_t t);
    void on_move_selected(const std::string& js, int64_t t);
    void on_controller_focus(const json& rec, const json& probe, bool deferred);
    json cs_fighter(const json& idx);
    std::string cs_fighter_text(const json& e);
    std::string color_brief(const std::string& code, const std::string& costume, int num);
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
