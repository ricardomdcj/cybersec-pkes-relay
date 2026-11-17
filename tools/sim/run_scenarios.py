"""Runs every scenario in tools/sim/scenarios/ and writes the frames to tools/sim/out/
(BMP as the simulator writes them, plus a PNG of each frame for viewing).

Usage (from the pkes-relay folder, after tools/sim/build_gcc.py):
    python tools/sim/run_scenarios.py [scenario ...]
"""
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from bmp2png import read_bmp, write_png  # noqa: E402

EXE = os.path.join(HERE, "build", "pkes_sim.exe")
OUT = os.path.join(HERE, "out")


def main(argv):
    if not os.path.exists(EXE):
        sys.exit("pkes_sim.exe not found: run tools/sim/build_gcc.py first")
    os.makedirs(OUT, exist_ok=True)
    names = argv[1:] or [os.path.splitext(os.path.basename(p))[0]
                         for p in sorted(glob.glob(os.path.join(HERE, "scenarios", "*.txt")))]
    status = 0
    for name in names:
        script = os.path.join(HERE, "scenarios", name + ".txt")
        r = subprocess.run([EXE, script, OUT], capture_output=True, text=True)
        with open(os.path.join(OUT, name + ".log"), "w", encoding="utf-8") as f:
            f.write(r.stdout + r.stderr)
        if r.returncode:
            status = 1
            print("FAILED", name, "-", (r.stderr or r.stdout).strip().splitlines()[-1:])
            continue
        frames = [l.split(" ", 1)[1] for l in r.stdout.splitlines() if l.startswith("wrote ")]
        for bmp in frames:
            width, height, rows = read_bmp(bmp)
            write_png(os.path.splitext(bmp)[0] + ".png", width, height, rows)
        print("%-18s %d frames" % (name, len(frames)))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
