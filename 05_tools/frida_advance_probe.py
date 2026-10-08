"""Find the per-frame GFx MovieImpl::Advance slot: hook every slot of the MovieImpl vtable (RVA 0x203dd38, 71 slots)
for a few seconds and report call counts per slot and thread, plus the first argument as float.
Usage: python frida_advance_probe.py [seconds]   (run only one Frida session on the game at a time)
Read-only; temporary hooks only (expect a short FPS dip while they are attached).
"""
import sys
import time

import frida

SECS = float(sys.argv[1]) if len(sys.argv) > 1 else 3
JS = r"""
const mod = Process.findModuleByName('KILLERINSTINCTX64_R.EXE');
const base = mod.base;
const VT = 0x203dd38, N = 71;
const counts = {}, threads = {}, samples = {}, errors = {};
const seen = new Set();
rpc.exports = { report() { const out = {}; for (const i in counts) out[i] = {n: counts[i], threads: Array.from(threads[i]), rva: base.add(VT + i * 8).readPointer().sub(base).toString(), s: samples[i]}; out.errors = errors; return out; } };
for (let i = 0; i < N; i++) {
  const fn = base.add(VT + i * 8).readPointer();
  const key = fn.sub(base).toString();
  if (seen.has(key)) continue;  // several slots can share one function (pure virtual thunks)
  seen.add(key);
  counts[i] = 0; threads[i] = new Set(); samples[i] = [];
  try {
    Interceptor.attach(fn, { onEnter(args) {
      counts[i]++; threads[i].add(Process.getCurrentThreadId());
      if (samples[i].length < 2) samples[i].push({a1: args[1].toString(), a2: args[2].toString()});
      active[i] = (active[i] || 0) + 1;
    }, onLeave(r) { active[i]--; }});
  } catch (e) { errors[i] = String(e); }
}
// which slots are on the stack when AS3 calls into Lua (the dispatcher)? that slot is Advance.
const active = {}, enclosing = {};
Interceptor.attach(base.add(0x5dfe50), { onEnter(args) {
  const k = Object.keys(active).filter(i => active[i] > 0).join(',') + ' thread ' + Process.getCurrentThreadId();
  enclosing[k] = (enclosing[k] || 0) + 1;
}});
rpc.exports.enclosing = () => enclosing;
"""
session = frida.attach("KILLERINSTINCTX64_R.EXE")
script = session.create_script(JS)
script.load()
time.sleep(SECS)
rep = script.exports_sync.report()
print("dispatcher enclosing slots:", script.exports_sync.enclosing())
script.unload()
session.detach()
print("errors:", rep.pop("errors", {}))
for i, r in sorted(rep.items(), key=lambda kv: -kv[1]["n"]):
    if r["n"]:
        print("slot %2s rva %s calls %5d (%.0f/s) threads %s sample %s" % (i, r["rva"], r["n"], r["n"] / SECS, r["threads"], r["s"][:1]))
