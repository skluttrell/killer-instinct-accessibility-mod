"""Backtrace probe (read-only): find the engine's AS3-invoke path and the queue consumer.
Hooks only cheap functions:
  0x5dfe50  CallUILuaScript dispatcher  -> on selected events, log a backtrace (RVAs). The AS3 code that
            calls CallUILuaScript (ScreenShown, PlayScroll...) runs because the engine invoked an AS3 method,
            so the stack shows game handler -> GFx Invoke -> AS3 VM -> ExternalInterface -> dispatcher.
  0x65a950  uiInvokeEvent ctor          -> log event pointer, swf, func
  0x668660  uiInvokeEvent dtor (vtable slot 7) -> one backtrace per distinct return address (the consumer)
Usage: python frida_bt_probe.py <logfile> [seconds]
"""
import datetime
import sys
import time

import frida

LOG = open(sys.argv[1], "a", encoding="utf-8")
DURATION = float(sys.argv[2]) if len(sys.argv) > 2 else 120

JS = r"""
const mod = Process.findModuleByName('KILLERINSTINCTX64_R.EXE');
const base = mod.base;
send({ev: 'base', base: base.toString()});
function rva(p) { try { return p.compare(base) >= 0 && p.compare(base.add(mod.size)) < 0 ? '0x' + p.sub(base).toString(16) : ('!' + p.toString()); } catch (e) { return '?'; } }
function bt(ctx) {
  let a = [];
  try { a = Thread.backtrace(ctx, Backtracer.ACCURATE); } catch (e) {}
  if (a.length < 3) { try { a = Thread.backtrace(ctx, Backtracer.FUZZY); } catch (e) {} }
  return a.map(rva);
}
function safeStr(p, max) {
  try {
    if (p.isNull()) return null;
    const r = Process.findRangeByAddress(p);
    if (r === null || r.protection.indexOf('r') < 0) return null;
    let s = p.readUtf8String();
    if (s === null) return null;
    return s.substring(0, max || 300);
  } catch (e) { return null; }
}
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
const WANT = new Set(['ScreenShown', 'PlayScroll', 'CheckIsDestInstalled', 'PlayBack', 'LoadDestination']);
const btSeen = {};
Interceptor.attach(base.add(0x5dfe50), {
  onEnter(args) {
    const n = args[3].toInt32();
    const out = [];
    for (let i = 0; i < Math.min(n, 6); i++) out.push(gfxValueStr(args[2].add(i * 0x30)));
    const key = out[1] || '';
    const rec = {ev: 'ei', argc: n, args: out, rcx: args[0].toString(), rdx: args[1].toString()};
    if (WANT.has(key) && (btSeen[key] = (btSeen[key] || 0) + 1) <= 2) rec.bt = bt(this.context);
    send(rec);
  }
});
Interceptor.attach(base.add(0x65a950), {
  onEnter(args) {
    send({ev: 'ctor', self: args[0].toString(), swf: safeStr(args[2], 80), func: safeStr(args[3], 80)});
  }
});
const dtorSeen = new Set();
Interceptor.attach(base.add(0x668660), {
  onEnter(args) {
    const ra = rva(this.returnAddress);
    let swf = null, func = null;
    try { // string fields: DHStd::string at +0x40 (swf) and +0x68 (func); inline buffer or pointer when cap >= 0x10
      function dstr(p) { const cap = p.add(0x18).readU64().toNumber(); return (cap >= 0x10 ? p.readPointer() : p).readUtf8String(); }
      swf = dstr(args[0].add(0x40)); func = dstr(args[0].add(0x68));
    } catch (e) {}
    const rec = {ev: 'dtor', self: args[0].toString(), flags: args[1].toInt32(), ra: ra, swf: swf, func: func};
    if (!dtorSeen.has(ra)) { dtorSeen.add(ra); rec.bt = bt(this.context); }
    send(rec);
  }
});
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
    session.detach()
    log("detached")
