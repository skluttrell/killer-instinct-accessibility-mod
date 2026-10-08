"""Trace the engine's AS3 invoke path (read-only). Hooks the game wrappers found by backtrace:
  0x1d6b30  wrapper(ctx, movieHolder*, name, result, [args, nargs])
  0x3c4fe0  wrapper(movieHolder, name, result, args, nargs) -> thread check -> 0xe47040
  0xe47040  thunk: rcx=[rcx+0x18]; jmp [[rcx]+0x1c8]  (vtable slot 57 of the inner object)
Logs the inner object, its vtable RVA and the resolved slot-57 target, then hooks that target too.
Usage: python frida_invoke_path.py <logfile> [seconds]
"""
import datetime
import sys
import time

import frida

LOG = open(sys.argv[1], "a", encoding="utf-8")
DURATION = float(sys.argv[2]) if len(sys.argv) > 2 else 90

JS = r"""
const mod = Process.findModuleByName('KILLERINSTINCTX64_R.EXE');
const base = mod.base;
function rva(p) { try { return p.compare(base) >= 0 && p.compare(base.add(mod.size)) < 0 ? '0x' + p.sub(base).toString(16) : ('!' + p.toString()); } catch (e) { return '?'; } }
function safeStr(p, max) {
  try { if (p.isNull()) return null; const r = Process.findRangeByAddress(p); if (r === null || r.protection.indexOf('r') < 0) return null;
        const s = p.readUtf8String(); return s === null ? null : s.substring(0, max || 200); } catch (e) { return null; }
}
const hooked = new Set();
let n1 = 0, n2 = 0, n3 = 0;
Interceptor.attach(base.add(0x1d6b30), { onEnter(args) {
  if (++n1 > 40) return;
  send({ev: 'w1', ctx: args[0].toString(), holder: args[1].toString(), name: safeStr(args[2]), res: args[3].toString(),
        a5: this.context.rsp.add(0x28).readPointer().toString(), nargs: this.context.rsp.add(0x30).readU32()});
}});
Interceptor.attach(base.add(0x3c4fe0), { onEnter(args) {
  if (++n2 > 40) return;
  let inner = null, vt = null, target = null;
  try { inner = args[0].add(0x18).readPointer(); const z = inner.add(0x18).readPointer(); vt = z.readPointer(); target = vt.add(0x1c8).readPointer();
        if (!hooked.has(target.toString())) { hooked.add(target.toString());
          Interceptor.attach(target, { onEnter(a) { if (++n3 > 40) return; send({ev: 'slot57', self: a[0].toString(), name: safeStr(a[1]), res: a[2].toString(), args: a[3].toString(), nargs: a[4].toInt32(), tid: Process.getCurrentThreadId()}); },
                                       onLeave(r) { if (n3 <= 40) send({ev: 'slot57_ret', ret: r.toInt32()}); } });
          send({ev: 'hooked', target: rva(target)}); }
  } catch (e) { send({ev: 'err', msg: String(e)}); }
  send({ev: 'w2', holder: args[0].toString(), ownerTid: args[0].add(0x48).readU32(), tid: Process.getCurrentThreadId(), name: safeStr(args[1]),
        inner: inner ? inner.toString() : null, vtable: vt ? rva(vt) : null, target: target ? rva(target) : null, nargs: this.context.rsp.add(0x28).readU32()});
}});
send({ev: 'ready'});
"""


def log(line):
    LOG.write("%s %s\n" % (datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], line))
    LOG.flush()


def on_message(msg, data):
    log(str(msg.get("payload", msg)))


session = frida.attach("KILLERINSTINCTX64_R.EXE")
script = session.create_script(JS)
script.on("message", on_message)
script.load()
log("attached for %.0fs" % DURATION)
time.sleep(DURATION)
session.detach()
log("detached")
