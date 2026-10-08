# Tools (status 2026-10-08)

Installed with user approval: Frida + frida-tools (pip --user), prismatoid wheel for prism.dll (pip --user), JPEXS FFDec 26.3 + Temurin JRE 21 (winget). Capstone and Python 3.10 were already present. Cheat Engine is excluded (GUI-only). x64dbg / System Informer were not needed.

| Tool | Purpose | Phase | Notes |
|---|---|---|---|
| Killer Instinct (Steam) + AE content | Target | 0 | Steam build only; Store/UWP build is unsupported |
| System Informer (Process Hacker) | Inspect loaded modules, imports, anti-cheat presence | 0 | Read-only |
| x64dbg | Breakpoints on string writes, find Scaleform/menu code | 0 | April 2026 release has initial screen-reader support |
| Cheat Engine | Pointer scans, AOB generation, reuse published tables | 0 | Existing KI tables give health/meter pointers |
| ReClass.NET | Reconstruct menu/UI structs from memory | 0 | |
| KIUnpacker (thethiny) | Extract `.pak` contents | 0 | GPL-3.0 C++; may need AE fixes |
| QuickBMS | Alternative pak extraction | 0 | Only if KIUnpacker fails on AE |
| JPEXS Free Flash Decompiler | Decompile Scaleform `.gfx`/`.swf` UI, read ActionScript and TextField names | 0 | `.gfx` compression may need the FFDec GFX plugin |
| Python 3.12 + pymem | Companion app prototype | 1 | Fast iteration; CFC-Access precedent |
| .NET 8 (alternative) | Companion app if we prefer a single exe | 1 | Easier distribution to end users |
| Prism (prism.dll, MPL-2.0) | Speech to NVDA/JAWS/SAPI/OneCore/UIA... | 1 | user decision; verified picks NVDA here |
| NVDA | Testing target screen reader | 1 | Free |
| Windows.Media.Ocr (winrt) | OCR fallback | 1 | Built into Windows |
| MinHook or Microsoft Detours | In-process hooks | 3 | Only if focus must come from Scaleform |
| dinput8 proxy template | Auto-load | 3 | Verify exe imports dinput8 first; else `dxgi.dll` or `version.dll` |
| AccessForge | Distribution to blind users | 1+ | 1-click installer platform |
