// Opponent radar: a soft stereo pulse during a match, generated like the tones in FFVII-Access (waveOut, synthesized
// 16-bit PCM with fades) but stereo. Pan = which side the opponent is on relative to Player 1, rate = horizontal
// distance (closer = faster), pitch = opponent's height above the ground (in the air = higher).
// Position source: the match state the game's own Lua bindings Opponent_GetPosX/Y/Z read (see radar.cpp).
#pragma once
#include "common.h"

namespace ki {
namespace radar {

// Address of the game's match-state getter (the helper the Opponent_GetPos* bindings call: `mov rax,[rip+disp]`,
// then `test dword [rax+0x48], 0x8000`). The global it reads is decoded from that first instruction.
void setMatchGetter(uintptr_t fn);
// Same shape, for the round-state object GetRoundElapsedTime/GetRoundStartTime read: its frame counter at +0x370 only
// advances while the fight runs (not on the loading screen, not while paused), which is what gates the pulse.
void setRoundGetter(uintptr_t fn);
// Same shape, for the level object the GetLevelId Lua binding reads: the int at +0xbfc identifies the loaded stage
// (a hash; matched against the stage list by the narrator, so Random stages can be announced).
void setLevelGetter(uintptr_t fn);
int32_t levelId();            // 0 when unavailable
void setMatchLiveCallback(void (*cb)(int32_t levelId));   // called from the radar thread when a match becomes live
bool start();                 // starts the pulse thread (no-op without a decoded match global)
void stop();
void setEnabled(bool on);     // Ctrl+Shift+P
bool enabled();

}  // namespace radar
}  // namespace ki
