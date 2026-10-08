"""Minimal ctypes wrapper for prism.dll (the Prism speech/screen-reader library, MPL-2.0, https://github.com/ethindp/prism).
Works on Python 3.10 (the official 'prismatoid' binding needs 3.11+). Uses the DLL shipped inside the prismatoid wheel.
Usage:
  python prism_ctypes.py            # list best backend, no speech
  python prism_ctypes.py "text"     # speak the text through the best backend (NVDA/JAWS/SAPI...)
"""
import ctypes as C
import os
import sys
from pathlib import Path

PRISM_OK = 0
PRISM_ERROR_ALREADY_INITIALIZED = 15  # per header enum order; only used leniently


def find_dll():
    for entry in sys.path + [str(Path.home() / "AppData/Roaming/Python/Python310/site-packages")]:
        p = Path(entry) / "prism" / "_native" / "prism.dll"
        if p.is_file():
            return p
    raise FileNotFoundError("prism.dll not found; pip install prismatoid or place prism.dll next to this script")


class PrismConfig(C.Structure):
    _fields_ = [
        ("version", C.c_uint8),
        ("registry", C.c_void_p),
        ("availability_callback", C.c_void_p),
        ("availability_userdata", C.c_void_p),
        ("availability_poll_interval_ms", C.c_uint32),
        ("availability_debounce_samples", C.c_uint32),
        ("availability_backoff_max_ms", C.c_uint32),
        ("availability_auto_power_manage", C.c_bool),
        ("availability_baseline_callback", C.c_void_p),
    ]


class Prism:
    def __init__(self, dll_path=None):
        p = Path(dll_path) if dll_path else find_dll()
        os.add_dll_directory(str(p.parent))
        self.lib = C.CDLL(str(p))
        L = self.lib
        L.prism_config_init.restype = PrismConfig
        L.prism_init.argtypes = [C.POINTER(PrismConfig)]
        L.prism_init.restype = C.c_void_p
        L.prism_shutdown.argtypes = [C.c_void_p]
        L.prism_registry_create_best.argtypes = [C.c_void_p]
        L.prism_registry_create_best.restype = C.c_void_p
        L.prism_backend_initialize.argtypes = [C.c_void_p]
        L.prism_backend_initialize.restype = C.c_int
        L.prism_backend_name.argtypes = [C.c_void_p]
        L.prism_backend_name.restype = C.c_char_p
        L.prism_backend_speak.argtypes = [C.c_void_p, C.c_char_p, C.c_bool]
        L.prism_backend_speak.restype = C.c_int
        L.prism_backend_output.argtypes = [C.c_void_p, C.c_char_p, C.c_bool]
        L.prism_backend_output.restype = C.c_int
        L.prism_backend_braille.argtypes = [C.c_void_p, C.c_char_p]
        L.prism_backend_braille.restype = C.c_int
        L.prism_backend_stop.argtypes = [C.c_void_p]
        L.prism_backend_stop.restype = C.c_int
        L.prism_backend_free.argtypes = [C.c_void_p]
        self.cfg = L.prism_config_init()
        self.ctx = L.prism_init(C.byref(self.cfg))
        if not self.ctx:
            raise RuntimeError("prism_init failed")
        self.backend = L.prism_registry_create_best(self.ctx)
        if not self.backend:
            raise RuntimeError("no usable speech backend")
        rc = L.prism_backend_initialize(self.backend)
        if rc not in (PRISM_OK, PRISM_ERROR_ALREADY_INITIALIZED):
            raise RuntimeError("backend init failed, PrismError %d" % rc)

    @property
    def name(self):
        return self.lib.prism_backend_name(self.backend).decode()

    def speak(self, text, interrupt=True):
        rc = self.lib.prism_backend_output(self.backend, text.encode("utf-8"), interrupt)
        if rc != PRISM_OK:
            raise RuntimeError("output failed, PrismError %d" % rc)

    def stop(self):
        self.lib.prism_backend_stop(self.backend)

    def close(self):
        if self.backend:
            self.lib.prism_backend_free(self.backend)
            self.backend = None
        if self.ctx:
            self.lib.prism_shutdown(self.ctx)
            self.ctx = None


if __name__ == "__main__":
    p = Prism()
    print("prism.dll:", find_dll())
    print("best backend:", p.name)
    if len(sys.argv) > 1:
        p.speak(" ".join(sys.argv[1:]))
        print("spoken")
    p.close()
