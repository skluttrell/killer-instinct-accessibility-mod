// Native Lua HUD events and training state, separate from the Scaleform menus.
#pragma once
#include "common.h"

namespace ki {
namespace training {

enum class Event { None, Commands, Objective, ClearObjective };
Event identify(uintptr_t script, uintptr_t eventKey);
const char* eventName(Event event);
// GetNumberTrainingCommands / GetTrainingObjectiveTitle both load the UI manager
// through the RIP-relative pointer at the start of their signature-checked code.
uintptr_t managerGlobal(uintptr_t getter);
// Copies strings and counters while the native event is being dispatched. No
// game pointers are retained in the resulting JSON or used by the speech thread.
bool snapshot(uintptr_t manager, Event event, json& out);
bool snapshotGlobal(uintptr_t global, Event event, json& out);

}  // namespace training
}  // namespace ki
