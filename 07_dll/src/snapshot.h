// Focus snapshots: the per-screen reader table (same data as 06_narrator/agent.js SCREENS) and the snapshot
// function that produces the same JSON records the Frida agent sent ({ev:"focus", swf, why, index, groups, probe, cs}).
#pragma once
#include "common.h"

namespace ki {
namespace snap {

void init();                                   // parse the SCREENS table
bool hasScreen(const std::string& swf);
json snapshot(const std::string& swf, const std::string& why);   // {} when nothing could be read; sets rec["missing"] otherwise
std::string lastGoodSwf();

}  // namespace snap
}  // namespace ki
