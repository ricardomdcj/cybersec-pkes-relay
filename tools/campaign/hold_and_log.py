import argparse
import datetime as dt
import pathlib
import sys
import time

import serial


def stamp():
    return dt.datetime.now().strftime("%H:%M:%S.%f")[:-3]


def open_port(name, rts):
    port = serial.Serial()
    port.port = name
    port.baudrate = 115200
    port.timeout = 0.1
    port.dtr = False
    port.rts = rts
    port.open()
    return port


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hold", default="")
    ap.add_argument("--log", default="")
    ap.add_argument("--out", required=True)
    ap.add_argument("--stop", required=True)
    ap.add_argument("--max-s", type=float, default=1200.0)
    args = ap.parse_args()

    out = open(args.out, "w", encoding="utf-8", newline="\n")

    def say(msg):
        out.write(f"{stamp()} [HELPER] {msg}\n")
        out.flush()

    held = []
    for name in [p for p in args.hold.split(",") if p]:
        try:
            port = open_port(name, rts=True)
        except serial.SerialException as exc:
            say(f"ERROR holding {name}: {exc}")
            for _, p in held:
                p.rts = False
                p.close()
            out.close()
            return 1
        port.reset_input_buffer()
        held.append((name, port))
        say(f"held {name} in reset (RTS asserted)")

    logger = None
    if args.log:
        try:
            logger = open_port(args.log, rts=False)
            say(f"logging {args.log} passively (DTR/RTS idle)")
        except serial.SerialException as exc:
            say(f"ERROR logging {args.log}: {exc}")

    stop = pathlib.Path(args.stop)
    deadline = time.time() + args.max_s
    buf = b""
    while not stop.exists() and time.time() < deadline:
        for name, port in held:
            data = port.read(4096)
            if data:
                say(f"UNEXPECTED output from held {name}: {len(data)} bytes")
        if logger is not None:
            buf += logger.read(4096)
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                out.write(f"{stamp()} {raw.decode('utf-8', 'replace').rstrip(chr(13))}\n")
            out.flush()

    say("releasing held ports")
    for name, port in held:
        port.rts = False
        port.close()
    if logger is not None:
        logger.close()
    say("released")
    out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
