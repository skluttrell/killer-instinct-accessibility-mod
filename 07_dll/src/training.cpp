#include "training.h"
#include <algorithm>
#include <limits>

namespace ki {
namespace training {
namespace {

constexpr uint32_t nameHash(const char* name) {
    uint32_t crc = 0xffffffffu;
    for (; *name; ++name) {
        crc ^= (unsigned char)*name;
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    }
    return crc ^ 0xffffffffu;
}

bool readBytes(uintptr_t address, void* out, size_t size) {
    if (!address || size > std::numeric_limits<uintptr_t>::max() - address) return false;
    __try { memcpy(out, (const void*)address, size); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> bool readT(uintptr_t address, T& out) { return readBytes(address, &out, sizeof out); }

// DHStd::string: 16 bytes inline or a pointer, length +0x10, capacity +0x18.
// The retail build usually omits the strings in event-name keys; identify events
// by their CRC32(lowercase name), as uiQueuedEvent::Dispatch does.
bool readString(uintptr_t address, std::string& out) {
    size_t length = 0, capacity = 0;
    if (!readT(address + 0x10, length) || !readT(address + 0x18, capacity) ||
        length > 4096 || length > capacity || (capacity < 16 && length >= 16)) return false;
    uintptr_t text = address;
    if (capacity >= 16 && !readT(address, text)) return false;
    out.resize(length);
    if (length && !readBytes(text, &out[0], length)) return false;
    return out.find('\0') == std::string::npos &&
        (out.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, out.data(), (int)out.size(), nullptr, 0) > 0);
}

// uiTrainingCommand (GetTrainingCommand, RVA 0x68c0d0; copy at 0x65dc70):
// localized text +0, mirrored text +0x28, target +0x50, count +0x54,
// and an explicit completion flag +0x5c. Entries are 0x60 bytes apart.
bool readEntry(uintptr_t address, json& out) {
    std::string text;
    int32_t target = 0, count = 0;
    uint8_t complete = 0;
    if (!readString(address, text) || !readT(address + 0x50, target) ||
        !readT(address + 0x54, count) || !readT(address + 0x5c, complete) ||
        target < 0 || target > 1000000 || count < 0 || count > 1000000 || complete > 1) return false;
    out = json{{"text", text}, {"target", target}, {"count", target > 0 ? std::min(count, target) : count},
               {"complete", complete != 0 || (target > 0 && count >= target)}};
    return true;
}

}  // namespace

Event identify(uintptr_t script, uintptr_t eventKey) {
    uint32_t scriptHash = 0, eventHash = 0;
    constexpr uint32_t hud = nameHash("hud"), commands = nameHash("updatetrainingcommands"),
        objective = nameHash("updatetrainingobjective"), clear = nameHash("cleartrainingobjective");
    if (!script || !eventKey || !readT(script + 0x18, scriptHash) || scriptHash != hud ||
        !readT(eventKey, eventHash)) return Event::None;
    if (eventHash == commands) return Event::Commands;
    if (eventHash == objective) return Event::Objective;
    if (eventHash == clear) return Event::ClearObjective;
    return Event::None;
}

const char* eventName(Event event) {
    switch (event) {
        case Event::Commands: return "UpdateTrainingCommands";
        case Event::Objective: return "UpdateTrainingObjective";
        case Event::ClearObjective: return "ClearTrainingObjective";
        default: return "";
    }
}

uintptr_t managerGlobal(uintptr_t getter) {
    uint8_t opcode[3]{};
    int32_t displacement = 0;
    if (!readBytes(getter, opcode, sizeof opcode) || opcode[0] != 0x48 || opcode[1] != 0x8b || opcode[2] != 0x05 ||
        !readT(getter + 3, displacement)) return 0;
    return getter + 7 + displacement;
}

bool snapshot(uintptr_t manager, Event event, json& out) {
    out = nullptr;
    // ClearTrainingObjective only sends an event: the old text remains at +0x900.
    if (event == Event::ClearObjective) return true;
    if (!manager) return false;
    if (event == Event::Objective) return readEntry(manager + 0x900, out);
    if (event != Event::Commands) return false;
    uintptr_t begin = 0, end = 0;
    if (!readT(manager + 0x8e0, begin) || !readT(manager + 0x8e8, end) || end < begin ||
        (end - begin) % 0x60 != 0 || (end - begin) / 0x60 > 128 || (!begin && end)) return false;
    json entries = json::array();
    for (uintptr_t address = begin; address < end; address += 0x60) {
        json entry;
        if (!readEntry(address, entry)) return false;
        entries.push_back(std::move(entry));
    }
    out = std::move(entries);
    return true;
}

bool snapshotGlobal(uintptr_t global, Event event, json& out) {
    uintptr_t manager = 0;
    return event == Event::ClearObjective ? snapshot(0, event, out) :
        readT(global, manager) && snapshot(manager, event, out);
}

}  // namespace training
}  // namespace ki
