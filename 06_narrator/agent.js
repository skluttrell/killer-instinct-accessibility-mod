// KI accessibility narrator - Frida agent (build 14306144, exe SHA-256 33bbd291...edf6).
// Read-only: hooks three engine functions and calls public ActionScript getters through the engine's own
// GFx entry points on the UI thread. Nothing in game memory is patched.
//   0x5dfe50  CallUILuaScript dispatcher (AS3 -> Lua): screen, function, json   -> 'ei' events + focus snapshots
//   0x1122ef0 AS3::MovieRoot::Invoke (Lua -> AS3 via root.Invoke(swf, func, json)) -> 'inv' events
//   GFx::Value API: ObjectInterface slot 4 GetMember (0x11502e0), slot 6 Invoke (0x1150a00), Value release 0x350bd0
//   0xe4a700  GFx::MovieImpl::Advance (MovieImpl vtable slot 24): its return is the per-frame UI-thread tick for deferred snapshots
'use strict';

const MOD = 'KILLERINSTINCTX64_R.EXE';
const mod = Process.findModuleByName(MOD);
if (mod === null) throw new Error('game module not found');
const base = mod.base;
const R = {
  dispatcher: 0x5dfe50, rootInvoke: 0x1122ef0, objInvoke: 0x1150a00, objGetMember: 0x11502e0,
  valueRelease: 0x350bd0, advance: 0xe4a700, vtMovieRoot: 0x20a2928, vtObjIface: 0x20a6768, vtMovieImpl: 0x203dd38,
};
function check(vt, slot, rva, what) {
  const got = base.add(vt + slot * 8).readPointer();
  if (!got.equals(base.add(rva))) throw new Error('vtable check failed for ' + what + ': got ' + got.sub(base) + ' (different game build?)');
}
check(R.vtMovieRoot, 57, R.rootInvoke, 'MovieRoot::Invoke');
check(R.vtObjIface, 6, R.objInvoke, 'ObjectInterface::Invoke');
check(R.vtObjIface, 4, R.objGetMember, 'ObjectInterface::GetMember');
check(R.vtMovieImpl, 24, R.advance, 'MovieImpl::Advance');

const RootInvoke = new NativeFunction(base.add(R.rootInvoke), 'uint8', ['pointer', 'pointer', 'pointer', 'pointer', 'uint32']);
const ObjInvoke = new NativeFunction(base.add(R.objInvoke), 'uint8', ['pointer', 'pointer', 'pointer', 'pointer', 'pointer', 'uint64', 'uint8']);
const ObjGetMember = new NativeFunction(base.add(R.objGetMember), 'uint8', ['pointer', 'pointer', 'pointer', 'pointer', 'uint8']);
const ValueRelease = new NativeFunction(base.add(R.valueRelease), 'void', ['pointer']);

// ---- GFx::Value (0x30 bytes): +0x10 pObjectInterface, +0x18 Type, +0x20 payload ----
const VSIZE = 0x30;
const ZEROS = new Array(VSIZE).fill(0);
// One fresh Value per use (a rotating pool clobbered the live screen reference after 16 allocations).
function vnew() { return Memory.alloc(VSIZE); }
function vtype(v) { return v.add(0x18).readU32(); }
function vkind(v) { return vtype(v) & 0x3f; }
function visobj(v) { const k = vkind(v); return k === 8 || k === 9 || k === 0xA; }
function vrelease(v) { if (vtype(v) & 0x40) { try { ValueRelease(v); } catch (e) {} } v.writeByteArray(ZEROS); }
function vread(v) {
  const t = vtype(v), k = t & 0x3f;
  let p = v.add(0x20);
  if (k === 6 || k === 7) {
    if (t & 0x40) p = p.readPointer();
    const s = p.readPointer();
    return k === 6 ? s.readUtf8String() : s.readUtf16String();
  }
  if (k === 5) return p.readDouble();
  if (k === 4) return p.readU32();
  if (k === 3) return p.readS32();
  if (k === 2) return p.readU8() !== 0;
  if (k === 1) return null;
  if (k === 0) return undefined;
  return {obj: k};
}
const CSTR = {};
function cstr(s) { if (!CSTR[s]) CSTR[s] = Memory.allocUtf8String(s); return CSTR[s]; }
const argV = Memory.alloc(VSIZE);
function vsetstr(v, s) { v.writeByteArray(ZEROS); v.add(0x18).writeU32(6); v.add(0x20).writePointer(cstr(s)); }

// ---- root + screen access ----
const VT_ROOT = base.add(R.vtMovieRoot);
let root = null;
function findRoot(wrapper) {
  try {
    const z = wrapper.readPointer().add(0x18).readPointer().add(0x18).readPointer();
    if (z.readPointer().equals(VT_ROOT)) return z;
  } catch (e) {}
  return null;
}
function rootCallStr(name) {           // root.<name>() -> string|null
  const out = vnew();
  const ok = RootInvoke(root, cstr(name), out, ptr(0), 0);
  const r = ok ? vread(out) : null;
  vrelease(out);
  return typeof r === 'string' ? r : null;
}
function getRef(swf) {                 // root.GetSWFRefFromString(swf) -> object Value (caller releases) | null
  vsetstr(argV, swf);
  const out = vnew();
  const ok = RootInvoke(root, cstr('root.GetSWFRefFromString'), out, argV, 1);
  if (ok && visobj(out)) return out;
  vrelease(out);
  return null;
}
function getMember(objV, name, out) {
  const iface = objV.add(0x10).readPointer(), pdata = objV.add(0x20).readPointer();
  if (iface.isNull()) return false;
  return ObjGetMember(iface, pdata, cstr(name), out, vkind(objV) === 0xA ? 1 : 0) !== 0;
}
function readPath(objV, path) {       // "a.b.c" -> JS value or undefined
  let cur = objV;
  const segs = path.split('.');
  for (let i = 0; i < segs.length; i++) {
    const nxt = vnew();
    const ok = getMember(cur, segs[i], nxt);
    if (cur !== objV) vrelease(cur);
    if (!ok) { vrelease(nxt); return undefined; }
    cur = nxt;
    if (i < segs.length - 1 && !visobj(cur)) { vrelease(cur); return undefined; }
  }
  const r = vread(cur);
  if (cur !== objV) vrelease(cur);
  return r;
}
function invoke0(objV, name) {         // obj.name() -> JS value
  return invokeArgs(objV, name, []);
}
function invokeArgs(objV, name, args) { // obj.name(args...) -> JS value; args: JS numbers (int) or strings
  const iface = objV.add(0x10).readPointer(), pdata = objV.add(0x20).readPointer();
  if (iface.isNull()) return undefined;
  let pargs = ptr(0);
  if (args.length) {
    pargs = Memory.alloc(VSIZE * args.length);
    for (let i = 0; i < args.length; i++) {
      const a = pargs.add(i * VSIZE);
      if (typeof args[i] === 'number') { a.add(0x18).writeU32(3); a.add(0x20).writeS32(args[i]); }
      else vsetstr(a, String(args[i]));
    }
  }
  const out = vnew();
  const ok = ObjInvoke(iface, pdata, out, cstr(name), pargs, args.length, vkind(objV) === 0xA ? 1 : 0);
  const r = ok ? vread(out) : undefined;
  vrelease(out);
  return r;
}
function memberObj(objV, name) {       // child object Value (caller releases) | null
  const out = vnew();
  if (getMember(objV, name, out) && visobj(out)) return out;
  vrelease(out);
  return null;
}

// ---- per-screen readers: where the focused item's text lives (from the decompiled AS3) ----
// 'state' = the button's current timeline label: GainFocus/FocusedLoop(/RemappingLoop) when it is the live cursor,
// LoseFocus/UnfocusedLoop or BuildOn/Off otherwise (BaseButton.Activate/Deactivate in the AS3).
// Two reader modes: index mode calls the screen's public selection getter (indexFn, default GetSelectionIndex) and
// reads the item at that index; scan mode (screens without a getter) reads every item's state label and reports the
// focused one with its position among the visible items.
const BTN = {label: 'mcTxtLarge.txt.text', state: 'currentLabel', visible: 'visible'};
const SCREENS = {
  'MainMenu.swf': {groups: {main: {item: 'mcMenuGroup.mcMenuBtn{i}.mcBtn',
      fields: {label: 'mcTxt.txtButton.text', desc: 'mcDescription.mcDescriptionContainer.txtDescription.text', state: 'currentLabel'}}}},
  'OptionsMenu.swf': {groups: {
      entries: {item: 'mcMenuGroup.mcOptionsMain.mcBtn{i}', fields: {label: 'mcTxtLarge.txt.text', desc: 'mcTextDesc.mcTextHolder.txtDescription.text', state: 'currentLabel'}},
      toggles: {item: 'mcMenuGroup.mcToggleMenu.mcBtn{i}', fields: {label: 'mcTxtLarge.txt.text', desc: 'mcTextDesc.mcTextHolder.txtDescription.text',
               desc2: 'mcTextDesc.txtDescription.text', value: 'mcValueLarge.txt.text', valueShown: 'mcValueLarge.visible', slider: 'mcSliderValue.txt.text', state: 'currentLabel'}}}},
  // Character select: no GetSelectionIndex; the public FighterSelectModule gives the grid focus per side, the
  // costume/color modules give their selected indices. Names come from the Lua_Populate JSON (narrator.py).
  'CharacterSelect.swf': {custom: 'charselect'},
  // Pause menu (in match): PauseMenu.as navigates after the scroll sound, so the deferred snapshot is the one that counts.
  'PauseMenu.swf': {scan: true, groups: {main: {item: 'mcMenuGroup.mcBtn{i}', count: 7,
      fields: {label: 'mcTxtLarge.txtButton.text', state: 'currentLabel', visible: 'visible'}}}},
  // Practice pause menu: three menus (Main / Dummy+Practice toggles / Theme list) under a category toggle.
  'PracticePauseMenu.swf': {scan: true, probe: {category: 'mcMenuGroup.mcToggle.mcFilterTxt.txtFilter.text'}, groups: {
      main: {when: 'mcMenuGroup.mcPauseOptionsMain.visible', item: 'mcMenuGroup.mcPauseOptionsMain.mcBtn{i}', count: 8, fields: BTN},
      toggles: {when: 'mcMenuGroup.mcToggleMenu.visible', item: 'mcMenuGroup.mcToggleMenu.mcBtn{i}', count: 12, fields: {label: 'mcTxtLarge.txt.text', value: 'mcValueLarge.txt.text', valueShown: 'mcValueLarge.visible',
               slider: 'mcSliderValue.txt.text', desc: 'mcTextDesc.mcTextHolder.txtDescription.text', desc2: 'mcTextDesc.txtDescription.text', state: 'currentLabel', visible: 'visible'}},
      themes: {when: 'mcMenuGroup.mcThemesMenu.visible', item: 'mcMenuGroup.mcThemesMenu.mcScrollList.mcBtn{i}', count: 8, fields: BTN}}},
  'Trials.swf': {scan: true, probe: {difficulty: 'mcMenuGroup.mcDifficulty.txtItem.text'}, groups: {main: {item: 'mcMenuGroup.mcBtn{i}', count: 4,
      fields: {label: 'mcTxtLarge.txt.text', desc: 'mcTextDesc.txtDescription.text', state: 'currentLabel', visible: 'visible'}}}},
  // Dojo: 2 rows x 8 lessons, GetSelectionIndex is the flat index.
  'Dojo.swf': {probe: {mode: 'mcDojo.mcCategoryToggle.txtDojoMode.text', title: 'mcDojo.mcDisplayContent.mcTitle.txtDojoLevelName.text',
      desc: 'mcDojo.mcDisplayContent.mcDescription.txtDescription.text'},
      groups: {rows: {item: 'mcDojo.mcDojoRow{r}.mcEntry{e}', rowSize: 8, fields: {label: 'mcTxt.txtDojoLevelName.text', state: 'currentLabel', locked: 'mcLocked.visible', done: 'mcCompleted.visible'}}}},
  // Popup with a vertical button list (store style): GetSelectedDisplayButton -> DISPLAY<i>.
  'Popup.swf': {indexFn: 'GetSelectedDisplayButton', probe: {title: 'Title.text', body: 'Body.text', hasButtons: 'ButtonContainer.visible'},
      groups: {display: {item: 'DISPLAY{i}', fields: {label: 'mcTxt.txtButton.text', state: 'currentLabel', visible: 'visible'}}}},
  'StoreMain.swf': {scan: true, groups: {items: {items: ['mcStoreMain.mcItem0', 'mcStoreMain.mcItem1_1', 'mcStoreMain.mcItem2_1', 'mcStoreMain.mcItem3_1',
      'mcStoreMain.mcItem4_1', 'mcStoreMain.mcItem5_1', 'mcStoreMain.mcItem6_1', 'mcStoreMain.mcItem1', 'mcStoreMain.mcItem2', 'mcStoreMain.mcItem3',
      'mcStoreMain.mcItem4', 'mcStoreMain.mcItem5', 'mcStoreMain.mcItem6', 'mcStoreMain.mcItem7'],
      fields: {label: 'mcTitle.txtItem.text', state: 'currentLabel', visible: 'visible'}}}},
  'MatchOutcome.swf': {scan: true, groups: {
      p1: {item: 'mcP1RematchOptions.MenuOption_0{i}', first: 1, count: 4, fields: {label: 'mcTxt.txtButton.text', state: 'currentLabel', visible: 'visible'}},
      p2: {item: 'mcP2RematchOptions.MenuOption_0{i}', first: 1, count: 4, fields: {label: 'mcTxt.txtButton.text', state: 'currentLabel', visible: 'visible'}},
      p1stats: {items: ['mcP1Stats.mcOffense', 'mcP1Stats.mcDefense', 'mcP1Stats.mcCombos', 'mcP1Stats.mcVariety'], fields: {state: 'currentLabel', visible: 'visible'}},
      p2stats: {items: ['mcP2Stats.mcOffense', 'mcP2Stats.mcDefense', 'mcP2Stats.mcCombos', 'mcP2Stats.mcVariety'], fields: {state: 'currentLabel', visible: 'visible'}}}},
  // Controller config: per side, 13 remappable commands + right-stick toggle + combo-assist toggle, then Save/Default.
  'ControllerConfig.swf': {scan: true, probe: {p1start: 'mcMenuGroup.mcStartPanelP1.visible', p2start: 'mcMenuGroup.mcStartPanelP2.visible',
      p1panel: 'mcMenuGroup.mcControlsPanelP1.visible', p2panel: 'mcMenuGroup.mcControlsPanelP2.visible',
      p1prompt: 'mcMenuGroup.mcStartPanelP1.mcPressStart.txtStart.text'},
      groups: {
      p1: {when: 'mcMenuGroup.mcControlsPanelP1.visible', item: 'mcMenuGroup.mcControlsPanelP1.mcButtonGroup.mcBtn{i}', count: 15, fields: {state: 'currentLabel', visible: 'visible', text: 'mcTxt.TxtItem.text', value: 'mcValue.TxtItem.text', warn: 'mcCmdIconGroup.mcMessageText.txt.text', warnState: 'mcCmdIconGroup.currentLabel'}},
      p1exit: {when: 'mcMenuGroup.mcControlsPanelP1.visible', items: ['mcMenuGroup.mcControlsPanelP1.mcButtonLeft', 'mcMenuGroup.mcControlsPanelP1.mcButtonRight'], fields: {label: 'mcTxt.txtButton.text', state: 'currentLabel'}},
      p2: {when: 'mcMenuGroup.mcControlsPanelP2.visible', item: 'mcMenuGroup.mcControlsPanelP2.mcButtonGroup.mcBtn{i}', count: 15, fields: {state: 'currentLabel', visible: 'visible', text: 'mcTxt.TxtItem.text', value: 'mcValue.TxtItem.text', warn: 'mcCmdIconGroup.mcMessageText.txt.text', warnState: 'mcCmdIconGroup.currentLabel'}},
      p2exit: {when: 'mcMenuGroup.mcControlsPanelP2.visible', items: ['mcMenuGroup.mcControlsPanelP2.mcButtonLeft', 'mcMenuGroup.mcControlsPanelP2.mcButtonRight'], fields: {label: 'mcTxt.txtButton.text', state: 'currentLabel'}}}},
  // Event-driven screens (narrator.py reads the Lua traffic): StageSelect (StageSelectionChanged), CommandList (EntrySelected).
  'CommandList.swf': {indexFn: 'GetSelectionIndex'},
  'StageSelect.swf': {custom: 'none'},
  'MultiplayerLobby.swf': {indexFn: 'GetSelectionIndex'},
};
function isFocusedLabel(s) {
  if (typeof s !== 'string') return false;
  if (s === 'RemappingLoop' || s.indexOf('Step') === 0) return true;   // a toggle only steps while it has the cursor
  return s.indexOf('Focus') >= 0 && s.indexOf('Lose') !== 0 && s.indexOf('Unfocused') !== 0;
}
function itemPaths(gc) {
  if (gc.items) return gc.items;
  const first = gc.first || 0, out = [];
  for (let i = first; i < first + gc.count; i++) out.push(gc.item.replace('{i}', String(i)));
  return out;
}
function readFields(ref, item, fields, skip) {
  const f = {};
  for (const k in fields) {
    if (skip && skip[k] !== undefined) { f[k] = skip[k]; continue; }
    const v = readPath(ref, item + '.' + fields[k]);
    if (v !== undefined) f[k] = v;
  }
  return f;
}
function snapshotCharSelect(ref, rec) {
  const cs = {};
  const fs = memberObj(ref, 'mcFighterSelect');
  if (fs) {
    cs.f0 = invokeArgs(fs, 'GetFocusedFighterIndex', [0]);
    cs.f1 = invokeArgs(fs, 'GetFocusedFighterIndex', [1]);
    vrelease(fs);
  }
  for (const side of [1, 2]) {
    const co = memberObj(ref, 'mcP' + side + 'CostumeSelect');
    if (co) { cs['c' + (side - 1)] = invoke0(co, 'GetSelectedCostumeIndex'); cs['slot' + (side - 1)] = invoke0(co, 'GetSelectedCustomSlot'); vrelease(co); }
    const cl = memberObj(ref, 'mcP' + side + 'ColorSelect');
    if (cl) { cs['col' + (side - 1)] = invoke0(cl, 'GetSelectedColorBackendIndex'); vrelease(cl); }
  }
  rec.cs = cs;
}
function snapshotScan(ref, cfg, rec) {       // scan mode: find the focused item of every group by its state label
  rec.index = -1;
  rec.groups = {};
  for (const g in cfg.groups) {
    const gc = cfg.groups[g];
    if (gc.when && readPath(ref, gc.when) === false) { rec.groups[g] = {count: 0, hidden: true}; continue; }  // whole menu hidden
    const paths = itemPaths(gc);
    let focused = null, pos = -1, visible = 0;
    for (let i = 0; i < paths.length; i++) {
      const st = readPath(ref, paths[i] + '.' + gc.fields.state);
      let vis = true;
      if (gc.fields.visible) { const v = readPath(ref, paths[i] + '.' + gc.fields.visible); vis = v !== false; }
      if (vis && gc.fields.label) {                  // an unused button can stay visible with empty text: do not count it
        const lb = readPath(ref, paths[i] + '.' + gc.fields.label);
        if (lb === '' || lb === undefined || lb === 'missing text') vis = false;
        else if (isFocusedLabel(st) && focused === null) { focused = readFields(ref, paths[i], gc.fields, {state: st, visible: true, label: lb}); }
      } else if (vis && isFocusedLabel(st) && focused === null) focused = readFields(ref, paths[i], gc.fields, {state: st, visible: true});
      if (vis) visible++;
      if (focused && pos < 0) { focused.index = (gc.first || 0) + i; pos = visible - 1; }
    }
    rec.groups[g] = focused || {};
    if (focused) focused.pos = pos;
    rec.groups[g].count = visible;
  }
}
let inSnapshot = false, lastGoodSwf = null;
function snapshot(swf, why) {
  if (!root || inSnapshot) return;
  inSnapshot = true;
  const t0 = Date.now();
  try {
    let ref = getRef(swf);
    if (!ref && lastGoodSwf && lastGoodSwf !== swf) {
      // a shared component called a script of a movie that is not loaded (e.g. the practice menu's toggles call
      // OptionsMenu.SetVariables): the cursor is still on the last screen we could read
      ref = getRef(lastGoodSwf);
      if (ref) { currentSwf = swf = lastGoodSwf; }
    }
    if (!ref) { send({ev: 'focus', swf: swf, why: why, missing: true}); return; }
    lastGoodSwf = swf;
    const rec = {ev: 'focus', swf: swf, why: why, t: t0};
    try {
      const cfg = SCREENS[swf];
      if (cfg && cfg.probe) { rec.probe = {}; for (const k in cfg.probe) { const v = readPath(ref, cfg.probe[k]); if (v !== undefined) rec.probe[k] = v; } }
      if (cfg && cfg.custom === 'charselect') { snapshotCharSelect(ref, rec); rec.index = -1; }
      else if (cfg && cfg.custom === 'none') { rec.index = -1; }
      else if (cfg && cfg.scan) { snapshotScan(ref, cfg, rec); }
      else {
        const idx = invoke0(ref, (cfg && cfg.indexFn) || 'GetSelectionIndex');
        rec.index = typeof idx === 'number' ? idx : null;
        if (cfg && cfg.groups && rec.index !== null) {
          if (cfg.mode) { const m = readPath(ref, cfg.mode); if (typeof m === 'boolean') rec.mode = m; }
          rec.groups = {};
          for (const g in cfg.groups) {
            const gc = cfg.groups[g];
            let item;
            if (gc.rowSize) item = gc.item.replace('{r}', String(Math.floor(rec.index / gc.rowSize))).replace('{e}', String(rec.index % gc.rowSize));
            else item = gc.item.replace('{i}', String(rec.index));
            rec.groups[g] = readFields(ref, item, gc.fields);
          }
        }
      }
    } catch (e) { rec.err = String(e); }
    vrelease(ref);
    rec.ms = Date.now() - t0;
    send(rec);
  } finally { inSnapshot = false; }
}
// Deferred snapshot: some screens play the scroll sound before moving the cursor (PauseMenu, Trials) and others set
// their text after it, so after every trigger a second snapshot is taken when the current frame's MovieImpl::Advance
// returns (same UI thread, outside the AVM: the normal place for a host to call the GFx::Value API).
// Deferred snapshots per trigger: next frame (cursor moved after the sound), 30 and 180 frames later (menus that build on
// with an animation before the first button gains focus, e.g. ControllerConfig, MatchOutcome). The narrator de-duplicates.
const SETTLE_FRAMES = [1, 30, 180];   // +1 frame, +0.5 s, +3 s (MatchOutcome builds on for ~3 s)
let pending = null, uiThread = null;
function schedule(swf, why) { pending = {swf: swf, why: why, due: SETTLE_FRAMES.slice(), frame: 0}; }
function runPending(where) {
  if (!pending || Process.getCurrentThreadId() !== uiThread) return;
  pending.frame++;
  if (pending.frame < pending.due[0]) return;
  pending.due.shift();
  const p = pending;
  if (p.due.length === 0) pending = null;
  snapshot(p.swf, p.why + (p.frame > 1 ? '++' : '+'));
}
let tickMovie = null;   // count frames of one movie only (several movies advance per frame)
Interceptor.attach(base.add(R.advance), {
  onEnter(args) { if (tickMovie === null) tickMovie = args[0]; this.tick = args[0].equals(tickMovie); },
  onLeave(ret) { if (this.tick && pending && enabled) runPending('advance'); }
});

// ---- hooks ----
function gfxArgStr(p) {
  try { const r = vread(p); return typeof r === 'string' ? r : (r === undefined ? '' : JSON.stringify(r)); } catch (e) { return '<err>'; }
}
const SCRIPT2SWF = {MainMenu: 'MainMenu.swf', MessageBox: 'Popup.swf', CharSelectMenu: 'CharacterSelect.swf'};
// Scripts that never own the cursor: they must not change the current screen.
const HELPERS = new Set(['uiSounds', 'uiScreenUtil', 'Legend', 'GenericAssets', 'PauseBackground', 'FrontendBackground', 'FrontEndBackground',
  'Toast', 'LoadingScreen', 'BackgroundShell', 'ForegroundShell']);
let prevSwf = null;
const TRIGGERS = new Set(['PlayScroll', 'PlayPauseMenuButtons', 'CheckIsDestInstalled', 'ScreenShown', 'PlayScrollLeftRight',
  'PlayScrollLeftRight2', 'PlayNegative', 'PlayAccept', 'PlayBack', 'SetVariables', 'CommandSelect', 'PlayOption', 'PlaySoundString',
  // character select
  'PlayScrollP1', 'PlayScrollP2', 'PlayNegativeP1', 'PlayNegativeP2', 'PlayBackP1', 'PlayBackP2', 'AS_PlayerPickedFighter',
  'AS_PlayerPickedCostume', 'AS_PlayerPickedColor', 'AS_ResetPlayerSelection', 'AS_SetSelections',
  // pause menus, post-match, controller config, practice, command list, stage select
  'PlayExitMenu', 'ResetPauseMenuButtons', 'PlayTransIn', 'PlayScroll1', 'PlayScroll2', 'PlayPostMatchButtonsP1', 'PlayPostMatchButtonsP2',
  'PlayAcceptP1', 'PlayAcceptP2', 'AS_ListenForAnyKey', 'GetEntriesInCategory', 'UpdateCategoryIndex', 'AS_EnteredPracticeTab',
  'EntrySelected', 'StageSelectionChanged', 'CallPostMatchSelectionChanged', 'UpdateMenuIndices']);
// Lua -> AS3 calls after which the screen text is fresh (snapshot in onLeave of root.Invoke).
const INV_TRIGGERS = new Set(['Populate', 'Lua_Populate', 'PopulateEntries', 'PopulateStatesOnly', 'RefreshPage', 'Lua_ChangeSelectionTo',
  'Lua_ReceiveAnyKeyPress', 'CancelMapping', 'UpdatePlayerMenuIndex']);
let currentSwf = null;
let enabled = true;
Interceptor.attach(base.add(R.dispatcher), {
  onEnter(args) {
    if (root === null) { root = findRoot(args[0]); if (root) send({ev: 'root', root: root.toString()}); }
    if (uiThread === null) uiThread = Process.getCurrentThreadId();
    const n = args[3].toInt32();
    const a = [];
    for (let i = 0; i < Math.min(n, 4); i++) a.push(gfxArgStr(args[2].add(i * VSIZE)));
    const script = a[0] || '', fn = a[1] || '';
    this.fn = fn;
    if (fn === 'LoadDestination') { try { currentSwf = JSON.parse(a[2]).Destination; } catch (e) {} }
    else if (script && !HELPERS.has(script)) {
      const sw = SCRIPT2SWF[script] || (script + '.swf');
      if (sw === 'Popup.swf' && currentSwf !== 'Popup.swf') prevSwf = currentSwf;
      currentSwf = sw;
    }
    this.swf = currentSwf;
    send({ev: 'ei', script: script, fn: fn, json: a[2] || '', t: Date.now()});
  },
  onLeave(ret) {
    if (!enabled || !TRIGGERS.has(this.fn) || !root) return;
    let swf = this.swf;
    if (!swf) { swf = rootCallStr('root.GetActiveSWF'); if (swf) currentSwf = swf; }
    if (swf) { snapshot(swf, this.fn); schedule(swf, this.fn); }
  }
});
Interceptor.attach(base.add(R.rootInvoke), {
  onEnter(args) {
    if (root === null && args[0].readPointer().equals(VT_ROOT)) root = args[0];
    const n = args[4].toInt32();
    const a = [];
    for (let i = 0; i < Math.min(n, 3); i++) a.push(gfxArgStr(args[3].add(i * VSIZE)));
    const name = args[1].readUtf8String();
    this.inv = null;
    if (name === 'root.Invoke') {
      send({ev: 'inv', swf: a[0], fn: a[1], json: a[2], t: Date.now()}); this.inv = a;
      if (a[0] === 'Popup.swf' && a[1] === 'Close' && currentSwf === 'Popup.swf' && prevSwf) currentSwf = prevSwf;  // popup closed: cursor returns to the screen below
    }
    else send({ev: 'rootcall', name: name, args: a, t: Date.now()});
  },
  onLeave(ret) {
    if (!enabled || !root || inSnapshot) return;
    if (this.inv && INV_TRIGGERS.has(this.inv[1]) && SCREENS[this.inv[0]]) { snapshot(this.inv[0], 'inv:' + this.inv[1]); schedule(this.inv[0], 'inv:' + this.inv[1]); }
  }
});
rpc.exports = {
  enable(on) { enabled = !!on; return enabled; },
  state() { return {root: root ? root.toString() : null, currentSwf: currentSwf}; },
};
send({ev: 'ready', base: base.toString()});
