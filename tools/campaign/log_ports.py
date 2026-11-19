import argparse
import datetime as dt
import pathlib
import sys
import time

import serial


def stamp():
    return dt.datetime.now().strftime("%H:%M:%S.%f")[:-3]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ports", required=True, help="COM5:RELAY_A,COM6:KEY,...")
    ap.add_argument("--pulse", default="", help="ports to reset once after opening")
    ap.add_argument("--cmd", default="", help="file with 'PORT text' lines; text is sent, or PULSE resets")
    ap.add_argument("--out", required=True)
    ap.add_argument("--stop", required=True)
    ap.add_argument("--max-s", type=float, default=1200.0)
    args = ap.parse_args()

    out = open(args.out, "w", encoding="utf-8", newline="\n")
    readers = []
    opened = {}
    for item in args.ports.split(","):
        name, tag = item.split(":")
        port = serial.Serial()
        port.port = name
        port.baudrate = 115200
        port.timeout = 0.05
        port.dtr = False
        port.rts = False
        try:
            port.open()
        except serial.SerialException as exc:
            out.write(f"{stamp()} [LOGGER] ERROR {name}: {exc}\n")
            out.flush()
            continue
        readers.append([tag, port, b""])
        opened[name] = port
        out.write(f"{stamp()} [LOGGER] {name} logged as {tag} (DTR/RTS idle)\n")
        out.flush()

    def pulse(name, why):
        opened[name].rts = True
        time.sleep(0.2)
        opened[name].rts = False
        out.write(f"{stamp()} [LOGGER] reset pulse on {name} ({why})\n")
        out.flush()

    for name in [p for p in args.pulse.split(",") if p]:
        if name in opened:
            pulse(name, "start")

    cmd_path = pathlib.Path(args.cmd) if args.cmd else None
    stop = pathlib.Path(args.stop)
    deadline = time.time() + args.max_s
    while not stop.exists() and time.time() < deadline:
        if cmd_path is not None and cmd_path.exists():
            lines = cmd_path.read_text(encoding="ascii").splitlines()
            cmd_path.unlink()
            for ln in lines:
                parts = ln.strip().split(" ", 1)
                if len(parts) != 2:
                    continue
                name, text = parts
                if name not in opened:
                    continue
                if text == "PULSE":
                    pulse(name, "command")
                    continue
                opened[name].write((text + "\n").encode("ascii"))
                opened[name].flush()
                out.write(f"{stamp()} [LOGGER] sent '{text}' to {name}\n")
                out.flush()
        for r in readers:
            data = r[1].read(4096)
            if data:
                r[2] += data
                while b"\n" in r[2]:
                    raw, r[2] = r[2].split(b"\n", 1)
                    line = raw.decode("utf-8", "replace").rstrip(chr(13))
                    out.write(f"{stamp()} [{r[0]}] {line}\n")
                out.flush()

    for r in readers:
        r[1].close()
    out.write(f"{stamp()} [LOGGER] closed\n")
    out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
