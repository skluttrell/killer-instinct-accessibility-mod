"""Phase 0 C live discovery with Frida (read-only hooks; never writes game memory).
Hooks (build 14306144, exe SHA-256 33bbd291...):
  loc_get   0x1a9640  LocalizationManager::GetString(this, uint32 crc32(lowercase key)) -> const char*
  loc_key   0x1a97e0  key-string variant used by Lua GetDisplayStringForKey
  lua_glob  0x1214e80 lookup of a Lua global by name (name in rdx)
  console   0x5dfe50  CallUILuaScript console command handler
  movie[i]  Scaleform GFx MovieImpl vtable (RVA 0x203dd38), 71 slots: log slot + string-looking args
Usage: python frida_discover.py <logfile> [seconds]
"""
import datetime
import sys
import time

import frida

LOG = open(sys.argv[1], "a", encoding="utf-8")
DURATION = float(sys.argv[2]) if len(sys.argv) > 2 else 600

JS = r"""
const MOD = 'KILLERINSTINCTX64_R.EXE';
const ENABLE_VT_HOOKS = false; // per-frame vtable hooks drop the game to ~2 FPS; enable only for targeted discovery
const mod = Process.findModuleByName(MOD);
if (mod === null) { throw new Error('module not found'); }
const base = mod.base;
send({ev: 'base', base: base.toString()});

function safeStr(p, max) {
  try {
    if (p.isNull()) return null;
    const r = Process.findRangeByAddress(p);
    if (r === null || r.protection.indexOf('r') < 0) return null;
    let s = p.readUtf8String();  // reads up to the NUL terminator
    if (s === null || s.length < 2) return null;
    s = s.substring(0, max || 2000);
    for (let i = 0; i < Math.min(s.length, 8); i++) { const c = s.charCodeAt(i); if (c < 0x20 || c > 0x7e) return null; }
    return s;
  } catch (e) { return null; }
}

const seenLoc = new Set();
Interceptor.attach(base.add(0x1a9640), {
  onEnter(args) { this.hash = args[1].toInt32() >>> 0; },
  onLeave(ret) {
    const key = this.hash.toString(16);
    if (seenLoc.has(key)) return;
    seenLoc.add(key);
    let s = null, hex = null;
    try { s = ret.readUtf8String(); } catch (e) {}
    try { hex = hexdump(ret, {length: 24, header: false, ansi: false}); } catch (e) {}
    send({ev: 'loc', hash: key, text: s, hex: hex});
  }
});
Interceptor.attach(base.add(0x1a97e0), {
  onEnter(args) { this.k = safeStr(args[1], 120); },
  onLeave(ret) { send({ev: 'lockey', key: this.k, text: safeStr(ret, 200)}); }
});
Interceptor.attach(base.add(0x1214e80), {
  onEnter(args) { const n = safeStr(args[1], 120); if (n) send({ev: 'luaglob', name: n}); }
});
// ExternalInterface.call('CallUILuaScript', ...) dispatcher: r8 = GFx::Value[] (stride 0x30), r9 = argc
function gfxValueStr(p) {
  try {
    const type = p.add(0x18).readU32();
    let sp = p.add(0x20);
    if ((type >>> 6) & 1) sp = sp.readPointer();
    const kind = type & 0x8f;
    if (kind === 6 || kind === 7) return (sp.readPointer().readUtf8String());
    if (kind === 4) return 'num:' + p.add(0x20).readDouble();
    if (kind === 3) return 'int:' + p.add(0x20).readS32();
    if (kind === 2) return 'bool:' + p.add(0x20).readU8();
    if (kind === 1) return 'null';
    return 'type' + kind;
  } catch (e) { return '<err ' + e + '>'; }
}
Interceptor.attach(base.add(0x5dfe50), {
  onEnter(args) {
    const n = args[3].toInt32();
    const out = [];
    for (let i = 0; i < Math.min(n, 6); i++) out.push(gfxValueStr(args[2].add(i * 0x30)));
    send({ev: 'ei', argc: n, args: out});
  }
});
// AS3ValueObjectInterface vtable: Invoke/GetMember/SetMember on AS3 objects
const VT2 = 0x20a6768, N2 = 40, counts2 = {}, strCounts2 = {};
for (let i = 0; ENABLE_VT_HOOKS && i < N2; i++) {
  const fn = base.add(VT2).add(i * 8).readPointer();
  const slot = i;
  try {
    Interceptor.attach(fn, {
      onEnter(args) {
        counts2[slot] = (counts2[slot] || 0) + 1;
        const ss = [];
        for (let k = 1; k <= 5; k++) { const v = safeStr(args[k]); if (v) ss.push(k + ':' + v); }
        if (ss.length) { strCounts2[slot] = (strCounts2[slot] || 0) + 1; if (strCounts2[slot] > 300) return; }
        if (ss.length || counts2[slot] <= 4) {
          let hx = '';
          for (let k = 1; k <= 4; k++) { try { hx += ' a' + k + '=' + args[k].toString(); } catch (e) {} }
          send({ev: 'as3obj', slot: slot, fn: fn.sub(base).toString(), s: ss.join(' | '), hx: hx, n: counts2[slot]});
        }
      }
    });
  } catch (e) { send({ev: 'err', slot: 'obj' + slot, msg: String(e)}); }
}

// MovieImpl vtable
const VT = 0x203dd38, NSLOTS = 71;
const counts = {};
let slotLogging = true;
for (let i = 0; ENABLE_VT_HOOKS && i < NSLOTS; i++) {
  const fn = base.add(VT).add(i * 8).readPointer();
  const slot = i;
  try {
    Interceptor.attach(fn, {
      onEnter(args) {
        if (!slotLogging) return;
        counts[slot] = (counts[slot] || 0) + 1;
        const a1 = safeStr(args[1]), a2 = safeStr(args[2]), a3 = safeStr(args[3]);
        if ((a1 || a2 || a3) && counts[slot] < 100000)
          send({ev: 'movie', slot: slot, fn: fn.sub(base).toString(), a1: a1, a2: a2, a3: a3, n: counts[slot]});
      }
    });
  } catch (e) { send({ev: 'err', slot: slot, msg: String(e)}); }
}
const VT3 = 0x20a2928, N3 = 40, counts3 = {};
for (let i = 0; ENABLE_VT_HOOKS && i < N3; i++) {
  const fn = base.add(VT3).add(i * 8).readPointer();
  const slot = i;
  try {
    Interceptor.attach(fn, {
      onEnter(args) {
        counts3[slot] = (counts3[slot] || 0) + 1;
        const ss = [];
        for (let k = 1; k <= 5; k++) { const v = safeStr(args[k]); if (v) ss.push(k + ':' + v); }
        if (ss.length && counts3[slot] > 300) return;
        if (ss.length || counts3[slot] <= 2) send({ev: 'mroot', slot: slot, fn: fn.sub(base).toString(), s: ss.join(' | '), n: counts3[slot]});
      }
    });
  } catch (e) { send({ev: 'err', slot: 'mroot' + slot, msg: String(e)}); }
}
// Lua -> UI: QueueInvokeEventToWindow(windowId, ?, strA, strB) and the uiInvokeEvent constructor
function strOrDeref(p, max) {
  let v = safeStr(p, max || 600);
  if (v) return v;
  try { const q = p.readPointer(); v = safeStr(q, max || 600); if (v) return '*' + v; } catch (e) {}
  return null;
}
[[0x5d9680, 'QueueInvokeEventToWindow'], [0x5d97d0, 'QueueInvokeEventToWindowTblFunc'], [0x65a950, 'uiInvokeEvent::ctor']].forEach(function (h) {
  Interceptor.attach(base.add(h[0]), {
    onEnter(args) {
      function hd(p) { try { if (Process.findRangeByAddress(p) === null) return null; return hexdump(p, {length: 48, header: false, ansi: false}); } catch (e) { return null; } }
      send({ev: 'qinv', fn: h[1], a0: args[0].toString(), a1: args[1].toString(), a2: args[2].toString(), a3: args[3].toString(), s2: strOrDeref(args[2]), s3: strOrDeref(args[3]), s4: strOrDeref(this.context.rsp.add(0x28).readPointer(), 60000)});
    }
  });
});
rpc.exports = {
  counts() { return {movie: counts, as3obj: counts2, mroot: counts3}; },
  setslot(on) { slotLogging = !!on; return slotLogging; },
  resetloc() { seenLoc.clear(); return true; }
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
        log("slot counts: %s" % script.exports_sync.counts())
    except Exception as e:
        log("counts failed: %s" % e)
    session.detach()
    log("detached")
