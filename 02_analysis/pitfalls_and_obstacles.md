# Pitfalls and Obstacles

Ordered roughly by how likely each is to stop the project.

## 1. Closed custom engine with no mod API
Double Helix/Iron Galaxy in-house engine. No script extender, no Lua, no object introspection like UE4SS. Every piece of UI state must be found by reverse engineering a stripped x64 binary. This is the dominant cost and the reason effort estimates are wide.

## 2. Locating Scaleform UI state from memory
Scaleform object trees live on the heap behind several pointer hops. Finding a stable root (the GFx MovieView for the frontend, or the game's own menu model) requires a debugger session. Risk that labels are set from localized string IDs each frame, so we must also decode the string-table format. Mitigation: start from a known visible string (search UTF-16 for "Single Player"), walk back to its owner, and confirm with a second string.

## 3. Focus/selection is a visual state
Which menu item is "highlighted" may exist only as an ActionScript variable or a tween state. If the game-side model does not store a cursor index, we need the in-process hook (Option 2) or color heuristics on captured frames. This decides Phase 1 vs Phase 2.

## 4. Version drift
Every patch shifts addresses; the 2024 Cheat Engine table died this way. Mitigation: AOB signatures, a version table keyed by exe hash, and the fact that KI has had no feature patch since April 2024.

## 5. Online play and injection ethics
No anti-cheat is known, but KI is a live online game on PlayFab/Xbox Live. An injected DLL in a competitive game invites suspicion even if nothing checks for it. Mitigations: prefer read-only external process access (Option 1); never write memory; document clearly; optionally disable in-process hooks while in online modes. Also consider Microsoft's stance: Xbox Live terms prohibit "cheating or tampering", so a read-only assistive tool should be defensible but is a gray area we should not hide.

## 6. UWP / Microsoft Store build is out of reach
Store/Game Pass PC users get the UWP build, which cannot be proxy-DLL loaded and restricts process access. The mod will be Steam-only, which excludes some blind players. External read-only memory access of a UWP app is possible with elevated rights but is fragile; treat as unsupported.

## 7. No pak repacker
Asset-level fixes (Option 4) are blocked until a repacker exists. The unpacker author (thethiny) and the Hitbox Editor author are the people who would know.

## 8. Shadow Lords complexity
Procedurally changing menus, mission cards, dialogue with no voice-over, timed choices. Even with text access, the mod needs a navigation model (what is selectable, what is informational). Budget it as its own milestone after core menus.

## 9. Match-time performance
A 20 Hz poll is fine; OCR during matches is not. Keep in-match features to memory reads of health/meters (already known from CE tables) and avoid frame capture in fights.

## 10. Testing without sighted shortcuts
The target users cannot verify behavior visually. We need blind testers from AudioGames.net and a sighted developer loop. Also log every spoken string to a file so testers can report mismatches.

## 11. Community rules
The official Ultra-Combo forum bans mod links/tutorials. Technical discussion lives on the Project K Discord and AudioGames.net. Advocacy for *official* narration can still use Ultra-Combo.

## 12. Legal / distribution
Do not redistribute game assets (extracted SWF, strings). Ship only code plus offsets/signatures. Tolk is LGPL (dynamic link is fine). If we extract string tables, generate them on the user's machine at first run rather than bundling.

## 13. Steam Deck / Proton
A chunk of players run KI on Deck. A Windows-only companion that uses ReadProcessMemory will not run inside Proton's prefix without extra work. Treat Linux as out of scope for v1.

## 14. Game process lifecycle quirk
Reports of the process lingering after exit on Windows 10; the companion must detect window close, not just process exit, and must re-scan on relaunch.
