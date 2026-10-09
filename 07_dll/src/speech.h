// Speech output through prism.dll (https://github.com/ethindp/prism, MPL-2.0): best available screen reader
// (NVDA, JAWS, ...) or SAPI. A worker thread owns the backend so the game's UI thread never waits on it.
#pragma once
#include "common.h"

namespace ki {
namespace speech {

bool init(const std::wstring& prismDllPath);   // loads prism.dll, picks the best backend, starts the worker
void speak(const std::string& text, bool interrupt);
void stop();
std::string backendName();
void shutdown();
void abandon();   // process exit: the worker thread is already dead; detach it so the static std::thread's destructor does not terminate()

}  // namespace speech
}  // namespace ki
