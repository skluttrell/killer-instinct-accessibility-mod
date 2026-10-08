#pragma once
#include "common.h"
#include "narrator.h"

namespace ki {
namespace hooks {

bool resolveAddresses();     // exe hash -> known RVAs, else AOB signatures; fills gfx::g_fn
bool install(Narrator* nar); // MinHook detours on the dispatcher, MovieRoot::Invoke and MovieImpl::Advance
void uninstall();
void setEnabled(bool on);
bool enabled();

}  // namespace hooks
}  // namespace ki
