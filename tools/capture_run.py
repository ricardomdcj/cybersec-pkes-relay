#!/usr/bin/env python3
"""Captures one vehicle campaign from UART0 and writes its manifest (D-02 and D-03).

Each campaign goes to data/runs/<YYYYmmdd-HHMMSS>_<label>/, containing:
  capture_<port>.log   UART0 lines in UTF-8, prefixed with the host time
  manifest.json        label, distance, firmware commit (RUN_META), result summary

Usage (ESP-IDF Python; -I ignores neighbouring scripts):
  C:\\Espressif\\tools\\python\\v6.1\\venv\\Scripts\\python.exe -I tools/capture_run.py ^
      --port COM6 --label relay --dist 3.0 --start

  --start      waits for the key to be subscribed and then sends START (without it, the
               campaign does not run)
  --no-reset   do not pulse RTS (by default the board restarts and RUN_META enters the log)
  --max-s      maximum capture time in seconds (default 900)

Offline mode, rebuilds the manifest of a folder that was already captured:
  python -I tools/capture_run.py --manifest-only data/runs/<folder>

Before running: close any monitor that is holding the port (see TESTING.md, section 7).
"""

import argparse
import datetime as dt
import json
import pathlib
import re
import sys
import time

REPO = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_OUT = REPO / "data" / "runs"

RE_RUN_META = re.compile(r"RUN_META: (?:role|papel)=(\S+) fw=(\S+) build=(.+)$")
RE_CSVHDR_V2 = re.compile(r"CSVHDR v2")
RE_VEH_CSV = re.compile(r"(?:VEHICLE|VEICULO): CSV,(.*)$")
RE_RUN_LOG = re.compile(r"(?:VEHICLE|VEICULO): RUN label=")
SUBSCRIBED_MARKERS = ("subscribed to notify", "inscrito em notify")   # new and old firmware


def parse_log(lines):
    """Extracts what the manifest needs from a log. Tolerant of incomplete logs."""
    info = {
        "fw": None, "build": None, "csv_schema": None,
        "summary": None, "rows": 0, "rows_ok": 0, "rows_timeout": 0,
        "run_cmd_seen": False, "run_label_log": None, "run_dist_log": None,
    }
    for line in lines:
        m = RE_RUN_META.search(line)
        if m and info["fw"] is None and m.group(1) in ("vehicle", "veiculo"):
            info["fw"], info["build"] = m.group(2), m.group(3).strip()
        if RE_CSVHDR_V2.search(line):
            info["csv_schema"] = "v2"
        if RE_RUN_LOG.search(line):
            info["run_cmd_seen"] = True
            tail = re.split(r"RUN ", line, maxsplit=1)[1]
            for tok in tail.split():
                if tok.startswith("label="):
                    info["run_label_log"] = tok[6:]
                elif tok.startswith("dist="):
                    info["run_dist_log"] = tok[5:]
        m = RE_VEH_CSV.search(line)
        if m:
            fields = m.group(1).strip().split(",")
            info["rows"] += 1
            # v2: label,dist,round,nonce,t_tx,rtt,result,rssi,itvl,mtu  (result = [6])
            # v1: label,round,nonce,rtt,result,rssi                    (result = [4])
            result = fields[6] if len(fields) >= 10 else fields[4]
            if result in ("OK",):
                info["rows_ok"] += 1
            elif result == "TIMEOUT":
                info["rows_timeout"] += 1
        if " === " in line and ("VEHICLE:" in line or "VEICULO:" in line):
            info["summary"] = re.split(r"(?:VEHICLE|VEICULO): ", line, maxsplit=1)[1].strip()
    return info


def build_manifest(run_dir, port=None, label=None, dist=None, reset=None, cmds=None,
                   started=None, ended=None):
    """Writes manifest.json in the campaign folder, reading the capture_*.log files in it."""
    run_dir = pathlib.Path(run_dir)
    logs = sorted(run_dir.glob("capture_*.log"))
    lines = []
    for log in logs:
        lines += log.read_text(encoding="utf-8", errors="replace").splitlines()
    info = parse_log(lines)

    manifest_path = run_dir / "manifest.json"
    previous = {}
    if manifest_path.exists():
        previous = json.loads(manifest_path.read_text(encoding="utf-8"))

    summary_ok = info["summary"] is not None
    manifest = {
        "run_id": run_dir.name,
        "label": label or info["run_label_log"] or previous.get("label") or "?",
        "dist_m": dist or info["run_dist_log"] or previous.get("dist_m") or "-",
        "port": port or previous.get("port"),
        "started_local": started or previous.get("started_local"),
        "ended_local": ended or previous.get("ended_local"),
        "reset_rts": reset if reset is not None else previous.get("reset_rts"),
        "commands_sent": cmds if cmds is not None else previous.get("commands_sent", []),
        "firmware_commit": info["fw"] or previous.get("firmware_commit"),
        "firmware_build": info["build"] or previous.get("firmware_build"),
        "csv_schema": info["csv_schema"] or "v1",
        "run_cmd_seen_in_log": info["run_cmd_seen"],
        "rows_received": info["rows"],
        "rows_ok": info["rows_ok"],
        "rows_timeout": info["rows_timeout"],
        "summary_line": info["summary"],
        "complete": summary_ok,
        "captures": [p.name for p in logs],
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                             encoding="utf-8")
    return manifest


def capture(args):
    import serial  # pyserial from the ESP-IDF venv

    run_stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = pathlib.Path(args.out) / f"{run_stamp}_{args.label}"
    run_dir.mkdir(parents=True, exist_ok=True)
    log_path = run_dir / f"capture_{args.port}.log"
    started = dt.datetime.now().astimezone().isoformat(timespec="seconds")

    ser = serial.Serial(args.port, 115200, timeout=0.2)
    cmds = []
    if not args.no_reset:
        ser.setRTS(False)
        time.sleep(0.1)
        ser.setRTS(True)          # reset pulse: the boot (RUN_META) goes into the log

    buf = b""
    started_at = time.time()
    armed_at = time.time() + 4.0   # give the boot time to finish before sending commands
    run_sent = start_sent = subscribed = False
    summary_seen_at = None

    def send(f, text):
        ser.write((text + "\n").encode("utf-8"))
        cmds.append(text)
        f.write(f"{dt.datetime.now():%H:%M:%S.%f}[HOST] > {text}\n")
        f.flush()

    with open(log_path, "w", encoding="utf-8", newline="\n") as f:
        while True:
            now = time.time()
            if now - started_at > args.max_s:
                f.write(f"{dt.datetime.now():%H:%M:%S.%f}[HOST] maximum capture time reached\n")
                break
            chunk = ser.read(4096)
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.decode("utf-8", errors="replace").rstrip("\r")
                    f.write(f"{dt.datetime.now():%H:%M:%S.%f} {line}\n")
                    f.flush()
                    if any(marker in line for marker in SUBSCRIBED_MARKERS):
                        subscribed = True
                    if " === " in line and ("VEHICLE:" in line or "VEICULO:" in line):
                        summary_seen_at = now
            if now < armed_at:
                continue
            # RUN is always sent: it carries the label and the distance for the CSV and the display.
            if not run_sent:
                send(f, f"RUN label={args.label} dist={args.dist or '-'}")
                run_sent = True
            if args.start and not start_sent and subscribed:
                send(f, "START")
                start_sent = True
            if summary_seen_at and now - summary_seen_at > 2.0:
                break
    ser.close()
    ended = dt.datetime.now().astimezone().isoformat(timespec="seconds")
    m = build_manifest(run_dir, port=args.port, label=args.label,
                       dist=args.dist if args.dist else "-", reset=not args.no_reset,
                       cmds=cmds, started=started, ended=ended)
    print(f"campaign saved to {run_dir}")
    print(f"CSV rows: {m['rows_received']} (OK {m['rows_ok']}, TIMEOUT {m['rows_timeout']}); "
          f"summary: {'yes' if m['complete'] else 'NO (capture interrupted)'}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="COM port of the vehicle, e.g. COM6")
    ap.add_argument("--label", default="run", help="campaign label (e.g. baseline, relay)")
    ap.add_argument("--dist", help="distance in meters (e.g. 3.0)")
    ap.add_argument("--start", action="store_true", help="send START once the key is subscribed")
    ap.add_argument("--no-reset", action="store_true", help="do not pulse RTS at the start")
    ap.add_argument("--max-s", type=float, default=900.0, help="maximum capture time (s)")
    ap.add_argument("--out", default=str(DEFAULT_OUT), help="root folder of the campaigns")
    ap.add_argument("--manifest-only", metavar="FOLDER",
                    help="only rebuild manifest.json of a folder that was already captured")
    args = ap.parse_args(argv)

    if args.manifest_only:
        m = build_manifest(args.manifest_only)
        print(json.dumps(m, indent=2, ensure_ascii=False))
        return 0
    if not args.port:
        ap.error("--port is required to capture (or use --manifest-only)")
    return capture(args)


if __name__ == "__main__":
    sys.exit(main())
