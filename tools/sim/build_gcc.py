"""Builds the PKES screen simulator with the msys64 gcc (the work machine has no MSVC).

The simulator compiles the real components/cyd/cyd_ui.c and LVGL 8.3 for the PC. The hardware
(display, LED, timers) is replaced by tools/sim/sim_hal.c. LVGL and its options come from the
vehicle build, so run "idf.py build" in vehicle/ once before this script.

Usage (from the pkes-relay folder):
    python tools/sim/build_gcc.py [--config <path to sdkconfig.h>] [--clean]

Output: tools/sim/build/pkes_sim.exe (and tools/sim/out/, where the scenario frames go).
"""
import argparse
import glob
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
LVGL = os.path.join(ROOT, "vehicle", "managed_components", "lvgl__lvgl")
CYD = os.path.join(ROOT, "components", "cyd")
OUT = os.path.join(HERE, "build")
GCC = os.environ.get("PKES_SIM_GCC", r"C:\msys64\ucrt64\bin\gcc.exe")

# The UI code under test is the device's own file. display/LED are replaced by sim_hal.c.
OWN = [os.path.join(CYD, "cyd_ui.c"),
       os.path.join(HERE, "sim_hal.c"),
       os.path.join(HERE, "sim_main.c")]

COMMON = ["-std=gnu17", "-O1",
          '-DLV_CONF_KCONFIG_EXTERNAL_INCLUDE="sim_sdkconfig.h"',
          "-DLV_LVGL_H_INCLUDE_SIMPLE"]


def find_config(explicit):
    local = os.environ.get("LOCALAPPDATA", "")
    candidates = [explicit] if explicit else [
        os.path.join(ROOT, "vehicle", "build", "config", "sdkconfig.h"),
        os.path.join(local, "pkes_build", "vehicle_esp32", "config", "sdkconfig.h"),
    ]
    for c in candidates:
        if c and os.path.exists(c):
            return c
    sys.exit("sdkconfig.h not found. Run 'idf.py build' in vehicle/ first, or pass --config.")


def write_lv_config(src, dst):
    """LVGL options from the device build. The LVGL pool is doubled because pointers and
    some structures are bigger on the PC: the simulator does not measure device memory."""
    with open(src, encoding="utf-8") as f:
        lv = [ln for ln in f if ln.startswith("#define CONFIG_LV_")]
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write("#pragma once\n")
        for ln in lv:
            if ln.startswith("#define CONFIG_LV_MEM_SIZE_KILOBYTES "):
                ln = "#define CONFIG_LV_MEM_SIZE_KILOBYTES %d\n" % (2 * int(ln.split()[-1]))
            f.write(ln)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", help="sdkconfig.h of a vehicle build")
    ap.add_argument("--clean", action="store_true", help="rebuild every object")
    args = ap.parse_args()

    os.environ["PATH"] = r"C:\msys64\ucrt64\bin;" + os.environ["PATH"]
    obj_dir = os.path.join(OUT, "obj")
    if args.clean and os.path.isdir(obj_dir):
        for o in glob.glob(os.path.join(obj_dir, "*.o")):
            os.remove(o)
    os.makedirs(obj_dir, exist_ok=True)
    os.makedirs(os.path.join(HERE, "out"), exist_ok=True)

    cfg_src = find_config(args.config)
    lv_cfg = os.path.join(OUT, "sim_sdkconfig.h")
    write_lv_config(cfg_src, lv_cfg)
    cfg_mtime = os.path.getmtime(lv_cfg)

    lvgl_srcs = sorted(glob.glob(os.path.join(LVGL, "src", "**", "*.c"), recursive=True))
    jobs = [(s, True) for s in lvgl_srcs] + [(s, False) for s in OWN]
    inc = [os.path.join(HERE, "stubs"), HERE, OUT, LVGL, CYD]

    def compile_one(job):
        src, is_lvgl = job
        name = os.path.relpath(src, ROOT).replace(os.sep, "_").replace("/", "_") + ".o"
        obj = os.path.join(obj_dir, name)
        if is_lvgl and os.path.exists(obj) and os.path.getmtime(obj) >= max(
                os.path.getmtime(src), cfg_mtime):
            return obj, "", 0
        flags = COMMON + (["-w"] if is_lvgl else
                          ["-Wall", "-Wno-unused-function", "-Wno-unused-variable",
                           "-Wno-unused-but-set-variable"])
        cmd = [GCC] + flags + ["-I" + i for i in inc] + ["-c", src, "-o", obj]
        r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
        return obj, r.stderr, r.returncode

    with ThreadPoolExecutor(8) as ex:
        results = list(ex.map(compile_one, jobs))
    failed = False
    for obj, err, rc in results:
        if err.strip():
            print(err[:3000])
        failed = failed or rc != 0

    exe = os.path.join(OUT, "pkes_sim.exe")
    r = subprocess.run([GCC, "-o", exe] + [o for o, _, _ in results] + ["-lm"],
                       capture_output=True, text=True, errors="replace")
    if r.returncode or failed:
        print(r.stderr[:3000] or "compile failed")
        return 1
    print("ok:", exe)
    return 0


if __name__ == "__main__":
    sys.exit(main())
