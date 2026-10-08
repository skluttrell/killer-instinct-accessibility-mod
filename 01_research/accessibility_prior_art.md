# Accessibility Prior Art

## Part A: What Killer Instinct already does for blind players

Source: Game Developer postmortem by audio director Zachary Quarles; Ultra-Combo "Killer Insight: Sightless Kombat"; Family Gaming Database report; Xbox Accessibility Guideline 103 (cites KI as the example of spatial audio).

In-match (good):
- Hit, block, and take-damage sounds are hard-panned by screen thirds (Left/Center/Right), so position is audible.
- ~1,000 bespoke sounds per character, so the opponent's moves are identifiable by ear.
- HUD Volume slider (added after SightlessKombat's feedback) unlocks extra cues: KV/knockdown meter, Shadow meter, Instinct activation, Cinder burnout enders.
- Player-specific HUD sounds: P1 left channel, P2 right channel.
- Footsteps/jumps/foley emphasized for self-location.

Menus (bad):
- No menu narration on any platform. The postmortem openly calls this a gap, noting Xbox's speech synthesis API was available and unused.
- Blind players rely on: hand-written menu guides (the "Killer Instinct Community Guide" at reviews.sightlesskombat.com), memorized button counts ("go up one and press A"), and OCR (NVDA + Windows 10 OCR on the Steam version).
- Shadow Lords mode is "only accessible with large amounts of sighted help": text prompts, unvoiced dialogue, mission briefings, constantly changing menus. A stage is locked behind it.
- Ranked: cannot hear opponent rank or points earned.
- Known PC menu quirk: on PC, Help & Options is reached by going *up* one from Single/Multiplayer, and Audio is *up* two, which differs from console. A narration mod removes the need to know this.

Community requests on record:
- 2019-02-14, Ultra-Combo "Request for menu narration" (sljgamer1988): narrate menus, training, tutorials, move lists, online. No developer reply recorded.
- SightlessKombat's four asks: announce opponent rank online; read difficulty/timer settings; stat website; more character audio.

Official accessibility listing for AE (Xbox store): 8 features, none of which is screen narration.

## Part B: Screen-reader mods for other games (architecture reference)

| Mod | Game / engine | Loader | How it gets text | Speech | Language |
|---|---|---|---|---|---|
| CFC-Access | Capcom Fighting Collection (MT Framework + emulated CPS2/3) | `dinput8.dll` proxy (C, built with Zig) that spawns a Python process | Reads memory 20x/sec; identifies screen by vtable; pulls labels from `msg.arc`; Windows OCR for story text | Tolk | Python |
| Sparking Zero Access | DB Sparking! ZERO (UE5) | UE4SS + signature bypass | UE4SS object introspection: widget lookup table -> screen handlers -> TextBlock scan; 16 ms focus polling; HUD polling for HP/KI | UniversalSpeech via `speech_bridge.dll` | Lua |
| FFC Access | Fighting Fantasy Classics (Unity) | BepInEx 5 via `winhttp.dll` | Harmony patches on UI | Tolk | C# |
| Voice of the Old Republic | KOTOR 1 | `dinput8.dll` proxy | Prism speech bridge, in-process | Tolk fallback | C/C++ |
| DDLC Plus Access | DDLC+ (Unity) | MelonLoader | Harmony patches on text | UnityAccessibilityLib / UniversalSpeech | C# |
| FalloutNVAccess | Fallout NV (Gamebryo) | xNVSE plugin | Reads XML tile UI | Tolk | C++ |
| Grand Theft Accessibility | GTA V | ScriptHook | Script-level | - | - |
| Balatro screen reader | Balatro (LÖVE/Lua) | Lua mod | Direct | - | Lua |

Lessons that transfer to KI:
1. Passive memory reading beats hooking for stability (CFC-Access). Hooks break on every patch; KI is in maintenance mode so patches are rare, which softens this.
2. Pull strings from the game's own localization files instead of hardcoding; wording then matches the screen and other languages work.
3. A tiered text strategy (known screens -> generic text scan -> OCR) covers the long tail.
4. A single speech abstraction (Tolk or UniversalSpeech) handles NVDA/JAWS/SAPI.
5. Proxy DLL auto-load is the installer-free way to start a companion on launch and is what blind users expect.
6. KI is closer to CFC-Access (opaque custom engine, memory reading) than to Sparking Zero (engine with an introspection SDK). Expect more reverse engineering, less scripting.

## Part C: Platforms and distribution
- AccessForge: "1-click accessibility mod platform for blind gamers"; handles loader, deps, updates. Used by Sparking Zero Access. A finished KI mod should ship there.
- AccessMods (GitHub org): hub for accessibility mods plus `UnityAccessibilityLib` and `AccessModsInstaller`. Unity-focused but a community to announce in.
- Molitvan/blind-accessible-games-list: list to get KI added to once menus are narrated.
- AudioGames.net forum: where blind KI players already gather; recruit testers here.

## Part D: Fighting games with native narration (benchmarks)
- Mortal Kombat 11 / 1: full menu narration toggle.
- Street Fighter 6: sound accessibility options plus narration.
- Skullgirls PC: works with screen readers.
These set the UX bar: focus announcements, value changes on sliders, character names on select, round/health callouts.
