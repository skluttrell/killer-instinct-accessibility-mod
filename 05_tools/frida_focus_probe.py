"""Short discovery run: hook every GFx Sprite / AvmSprite vtable slot and log calls whose string arguments are
frame labels used for focus (GainFocus, LoseFocus, ...). Heavy on FPS; run for ~20 s only.
Usage: python frida_focus_probe.py <logfile> [seconds]
"""
import datetime
import sys
import time

import frida

LOG = open(sys.argv[1], "a", encoding="utf-8")
DURATION = float(sys.argv[2]) if len(sys.argv) > 2 else 20
JS = r"""
const mod = Process.findModuleByName('KILLERINSTINCTX64_R.EXE'); const base = mod.base;
const WANT = new Set(['GainFocus','LoseFocus','FocusedLoop','UnfocusedLoop','BuildOn','BuildOff','LoadingLoop','Default','Loop','Off','Selected']);
function rd(p) { try { if (p.isNull() || Process.findRangeByAddress(p) === null) return null; const s = p.readUtf8String(); return (s && s.length >= 2 && s.length < 64) ? s : null; } catch (e) { return null; } }
function str(p) {
  let s = rd(p); if (s) return s;
  try { const q = p.readPointer(); s = rd(q); if (s) return '*' + s; const r = q.readPointer(); s = rd(r); if (s) return '**' + s; } catch (e) {}
  return null;
}
function wstr(p) { try { if (p.isNull() || Process.findRangeByAddress(p) === null) return null; const s = p.readUtf16String(); return (s && s.length >= 2 && s.length < 64) ? s : null; } catch (e) { return null; } }
const tables = [[0x2049198 - 8*120, 'Sprite?'], [0x20a4d50, 'AvmSprite'], [0x20a78b0, 'AvmTextField']];
// Sprite primary vtable RVA: recompute from RTTI dump (offset 0 table printed first); use known value below
const SPRITE_VT = 0x2049198; // offset-24 table; primary table is right before it: find by scanning back for the COL pointer
let hooked = 0;
function hookTable(vt, n, name) {
  for (let i = 0; i < n; i++) {
    const fn = base.add(vt).add(i * 8).readPointer();
    if (fn.compare(base) < 0 || fn.compare(base.add(0x3000000)) > 0) break;
    const slot = i;
    try {
      Interceptor.attach(fn, { onEnter(args) {
        for (let k = 1; k <= 3; k++) {
          const s = str(args[k]) || wstr(args[k]);
          if (s && WANT.has(s.replace(/^\*+/, ''))) { send({ev: 'focus', t: name, slot: slot, fn: fn.sub(base).toString(), arg: k, s: s, self: args[0].toString()}); return; }
        }
      }});
      hooked++;
    } catch (e) {}
  }
}
hookTable(0x20a4d50, 34, 'AvmSprite');
hookTable(0x20a78b0, 33, 'AvmTextField');
// primary Sprite vtable: the RTTI dump printed its slots starting at 0xee48c0 ... locate by searching .rdata for that first slot pointer
const first = base.add(0xee48c0);
const rdata = Process.findModuleByName('KILLERINSTINCTX64_R.EXE').enumerateRanges('r--');
let found = null;
for (const r of rdata) { const hits = Memory.scanSync(r.base, r.size, first.toString(16).padStart(16, '0').match(/../g).reverse().join(' ')); if (hits.length) { found = hits[0].address; break; } }
if (found) { hookTable(found.sub(base).toInt32(), 120, 'Sprite'); send({ev: 'info', msg: 'Sprite vtable at ' + found.sub(base).toString()}); }
send({ev: 'ready', hooked: hooked});
"""
def log(line):
    LOG.write("%s %s\n" % (datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3], line)); LOG.flush()
def on_message(msg, data):
    log(str(msg.get("payload", msg)))
session = frida.attach("KILLERINSTINCTX64_R.EXE")
script = session.create_script(JS); script.on("message", on_message); script.load()
log("attached for %.0fs" % DURATION)
time.sleep(DURATION)
session.detach(); log("detached")
