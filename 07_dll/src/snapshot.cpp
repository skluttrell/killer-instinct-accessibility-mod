#include "snapshot.h"
#include "gfx.h"

namespace ki {
namespace snap {

// Per-screen readers: where the focused item's text lives (display-object paths from the decompiled AS3).
// 'state' = the button's current timeline label: GainFocus/FocusedLoop(/RemappingLoop/Step*) when it is the live cursor.
// index mode: the screen's public getter (indexFn, default GetSelectionIndex) + the item at that index;
// scan mode: every item's state label, the focused one reported with its position among visible, labelled items.
static const char* SCREENS_JSON = R"JSON({
 "MainMenu.swf": {"groups": {"main": {"item": "mcMenuGroup.mcMenuBtn{i}.mcBtn",
    "fields": {"label": "mcTxt.txtButton.text", "desc": "mcDescription.mcDescriptionContainer.txtDescription.text", "state": "currentLabel"}}}},
 "OptionsMenu.swf": {"groups": {
    "entries": {"item": "mcMenuGroup.mcOptionsMain.mcBtn{i}", "fields": {"label": "mcTxtLarge.txt.text", "desc": "mcTextDesc.mcTextHolder.txtDescription.text", "state": "currentLabel"}},
    "toggles": {"item": "mcMenuGroup.mcToggleMenu.mcBtn{i}", "fields": {"label": "mcTxtLarge.txt.text", "desc": "mcTextDesc.mcTextHolder.txtDescription.text",
       "desc2": "mcTextDesc.txtDescription.text", "value": "mcValueLarge.txt.text", "valueShown": "mcValueLarge.visible", "slider": "mcSliderValue.txt.text", "state": "currentLabel"}}}},
 "CharacterSelect.swf": {"custom": "charselect"},
 "PauseMenu.swf": {"scan": true, "groups": {"main": {"item": "mcMenuGroup.mcBtn{i}", "count": 7,
    "fields": {"label": "mcTxtLarge.txtButton.text", "state": "currentLabel", "visible": "visible"}}}},
 "PracticePauseMenu.swf": {"scan": true, "probe": {"category": "mcMenuGroup.mcToggle.mcFilterTxt.txtFilter.text"}, "groups": {
    "main": {"when": "mcMenuGroup.mcPauseOptionsMain.visible", "item": "mcMenuGroup.mcPauseOptionsMain.mcBtn{i}", "count": 8,
       "fields": {"label": "mcTxtLarge.txt.text", "state": "currentLabel", "visible": "visible"}},
    "toggles": {"when": "mcMenuGroup.mcToggleMenu.visible", "item": "mcMenuGroup.mcToggleMenu.mcBtn{i}", "count": 12,
       "fields": {"label": "mcTxtLarge.txt.text", "value": "mcValueLarge.txt.text", "valueShown": "mcValueLarge.visible", "slider": "mcSliderValue.txt.text",
          "desc": "mcTextDesc.mcTextHolder.txtDescription.text", "desc2": "mcTextDesc.txtDescription.text", "state": "currentLabel", "visible": "visible"}},
    "themes": {"when": "mcMenuGroup.mcThemesMenu.visible", "item": "mcMenuGroup.mcThemesMenu.mcScrollList.mcBtn{i}", "count": 8,
       "fields": {"label": "mcTxtLarge.txt.text", "state": "currentLabel", "visible": "visible"}}}},
 "Trials.swf": {"scan": true, "probe": {"difficulty": "mcMenuGroup.mcDifficulty.txtItem.text"}, "groups": {"main": {"item": "mcMenuGroup.mcBtn{i}", "count": 4,
    "fields": {"label": "mcTxtLarge.txt.text", "desc": "mcTextDesc.txtDescription.text", "state": "currentLabel", "visible": "visible"}}}},
 "Dojo.swf": {"probe": {"mode": "mcDojo.mcCategoryToggle.txtDojoMode.text", "title": "mcDojo.mcDisplayContent.mcTitle.txtDojoLevelName.text",
    "desc": "mcDojo.mcDisplayContent.mcDescription.txtDescription.text"},
    "groups": {"rows": {"item": "mcDojo.mcDojoRow{r}.mcEntry{e}", "rowSize": 8,
       "fields": {"label": "mcTxt.txtDojoLevelName.text", "state": "currentLabel", "locked": "mcLocked.visible", "done": "mcCompleted.visible"}}}},
 "Popup.swf": {"indexFn": "GetSelectedDisplayButton", "probe": {"title": "Title.text", "body": "Body.text", "hasButtons": "ButtonContainer.visible"},
    "groups": {"display": {"item": "DISPLAY{i}", "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel", "visible": "visible"}}}},
 "StoreMain.swf": {"scan": true, "groups": {"items": {"items": ["mcStoreMain.mcItem0", "mcStoreMain.mcItem1_1", "mcStoreMain.mcItem2_1", "mcStoreMain.mcItem3_1",
    "mcStoreMain.mcItem4_1", "mcStoreMain.mcItem5_1", "mcStoreMain.mcItem6_1", "mcStoreMain.mcItem1", "mcStoreMain.mcItem2", "mcStoreMain.mcItem3",
    "mcStoreMain.mcItem4", "mcStoreMain.mcItem5", "mcStoreMain.mcItem6", "mcStoreMain.mcItem7"],
    "fields": {"label": "mcTitle.txtItem.text", "state": "currentLabel", "visible": "visible"}}}},
 "MatchOutcome.swf": {"scan": true, "groups": {
    "p1": {"item": "mcP1RematchOptions.MenuOption_0{i}", "first": 1, "count": 4, "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel", "visible": "visible"}},
    "p2": {"item": "mcP2RematchOptions.MenuOption_0{i}", "first": 1, "count": 4, "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel", "visible": "visible"}},
    "p1stats": {"items": ["mcP1Stats.mcOffense", "mcP1Stats.mcDefense", "mcP1Stats.mcCombos", "mcP1Stats.mcVariety"], "fields": {"state": "currentLabel", "visible": "visible"}},
    "p2stats": {"items": ["mcP2Stats.mcOffense", "mcP2Stats.mcDefense", "mcP2Stats.mcCombos", "mcP2Stats.mcVariety"], "fields": {"state": "currentLabel", "visible": "visible"}}}},
 "ControllerConfig.swf": {"scan": true, "probe": {"p1start": "mcMenuGroup.mcStartPanelP1.visible", "p2start": "mcMenuGroup.mcStartPanelP2.visible",
    "p1panel": "mcMenuGroup.mcControlsPanelP1.visible", "p2panel": "mcMenuGroup.mcControlsPanelP2.visible",
    "p1prompt": "mcMenuGroup.mcStartPanelP1.mcPressStart.txtStart.text"},
    "groups": {
    "p1": {"when": "mcMenuGroup.mcControlsPanelP1.visible", "item": "mcMenuGroup.mcControlsPanelP1.mcButtonGroup.mcBtn{i}", "count": 15,
       "fields": {"state": "currentLabel", "visible": "visible", "text": "mcTxt.TxtItem.text", "value": "mcValue.TxtItem.text", "warn": "mcCmdIconGroup.mcMessageText.txt.text", "warnState": "mcCmdIconGroup.currentLabel"}},
    "p1exit": {"when": "mcMenuGroup.mcControlsPanelP1.visible", "items": ["mcMenuGroup.mcControlsPanelP1.mcButtonLeft", "mcMenuGroup.mcControlsPanelP1.mcButtonRight"],
       "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel"}},
    "p2": {"when": "mcMenuGroup.mcControlsPanelP2.visible", "item": "mcMenuGroup.mcControlsPanelP2.mcButtonGroup.mcBtn{i}", "count": 15,
       "fields": {"state": "currentLabel", "visible": "visible", "text": "mcTxt.TxtItem.text", "value": "mcValue.TxtItem.text", "warn": "mcCmdIconGroup.mcMessageText.txt.text", "warnState": "mcCmdIconGroup.currentLabel"}},
    "p2exit": {"when": "mcMenuGroup.mcControlsPanelP2.visible", "items": ["mcMenuGroup.mcControlsPanelP2.mcButtonLeft", "mcMenuGroup.mcControlsPanelP2.mcButtonRight"],
       "fields": {"label": "mcTxt.txtButton.text", "state": "currentLabel"}}}},
 "CommandList.swf": {"indexFn": "GetSelectionIndex"},
 "StageSelect.swf": {"custom": "none"},
 "MultiplayerLobby.swf": {"indexFn": "GetSelectionIndex"}
})JSON";

static json SCREENS;
static std::string s_lastGood;
static bool s_inSnapshot = false;

void init() { SCREENS = json::parse(SCREENS_JSON); }
bool hasScreen(const std::string& swf) { return SCREENS.contains(swf); }
std::string lastGoodSwf() { return s_lastGood; }

static bool isFocusedLabel(const json& s) {
    if (!s.is_string()) return false;
    const std::string& v = s.get_ref<const std::string&>();
    if (v == "RemappingLoop" || startsWith(v, "Step")) return true;   // a toggle only steps while it has the cursor
    return v.find("Focus") != std::string::npos && !startsWith(v, "Lose") && !startsWith(v, "Unfocused");
}

static std::vector<std::string> itemPaths(const json& gc) {
    std::vector<std::string> out;
    const json& items = jget(gc, "items");
    if (items.is_array()) { for (const auto& p : items) out.push_back(p.get<std::string>()); return out; }
    int first = jint(jget(gc, "first")), count = jint(jget(gc, "count"));
    std::string tmpl = jstr(gc, "item");
    for (int i = first; i < first + count; i++) out.push_back(replaceAll(tmpl, "{i}", std::to_string(i)));
    return out;
}

static json readFields(const gfx::GValue& ref, const std::string& item, const json& fields, const json& skip) {
    json f = json::object();
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (skip.is_object() && skip.contains(it.key())) { f[it.key()] = skip[it.key()]; continue; }
        json v = gfx::readPath(ref, item + "." + it.value().get<std::string>());
        if (!v.is_discarded()) f[it.key()] = v;
    }
    return f;
}

static void snapshotCharSelect(const gfx::GValue& ref, json& rec) {
    json cs = json::object();
    gfx::GValue fs;
    if (gfx::memberObj(ref, "mcFighterSelect", fs)) {
        json a = gfx::invoke(fs, "GetFocusedFighterIndex", {0}), b = gfx::invoke(fs, "GetFocusedFighterIndex", {1});
        if (!a.is_discarded()) cs["f0"] = a;
        if (!b.is_discarded()) cs["f1"] = b;
        gfx::release(fs);
    }
    for (int side = 1; side <= 2; side++) {
        gfx::GValue co, cl;
        std::string s = std::to_string(side - 1);
        if (gfx::memberObj(ref, gfx::cstr("mcP" + std::to_string(side) + "CostumeSelect"), co)) {
            json c = gfx::invoke(co, "GetSelectedCostumeIndex", {}), slot = gfx::invoke(co, "GetSelectedCustomSlot", {});
            if (!c.is_discarded()) cs["c" + s] = c;
            if (!slot.is_discarded()) cs["slot" + s] = slot;
            gfx::release(co);
        }
        if (gfx::memberObj(ref, gfx::cstr("mcP" + std::to_string(side) + "ColorSelect"), cl)) {
            json c = gfx::invoke(cl, "GetSelectedColorBackendIndex", {});
            if (!c.is_discarded()) cs["col" + s] = c;
            gfx::release(cl);
        }
    }
    rec["cs"] = cs;
}

static void snapshotScan(const gfx::GValue& ref, const json& cfg, json& rec) {
    rec["index"] = -1;
    json groups = json::object();
    const json& gs = jget(cfg, "groups");
    for (auto it = gs.begin(); it != gs.end(); ++it) {
        const json& gc = it.value();
        std::string when = jstr(gc, "when");
        if (!when.empty()) {
            json w = gfx::readPath(ref, when);
            if (w.is_boolean() && !w.get<bool>()) { groups[it.key()] = json{{"count", 0}, {"hidden", true}}; continue; }   // whole menu hidden
        }
        const json& fields = jget(gc, "fields");
        std::string stateField = jstr(fields, "state"), visField = jstr(fields, "visible"), labelField = jstr(fields, "label");
        std::vector<std::string> paths = itemPaths(gc);
        json focused;
        int pos = -1, visible = 0, first = jint(jget(gc, "first"));
        for (size_t i = 0; i < paths.size(); i++) {
            json st = gfx::readPath(ref, paths[i] + "." + stateField);
            bool vis = true;
            if (!visField.empty()) { json v = gfx::readPath(ref, paths[i] + "." + visField); vis = !(v.is_boolean() && !v.get<bool>()); }
            if (vis && !labelField.empty()) {   // an unused button can stay visible with empty text: do not count it
                json lb = gfx::readPath(ref, paths[i] + "." + labelField);
                if (lb.is_discarded() || (lb.is_string() && (lb.get<std::string>().empty() || lb.get<std::string>() == "missing text"))) vis = false;
                else if (isFocusedLabel(st) && focused.is_null()) focused = readFields(ref, paths[i], fields, json{{"state", st}, {"visible", true}, {"label", lb}});
            } else if (vis && isFocusedLabel(st) && focused.is_null()) focused = readFields(ref, paths[i], fields, json{{"state", st}, {"visible", true}});
            if (vis) visible++;
            if (!focused.is_null() && pos < 0) { focused["index"] = first + (int)i; pos = visible - 1; }
        }
        json g = focused.is_null() ? json::object() : focused;
        if (!focused.is_null()) g["pos"] = pos;
        g["count"] = visible;
        groups[it.key()] = g;
    }
    rec["groups"] = groups;
}

json snapshot(const std::string& swfIn, const std::string& why) {
    json rec = json::object();
    if (!gfx::g_root || s_inSnapshot) return rec;
    s_inSnapshot = true;
    std::string swf = swfIn;
    int64_t t0 = nowMs();
    gfx::GValue ref;
    bool ok = gfx::getRef(swf, ref);
    if (!ok && !s_lastGood.empty() && s_lastGood != swf) {
        // a shared component called a script of a movie that is not loaded (e.g. the practice menu's toggles call
        // OptionsMenu.SetVariables): the cursor is still on the last screen we could read
        if (gfx::getRef(s_lastGood, ref)) { swf = s_lastGood; ok = true; }
    }
    if (!ok) {
        s_inSnapshot = false;
        rec = json{{"ev", "focus"}, {"swf", swf}, {"why", why}, {"missing", true}};
        return rec;
    }
    s_lastGood = swf;
    rec = json{{"ev", "focus"}, {"swf", swf}, {"why", why}, {"t", t0}};
    const json& cfg = jget(SCREENS, swf.c_str());
    const json& probe = jget(cfg, "probe");
    if (probe.is_object()) {
        json p = json::object();
        for (auto it = probe.begin(); it != probe.end(); ++it) { json v = gfx::readPath(ref, it.value().get<std::string>()); if (!v.is_discarded()) p[it.key()] = v; }
        rec["probe"] = p;
    }
    std::string custom = jstr(cfg, "custom");
    if (custom == "charselect") { snapshotCharSelect(ref, rec); rec["index"] = -1; }
    else if (custom == "none") rec["index"] = -1;
    else if (jtruthy(jget(cfg, "scan"))) snapshotScan(ref, cfg, rec);
    else {
        std::string indexFn = jstr(cfg, "indexFn");
        json idx = gfx::invoke(ref, indexFn.empty() ? "GetSelectionIndex" : indexFn.c_str(), {});
        if (idx.is_number()) rec["index"] = (int)idx.get<double>(); else rec["index"] = nullptr;
        const json& gs = jget(cfg, "groups");
        if (gs.is_object() && rec["index"].is_number()) {
            int i = rec["index"].get<int>();
            std::string modePath = jstr(cfg, "mode");
            if (!modePath.empty()) { json m = gfx::readPath(ref, modePath); if (m.is_boolean()) rec["mode"] = m; }
            json groups = json::object();
            for (auto it = gs.begin(); it != gs.end(); ++it) {
                const json& gc = it.value();
                std::string item = jstr(gc, "item");
                int rowSize = jint(jget(gc, "rowSize"));
                if (rowSize) item = replaceAll(replaceAll(item, "{r}", std::to_string(i / rowSize)), "{e}", std::to_string(i % rowSize));
                else item = replaceAll(item, "{i}", std::to_string(i));
                groups[it.key()] = readFields(ref, item, jget(gc, "fields"), json());
            }
            rec["groups"] = groups;
        }
    }
    gfx::release(ref);
    rec["ms"] = nowMs() - t0;
    s_inSnapshot = false;
    return rec;
}

}  // namespace snap
}  // namespace ki
