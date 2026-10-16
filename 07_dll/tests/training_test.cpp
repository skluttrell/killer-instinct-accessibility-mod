// Synthetic native-memory fixtures and narration sequences. No game required.
#include "../src/training.h"
#include "../src/narrator.h"
#include "../src/speech.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <limits>

HMODULE g_thisModule = nullptr;
static std::vector<std::string> spoken;
static std::vector<bool> interrupts;
namespace ki { namespace speech {
void speak(const std::string& text, bool interrupt) { spoken.push_back(text); interrupts.push_back(interrupt); }
} }

static void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class T> static void put(uint8_t* bytes, size_t offset, T value) { memcpy(bytes + offset, &value, sizeof value); }
static void nativeString(uint8_t* bytes, const std::string& text) {
    memset(bytes, 0, 0x28);
    put(bytes, 0x10, text.size());
    put(bytes, 0x18, text.size() < 16 ? size_t(15) : text.size());
    if (text.size() < 16) memcpy(bytes, text.data(), text.size());
    else put(bytes, 0, (uintptr_t)text.c_str());
}
static void nativeEntry(uint8_t* bytes, const std::string& text, int target, int count, uint8_t complete = 0) {
    memset(bytes, 0, 0x60);
    nativeString(bytes, text);
    put(bytes, 0x50, target); put(bytes, 0x54, count); put(bytes, 0x5c, complete);
}
static json entry(const char* text, int target = 2, int count = 0, bool complete = false) {
    return json{{"text", text}, {"target", target}, {"count", count}, {"complete", complete}};
}
static void clearSpeech() { spoken.clear(); interrupts.clear(); }
static bool contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
static void testNativeMemory() {
    using namespace ki;
    using training::Event;
    std::array<uint8_t, 0x980> manager{};
    std::array<uint8_t, 0xc0> commands{};
    const std::string walk = "Walk forward", jump = "Jump - <TT>UP</TT>", objective = "Movement practice";
    nativeEntry(commands.data(), walk, 2, 1);
    nativeEntry(commands.data() + 0x60, jump, 2, 0, 1);
    nativeEntry(manager.data() + 0x900, objective, 3, 7);
    put(manager.data(), 0x8e0, (uintptr_t)commands.data());
    put(manager.data(), 0x8e8, (uintptr_t)(commands.data() + commands.size()));
    json data;
    check(training::snapshot((uintptr_t)manager.data(), Event::Commands, data), "native command snapshot");
    check(data.size() == 2 && data[0]["text"] == walk && data[1]["text"] == jump, "inline and heap strings");
    check(data[0]["count"] == 1 && data[1]["complete"] == true, "numeric and explicit completion");
    check(training::snapshot((uintptr_t)manager.data(), Event::Objective, data) && data["count"] == 3 && data["complete"] == true, "objective clamps completed count");
    uintptr_t global = (uintptr_t)manager.data();
    check(training::snapshotGlobal((uintptr_t)&global, Event::Objective, data), "manager global");
    check(training::snapshot(0, Event::ClearObjective, data) && data.is_null(), "clear never reads stale objective");
    std::array<uint8_t, 0x20> script{};
    uint32_t eventHash = Strings::crc32("updatetrainingcommands");
    put(script.data(), 0x18, Strings::crc32("hud"));
    check(training::identify((uintptr_t)script.data(), (uintptr_t)&eventHash) == Event::Commands, "retail hash-only event keys");
    put(script.data(), 0x18, Strings::crc32("another_script"));
    check(training::identify((uintptr_t)script.data(), (uintptr_t)&eventHash) == Event::None, "other scripts ignored");
    check(training::identify(1, 1) == Event::None && !training::snapshot(1, Event::Objective, data), "invalid pointers guarded");
    put(manager.data(), 0x8e8, (uintptr_t)commands.data() - 1);
    check(!training::snapshot((uintptr_t)manager.data(), Event::Commands, data), "reversed vector rejected");
    put(manager.data(), 0x8e8, (uintptr_t)commands.data() + 1);
    check(!training::snapshot((uintptr_t)manager.data(), Event::Commands, data), "misaligned vector rejected");
    put(manager.data(), 0x8e8, (uintptr_t)commands.data() + 129 * 0x60);
    check(!training::snapshot((uintptr_t)manager.data(), Event::Commands, data), "unbounded vector rejected");
    put(manager.data(), 0x8e8, (uintptr_t)commands.data() + 0x60);
    put(commands.data(), 0x10, size_t(5000));
    check(!training::snapshot((uintptr_t)manager.data(), Event::Commands, data), "oversize string rejected");
    const std::string unicode = u8"D\u00e9placez-vous", invalidUtf8("\xc3", 1);
    nativeEntry(commands.data(), unicode, 2, 0);
    check(training::snapshot((uintptr_t)manager.data(), Event::Commands, data) && data[0]["text"] == unicode, "localized UTF-8 preserved");
    nativeEntry(commands.data(), invalidUtf8, 2, 0);
    check(!training::snapshot((uintptr_t)manager.data(), Event::Commands, data), "invalid UTF-8 rejected before event logging");
    nativeEntry(commands.data(), walk, 2, -1);
    check(!training::snapshot((uintptr_t)manager.data(), Event::Commands, data), "invalid count rejected");
    put(manager.data(), 0x8e0, uintptr_t(0)); put(manager.data(), 0x8e8, uintptr_t(0));
    check(training::snapshot((uintptr_t)manager.data(), Event::Commands, data) && data.empty(), "empty vector clears commands");
    std::array<uint8_t, 48> getter{};
    getter[0] = 0x48; getter[1] = 0x8b; getter[2] = 0x05;
    put(getter.data(), 3, int32_t(25));
    check(training::managerGlobal((uintptr_t)getter.data()) == (uintptr_t)getter.data() + 32, "RIP-relative global resolution");
    getter[0] = 0;
    check(!training::managerGlobal((uintptr_t)getter.data()), "unexpected getter rejected");
}

static void testNarration() {
    using namespace ki;
    Strings strings;
    Narrator narrator(strings);
    json commands = json::array({entry("Walk - <TT>FORWARD</TT>"), entry("Jump - <TT>UP</TT>")});
    clearSpeech();
    narrator.on_training("UpdateTrainingObjective", entry("Movement", 0), 1000);
    narrator.on_training("UpdateTrainingCommands", commands, 1010);
    narrator.flush_training(1049);
    check(spoken.empty(), "updates wait to coalesce");
    narrator.flush_training(1060);
    check(spoken.size() == 1 && contains(spoken[0], "Objective: Movement") && contains(spoken[0], "Walk - forward, 0 of 2") &&
          contains(spoken[0], "Jump - up, 0 of 2") && !interrupts[0], "one queued goal batch with direction words");
    narrator.on_training("UpdateTrainingCommands", commands, 1070);
    narrator.flush_training(2000);
    check(spoken.size() == 1, "identical frame update silent");
    clearSpeech();
    commands[0]["count"] = 1;
    narrator.on_training("UpdateTrainingCommands", commands, 2100);
    narrator.flush_training(2150);
    check(spoken.size() == 1 && contains(spoken[0], "1 of 2") && !contains(spoken[0], "Jump"), "only changed goal spoken");
    clearSpeech();
    commands[1]["complete"] = true;
    narrator.on_training("UpdateTrainingCommands", commands, 2200);
    narrator.flush_training(2250);
    check(spoken.size() == 1 && spoken[0] == "Jump, complete", "completion without repeated input or contradictory count");
    clearSpeech();
    commands[0]["count"] = 0;
    narrator.on_training("UpdateTrainingCommands", commands, 2300);
    narrator.flush_training(2350);
    check(spoken.size() == 1 && spoken[0] == "Walk, reset", "counter reset spoken briefly");
    clearSpeech();
    narrator.on_training("ClearTrainingCommands", nullptr, 2400);
    narrator.on_training("UpdateTrainingCommands", commands, 2401);
    narrator.flush_training(2451);
    check(spoken.size() == 1 && contains(spoken[0], "Jump"), "identical lesson reread after clear in same batch");
    clearSpeech();
    narrator.on_training("ClearTrainingObjective", nullptr, 2500);
    narrator.read_desc();
    check(spoken.size() == 1 && !contains(spoken[0], "Movement") && contains(spoken[0], "Walk"), "description drops cleared objective");
    narrator.on_training("UpdateTrainingObjective", entry("Movement", 0), 2510);
    narrator.flush_training(2560);
    check(contains(spoken.back(), "Objective: Movement"), "same objective reread after clear");
    clearSpeech();
    narrator.on_training("UpdateTrainingObjective", entry("Use <TT>LIGHT_PUNCH</TT>", 1), 2600);
    narrator.flush_training(2650);
    check(contains(spoken.back(), "light punch") && !contains(spoken.back(), "_"), "attack markup spoken as attack name");
    clearSpeech();
    narrator.on_inv("Popup.swf", "Populate", R"({"Title":"Explanation","Body":"Press the jump key."})");
    narrator.on_training("UpdateTrainingObjective", entry("New goal", 1), 2700);
    narrator.flush_training(2750);
    check(spoken.size() == 2 && contains(spoken[0], "Explanation") && contains(spoken[1], "New goal") && !interrupts[1], "goals queue after popup explanation");
    narrator.read_desc();
    check(contains(spoken.back(), "Explanation"), "popup keeps description priority");
    narrator.on_inv("Popup.swf", "Close", "");
    narrator.read_desc();
    check(contains(spoken.back(), "New goal"), "goal description returns after popup");
    narrator.repeat();
    check(spoken.back() == spoken[spoken.size() - 2], "repeat works for goals");
    narrator.on_training("UpdateTrainingObjective", entry("Stale goal"), 2800);
    narrator.on_ei("uiScreenUtil", "LoadDestination", R"({"Destination":"Dojo.swf"})", 2801);
    clearSpeech();
    narrator.flush_training(3000);
    narrator.read_desc();
    check(spoken.size() == 1 && spoken[0] == "no description", "leaving fight cancels pending and cached goals");
    // Repeated updates must not keep delaying speech while the game runs.
    narrator.on_training("UpdateTrainingCommands", commands, 3100);
    for (int64_t t = 3110; t <= 3190; t += 10) narrator.on_training("UpdateTrainingCommands", commands, t);
    clearSpeech();
    narrator.flush_training(3200);
    check(spoken.size() == 1, "repeated events cannot starve speech");
    narrator.on_training("ClearTrainingCommands", nullptr, 3499);
    clearSpeech();
    for (int i = 0; i < 20; i++) narrator.on_training("UpdateTrainingObjective", entry("Ongoing task", 100, i), 3500 + i * 10);
    narrator.flush_training(3699);
    check(spoken.empty(), "changing progress remains coalesced");
    narrator.flush_training(3700);
    check(spoken.size() == 1 && contains(spoken[0], "19 of 100"), "continuous changes have a bounded wait");
    narrator.on_training("UpdateTrainingObjective", entry("Pending goal"), 3800);
    narrator.on_ei("uiScreenUtil", "LoadDestination", R"({"Destination":"PracticePauseMenu.swf"})", 3801);
    narrator.on_focus(json{{"swf", "PracticePauseMenu.swf"}, {"index", 0}, {"mode", "scan"},
        {"groups", {{"toggles", {{"label", "Dummy action"}, {"desc", "Choose opponent behaviour"}, {"state", "Focused"}, {"index", 0}}}}}});
    narrator.flush_training(3900);
    narrator.read_desc();
    check(spoken.back() == "Choose opponent behaviour", "pending goals preserve pause menu descriptions");
}

static size_t occurrences(std::string text, const std::string& phrase) {
    text = ki::lower(text);
    size_t result = 0, pos = 0;
    while ((pos = text.find(phrase, pos)) != std::string::npos) { ++result; pos += phrase.size(); }
    return result;
}

static void testTutorialNoise() {
    using namespace ki;
    Strings strings;
    Narrator narrator(strings);
    json goal = entry("Walking", 2);
    json commands = json::array({entry("Walk Forward - <TT>FORWARD</TT>", 2)});
    clearSpeech(); narrator.on_training("UpdateTrainingObjective", goal, 1000);
    narrator.on_training("UpdateTrainingCommands", commands, 1000); narrator.flush_training(1050);
    check(spoken.size() == 1 && contains(spoken[0], "Objective: Walking. Walk Forward") && occurrences(spoken[0], "0 of 2") == 1, "objective and command do not echo the same progress");
    clearSpeech(); commands[0]["count"] = 1;
    narrator.on_training("UpdateTrainingCommands", commands, 2000); narrator.flush_training(2050);
    check(spoken.size() == 1 && spoken[0] == "Walk Forward, 1 of 2", "counter changes omit the already introduced input sequence");
    clearSpeech(); commands[0]["count"] = 2; commands[0]["complete"] = true;
    commands.push_back(entry("Walk Backwards - <TT>BACK</TT>", 2)); goal["count"] = 1;
    narrator.on_training("UpdateTrainingObjective", goal, 3000); narrator.on_training("UpdateTrainingCommands", commands, 3000); narrator.flush_training(3050);
    check(spoken.size() == 1 && spoken[0] == "Walk Forward, complete. Walk Backwards - back, 0 of 2", "completed command and new action once, without aggregate counter echo");
    clearSpeech(); commands.erase(commands.begin());
    narrator.on_training("UpdateTrainingCommands", commands, 4000); narrator.flush_training(4050);
    check(spoken.empty(), "removing a completed row does not reread unchanged rows");
    commands[0]["count"] = 2; commands[0]["complete"] = true; goal["count"] = 2; goal["complete"] = true;
    narrator.on_training("UpdateTrainingObjective", goal, 5000); narrator.on_training("UpdateTrainingCommands", commands, 5000); narrator.flush_training(5050);
    check(spoken.size() == 1 && spoken[0] == "Walk Backwards, complete", "one completion instead of numeric and aggregate completion echoes");
    clearSpeech(); narrator.read_desc();
    check(contains(spoken.back(), "Walking, 2 of 2, complete") && contains(spoken.back(), "back, 2 of 2, complete"), "manual description retains full objective and command counts");

    // Reproduce the 0/1 -> 1/1 -> 0/1 loops seen at 23:43 and 23:46.
    Narrator combo(strings);
    goal = entry("Perform a Kick Ender", 1);
    commands = json::array({entry("Medium Kick - <TT>MEDIUM_KICK</TT>", 1), entry("Kick Ender - <TT>FORWARD</TT> + <TT>HEAVY_KICK</TT>", 1)});
    clearSpeech(); combo.on_training("UpdateTrainingObjective", goal, 1000); combo.on_training("UpdateTrainingCommands", commands, 1000); combo.flush_training(1050);
    check(spoken.size() == 1 && !contains(spoken[0], "Medium Kick - medium kick") && !contains(spoken[0], "0 of 1"), "new one-shot goals omit duplicate glyph and trivial count");
    clearSpeech();
    for (int attempt = 0; attempt < 6; ++attempt) {
        int64_t t = 2000 + attempt * 300;
        commands[0]["count"] = 1; commands[0]["complete"] = true;
        combo.on_training("UpdateTrainingCommands", commands, t); combo.flush_training(t + 50);
        commands[0]["count"] = 0; commands[0]["complete"] = false;
        combo.on_training("UpdateTrainingCommands", commands, t + 100); combo.flush_training(t + 150);
    }
    check(spoken.size() == 2 && spoken[0] == "Medium Kick, complete" && spoken[1] == "Medium Kick, reset", "repeated attempts produce one substep completion and one rate-limited reset");
    clearSpeech(); commands[0]["count"] = 1; commands[0]["complete"] = true;
    commands[1]["count"] = 1; commands[1]["complete"] = true; goal["count"] = 1; goal["complete"] = true;
    combo.on_training("UpdateTrainingObjective", goal, 6000); combo.on_training("UpdateTrainingCommands", commands, 6000); combo.flush_training(6050);
    check(spoken.size() == 1 && spoken[0] == "Kick Ender, complete", "final step remains spoken after suppressing repeated earlier completions");
    clearSpeech(); commands[0]["count"] = commands[1]["count"] = 0; commands[0]["complete"] = commands[1]["complete"] = false;
    combo.on_training("UpdateTrainingCommands", commands, 7000); combo.flush_training(7050);
    check(spoken.size() == 1 && spoken[0] == "Progress reset", "several resetting rows combine into one message");
    combo.on_training("ClearTrainingCommands", nullptr, 8000); combo.on_training("UpdateTrainingCommands", commands, 8000); combo.flush_training(8050);
    clearSpeech(); commands[0]["count"] = 1; commands[0]["complete"] = true;
    combo.on_training("UpdateTrainingCommands", commands, 9000); combo.flush_training(9050);
    check(spoken.size() == 1 && spoken[0] == "Medium Kick, complete", "lesson restart rearms completion feedback");

    Narrator advancing(strings);
    commands = json::array({entry("Heavy Combo Breaker - <TT>HEAVY_PUNCH</TT> + <TT>HEAVY_KICK</TT>", 1)});
    advancing.on_training("UpdateTrainingCommands", commands, 1000); advancing.flush_training(1050);
    commands[0]["count"] = 1; commands[0]["complete"] = true;
    advancing.on_training("UpdateTrainingCommands", commands, 2000);
    clearSpeech(); advancing.on_inv("Popup.swf", "Populate", R"({"Title":"Medium Combo Breaker","Body":"Now try the medium breaker."})");
    advancing.flush_training(2050);
    check(spoken.size() == 1 && contains(spoken[0], "Medium Combo Breaker") && !contains(spoken[0], "Heavy"), "old completion does not speak behind the next exercise's explanation");
    advancing.on_inv("Popup.swf", "Close", ""); advancing.on_training("UpdateTrainingCommands", commands, 3000); advancing.flush_training(3050);
    check(spoken.size() == 1, "suppressed stale completion cannot replay after popup closes");

    Narrator popup(strings);
    clearSpeech(); popup.on_inv("Popup.swf", "Populate", R"({"Title":"Standing Punches","Body":"Light LIGHT_PUNCH ( LIGHT_PUNCH ) Medium MEDIUM_PUNCH ( MEDIUM_PUNCH ) Heavy HEAVY_PUNCH ( HEAVY_PUNCH ). Press LIGHT_PUNCH + MEDIUM_PUNCH ( LIGHT_PUNCH + MEDIUM_PUNCH ). Use LIGHT_KICK ( MEDIUM_KICK )."})");
    check(spoken.size() == 1 && occurrences(spoken[0], "light punch") == 2 && occurrences(spoken[0], "medium punch") == 2 && occurrences(spoken[0], "heavy punch") == 1, "popup input names and glyph echoes collapse once");
    check(!contains(lower(spoken[0]), "light light punch") && !contains(lower(spoken[0]), "medium medium punch"), "attack strength labels do not echo their glyph names");
    check(contains(spoken[0], "light kick ( medium kick"), "distinct parenthesized input is preserved");
    Narrator localized(strings);
    clearSpeech(); localized.on_training("UpdateTrainingCommands", json::array({entry(u8"\u8df3\u8e8d - \u524d\u9032", 2)}), 1000); localized.flush_training(1050);
    check(spoken.size() == 1 && contains(spoken[0], u8"\u8df3\u8e8d - \u524d\u9032"), "distinct non-ASCII labels and instructions are preserved");

    Narrator standalone(strings);
    clearSpeech(); standalone.on_training("UpdateTrainingObjective", entry("Jump five times", 5), 1000); standalone.flush_training(1050);
    check(contains(spoken.back(), "Jump five times, 0 of 5"), "standalone objective exposes its required count");
    clearSpeech(); standalone.on_training("UpdateTrainingObjective", entry("Jump five times", 5, 1), 2000); standalone.flush_training(2050);
    check(spoken.size() == 1 && spoken[0] == "Objective, 1 of 5", "standalone objective has short progress feedback");
    Narrator reused(strings);
    commands = json::array({entry("Light Punch - <TT>LIGHT_PUNCH</TT>", 1)});
    reused.on_training("UpdateTrainingObjective", entry("First exercise", 1), 1000); reused.on_training("UpdateTrainingCommands", commands, 1000); reused.flush_training(1050);
    commands[0]["count"] = 1; commands[0]["complete"] = true;
    reused.on_training("UpdateTrainingCommands", commands, 2000); reused.flush_training(2050);
    commands[0]["count"] = 0; commands[0]["complete"] = false;
    clearSpeech(); reused.on_training("UpdateTrainingObjective", entry("Second exercise", 1), 3000); reused.on_training("UpdateTrainingCommands", commands, 3000); reused.flush_training(3050);
    check(spoken.size() == 1 && spoken[0] == "Objective: Second exercise. Light Punch", "new exercise with reused input reads its instruction again");
    clearSpeech(); commands[0]["count"] = 1; commands[0]["complete"] = true;
    reused.on_training("UpdateTrainingCommands", commands, 4000); reused.flush_training(4050);
    check(spoken.size() == 1 && spoken[0] == "Light Punch, complete", "reused substep completion rearms for a new exercise");
    Narrator commandOnly(strings);
    commands = json::array({entry("Attack", 1)});
    commandOnly.on_training("UpdateTrainingCommands", commands, 1000); commandOnly.flush_training(1050);
    clearSpeech();
    for (int attempt = 0; attempt < 3; ++attempt) {
        int64_t t = 2000 + attempt * 300;
        commands[0]["count"] = 1; commands[0]["complete"] = true;
        commandOnly.on_training("UpdateTrainingCommands", commands, t); commandOnly.flush_training(t + 50);
        commands[0]["count"] = 0; commands[0]["complete"] = false;
        commandOnly.on_training("UpdateTrainingCommands", commands, t + 100); commandOnly.flush_training(t + 150);
    }
    check(spoken.size() == 2 && spoken[0] == "Attack, complete" && spoken[1] == "Attack, reset", "absent objective does not turn every retry into a new exercise");
}

static void testReviewCompatibility() {
    using namespace ki;
    Strings strings;
    Narrator narrator(strings);
    narrator.on_training("UpdateTrainingCommands", json::array({entry("Jump", 2, 1)}), 1000);
    narrator.flush_training(1050);
    clearSpeech(); narrator.review_step(1);
    check(spoken.size() == 1 && contains(spoken[0], "Jump, 1 of 2"), "upstream line review can read a HUD announcement");
    narrator.repeat();
    check(spoken.back() == "Jump, 1 of 2", "review leaves the last HUD announcement available to repeat");
}

int main() {
    try { testNativeMemory(); testNarration(); testTutorialNoise(); testReviewCompatibility(); std::cout << "Training HUD checks passed\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
