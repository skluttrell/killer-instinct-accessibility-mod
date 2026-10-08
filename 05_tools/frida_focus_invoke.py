"""C2 experiment: read a screen's focus index by calling its public AS3 GetSelectionIndex() in-process.

How the engine talks to AS3 (build 14306144): every Lua->UI call becomes
  AS3::MovieRoot::Invoke(root, "root.Invoke", NULL, Value[3]{swf, func, json}, 3)      (RVA 0x1122ef0, vtable 0x20a2928 slot 57)
where ForegroundShell.swf's ShellBase.Invoke(swf, func, json) looks the screen up and calls screen[func](json).
That returns void, so to read a value we do it in two steps, on the UI thread, inside the CallUILuaScript hook:
  1. MovieRoot::Invoke(root, "root.GetSWFRefFromString", &ref, Value[1]{swf}, 1)   -> ref = DisplayObject Value
  2. ref.pObjectInterface->Invoke(pData, &res, "GetSelectionIndex", NULL, 0, isdobj)   (AS3ValueObjectInterface slot 6, RVA 0x1150a00)
  3. Value::Release(&ref) (RVA 0x350bd0) to drop the managed reference.
GFx::Value layout here: 0x30 bytes, pObjectInterface +0x10, Type +0x18 (6 = string, 3 = int, 5 = number, 0xA = display
object, bit 0x40 = managed), payload +0x20.
root = [[[rcx_of_dispatcher] + 0x18] + 0x18] (rcx of 0x5dfe50 is the uiMovie wrapper of the foreground shell).
Nothing in game memory is patched; we only call read-only AS3 getters through the engine's own entry points.
Usage: python frida_focus_invoke.py <logfile> [seconds]
"""
import datetime
import sys
import time

import frida

LOG = open(sys.argv[1], "a", encoding="utf-8")
DURATION = float(sys.argv[2]) if len(sys.argv) > 2 else 240

JS = r"""
const mod = Process.findModuleByName('KILLERINSTINCTX64_R.EXE');
const base = mod.base;
send({ev: 'base', base: base.toString()});
const RVA_ROOT_INVOKE = 0x1122ef0, RVA_OBJ_INVOKE = 0x1150a00, RVA_VALUE_RELEASE = 0x350bd0;
const VT_MOVIEROOT = base.add(0x20a2928), VT_OBJIFACE = base.add(0x20a6768);
send({ev: 'check', slot57: base.add(0x20a2928 + 57 * 8).readPointer().sub(base).toString(), objInvokeSlot6: base.add(0x20a6768 + 6 * 8).readPointer().sub(base).toString()});
const RootInvoke = new NativeFunction(base.add(RVA_ROOT_INVOKE), 'uint8', ['pointer', 'pointer', 'pointer', 'pointer', 'uint32']);
const ObjInvoke = new NativeFunction(base.add(RVA_OBJ_INVOKE), 'uint8', ['pointer', 'pointer', 'pointer', 'pointer', 'pointer', 'uint64', 'uint8']);
const ValueRelease = new NativeFunction(base.add(RVA_VALUE_RELEASE), 'void', ['pointer']);

function safeStr(p, max) {
  try {
    if (p.isNull()) return null;
    const r = Process.findRangeByAddress(p);
    if (r === null || r.protection.indexOf('r') < 0) return null;
    const s = p.readUtf8String();
    return s === null ? null : s.substring(0, max || 300);
  } catch (e) { return null; }
}
function valueStr(p) {
  try {
    const type = p.add(0x18).readU32();
    let sp = p.add(0x20);
    if ((type >>> 6) & 1) sp = sp.readPointer();
    const kind = type & 0x8f;
    if (kind === 6 || kind === 7) return (sp.readPointer().readUtf8String());
    if (kind === 5) return 'num:' + p.add(0x20).readDouble();
    if (kind === 4) return 'uint:' + p.add(0x20).readU32();
    if (kind === 3) return 'int:' + p.add(0x20).readS32();
    if (kind === 2) return 'bool:' + p.add(0x20).readU8();
    if (kind === 1) return 'null';
    if (kind === 0) return 'undef';
    return 'type0x' + type.toString(16);
  } catch (e) { return '<err ' + e + '>'; }
}
// scratch Values (0x30 each): arg string, ref result, int result
const scratch = Memory.alloc(0x100);
const argV = scratch, refV = scratch.add(0x30), resV = scratch.add(0x60);
const nameGetRef = Memory.allocUtf8String('root.GetSWFRefFromString');
const nameGetSel = Memory.allocUtf8String('GetSelectionIndex');
const swfBufs = {};
function swfBuf(s) { if (!swfBufs[s]) swfBufs[s] = Memory.allocUtf8String(s); return swfBufs[s]; }
function zero(p, n) { p.writeByteArray(new Array(n).fill(0)); }

let root = null;
function findRoot(wrapper) {
  try {
    const x = wrapper.readPointer();           // uiMovie wrapper -> movie holder
    const inner = x.add(0x18).readPointer();   // holder -> inner object
    const z = inner.add(0x18).readPointer();   // inner -> AS3::MovieRoot
    if (z.readPointer().equals(VT_MOVIEROOT)) return z;
    send({ev: 'root_mismatch', wrapper: wrapper.toString(), vt: z.readPointer().sub(base).toString()});
  } catch (e) { send({ev: 'root_err', msg: String(e)}); }
  return null;
}

function queryFocus(swf, why) {
  if (!root) return;
  const t0 = Date.now();
  try {
    zero(argV, 0x30); argV.add(0x18).writeU32(6); argV.add(0x20).writePointer(swfBuf(swf));
    zero(refV, 0x30);
    const ok1 = RootInvoke(root, nameGetRef, refV, argV, 1);
    const refType = refV.add(0x18).readU32();
    const rec = {ev: 'focus', swf: swf, why: why, ok1: ok1, refType: '0x' + refType.toString(16)};
    const kind = refType & 0x8f;
    if (ok1 && (kind === 0x8 || kind === 0xA || kind === 0x9)) {
      const iface = refV.add(0x10).readPointer();
      const pdata = refV.add(0x20).readPointer();
      rec.iface_vt = iface.readPointer().sub(base).toString();
      zero(resV, 0x30);
      const ok2 = ObjInvoke(iface, pdata, resV, nameGetSel, ptr(0), 0, kind === 0xA ? 1 : 0);
      rec.ok2 = ok2; rec.value = valueStr(resV); rec.resType = '0x' + resV.add(0x18).readU32().toString(16);
      if (resV.add(0x18).readU32() & 0x40) ValueRelease(resV);
    }
    if (refType & 0x40) ValueRelease(refV);
    rec.ms = Date.now() - t0;
    send(rec);
  } catch (e) { send({ev: 'focus_err', swf: swf, msg: String(e)}); }
}

const nameGetActive = Memory.allocUtf8String('root.GetActiveSWF');
function activeSwf() {  // ShellBase.GetActiveSWF(): String  (the screen Lua last made active)
  if (!root) return null;
  try {
    zero(resV, 0x30);
    const ok = RootInvoke(root, nameGetActive, resV, ptr(0), 0);
    const t = resV.add(0x18).readU32();
    const s = ok && ((t & 0x8f) === 6 || (t & 0x8f) === 7) ? valueStr(resV) : null;
    if (t & 0x40) ValueRelease(resV);
    if (!s) send({ev: 'active_err', ok: ok, type: '0x' + t.toString(16)});
    return s;
  } catch (e) { send({ev: 'active_err', msg: String(e)}); return null; }
}
const SCRIPT2SWF = {MainMenu: 'MainMenu.swf', MessageBox: 'Popup.swf', CharSelectMenu: 'CharacterSelect.swf', OptionsMenu: 'OptionsMenu.swf', LandingPage: 'LandingPage.swf'};
let currentSwf = null;
let queryEnabled = true;
const TRIGGERS = new Set(['PlayScroll', 'CheckIsDestInstalled', 'ScreenShown']);

Interceptor.attach(base.add(0x5dfe50), {
  onEnter(args) {
    if (root === null) root = findRoot(args[0]);
    const n = args[3].toInt32();
    const out = [];
    for (let i = 0; i < Math.min(n, 4); i++) out.push(valueStr(args[2].add(i * 0x30)));
    this.fn = out[1] || '';
    send({ev: 'ei', args: out, tid: Process.getCurrentThreadId()});
    const script = out[0] || '';
    const isScreen = script && script !== 'uiSounds' && script !== 'uiScreenUtil';
    if (this.fn === 'LoadDestination') { try { currentSwf = JSON.parse(out[2]).Destination; } catch (e) {} }
    else if (isScreen) currentSwf = SCRIPT2SWF[script] || (script + '.swf');
    this.swf = currentSwf;
  },
  onLeave(ret) {
    if (!queryEnabled || !TRIGGERS.has(this.fn)) return;
    const swf = this.swf || activeSwf();
    if (swf) queryFocus(swf, this.fn + (this.swf ? '' : '/active'));
  }
});
Interceptor.attach(base.add(RVA_ROOT_INVOKE), {
  onEnter(args) {
    if (root === null && args[0].readPointer().equals(VT_MOVIEROOT)) { root = args[0]; send({ev: 'root_from_invoke', root: root.toString()}); }
    const n = args[4].toInt32();
    const av = [];
    for (let i = 0; i < Math.min(n, 3); i++) av.push(valueStr(args[3].add(i * 0x30)));
    send({ev: 'rootinvoke', self: args[0].toString(), name: safeStr(args[1], 80), nargs: n, args: av});
  }
});
rpc.exports = {
  setquery(on) { queryEnabled = !!on; return queryEnabled; },
  state() { return {root: root ? root.toString() : null, currentSwf: currentSwf}; }
};
send({ev: 'ready'});
"""


def log(line):
    ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
    LOG.write("%s %s\n" % (ts, line))
    LOG.flush()


def on_message(msg, data):
    if msg["type"] == "send":
        log(str(msg["payload"]))
    else:
        log("ERR " + str(msg))


session = frida.attach("KILLERINSTINCTX64_R.EXE")
script = session.create_script(JS)
script.on("message", on_message)
script.load()
log("attached; logging for %.0fs" % DURATION)
t0 = time.time()
try:
    while time.time() - t0 < DURATION:
        time.sleep(1)
finally:
    try:
        log("state: %s" % script.exports_sync.state())
    except Exception as e:
        log("state failed: %s" % e)
    session.detach()
    log("detached")
