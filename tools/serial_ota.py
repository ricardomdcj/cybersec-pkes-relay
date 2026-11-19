#!/usr/bin/env python3
"""
Serial OTA for the PKES bench: reflashes a CYD over UART0 (CH340) without download mode.

Prerequisite: the board must already be running a firmware from this bench (with the
serial_ota component), flashed once through esptool (bootstrap). From then on:

    python tools/serial_ota.py COM6 vehicle/build/vehicle.bin

Close the monitor/port first. The protocol is described in components/serial_ota/serial_ota.h.

Robustness (CH340 at 115200 without flow control):
 - CRC-16 per block when the firmware announces "crc=1" (catches bit flips in the content that
   base64 does not see); older firmware falls back to the legacy mode (base64 only), so the
   rollout works.
 - the block is resent on NAK/timeout, and the whole transfer is retried on a final failure.
"""
import sys
import time
import base64

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial not found. Use the ESP-IDF Python, or: pip install pyserial")

CHUNK = 512        # bytes per block (about 684 characters in base64, fits the firmware's LINE_MAX)
TRANSFER_TRIES = 4  # attempts for the whole transfer


def crc16(d):   # CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), same as the firmware
    c = 0xFFFF
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if (c & 0x8000) else (c << 1) & 0xFFFF
    return c


def transfer(s, data, size, use_crc):
    def send(line):
        s.write((line + "\n").encode())

    def wait(prefix, timeout):
        t0 = time.time()
        while time.time() - t0 < timeout:
            ln = s.readline().decode(errors="replace").strip()
            if ln.startswith(prefix):
                return ln
            if ln.startswith("OTA:ERR"):
                return None            # failure: the caller restarts the transfer
        return None

    def wait_ack(timeout):
        t0 = time.time()
        while time.time() - t0 < timeout:
            ln = s.readline().decode(errors="replace").strip()
            if ln.startswith("OTA:ACK"):
                return "ACK"
            if ln.startswith("OTA:NAK"):
                return "NAK"
            if ln.startswith("OTA:ERR"):
                return "ERR"
        return None

    s.reset_input_buffer()
    send(f"OTA:BEGIN {size}")
    if not wait("OTA:READY", timeout=30):   # the first ACK is slow: it erases the OTA partition
        print("  no OTA:READY")
        return False

    sent = resends = 0
    for i in range(0, size, CHUNK):
        chunk = data[i:i + CHUNK]
        b64 = base64.b64encode(chunk).decode()
        msg = ("OTA:D %04X %s" % (crc16(chunk), b64)) if use_crc else ("OTA:D " + b64)
        for _ in range(8):   # resend the block if it gets corrupted
            send(msg)
            r = wait_ack(timeout=(45 if i == 0 else 15))
            if r == "ACK":
                break
            if r == "ERR":
                print("\n  firmware aborted (ERR)")
                return False
            resends += 1
        else:
            print(f"\n  8 attempts without ACK after {sent} bytes")
            return False
        sent += len(chunk)
        print(f"\r  {sent * 100 // size:3d}%  ({sent}/{size} bytes)", end="", flush=True)
    print()

    send("OTA:END")
    ok = wait("OTA:OK", timeout=20)
    if ok:
        print(f"{ok}  ({resends} block(s) resent)")
    return bool(ok)


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: serial_ota.py <port> <firmware.bin> [baud]")
    port, path = sys.argv[1], sys.argv[2]
    baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

    with open(path, "rb") as f:
        data = f.read()
    size = len(data)

    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, baud, 2
    s.dtr = False   # pyserial asserts DTR and RTS on open; that would reset the board
    s.rts = False
    s.open()
    time.sleep(0.3)
    s.reset_input_buffer()

    # Negotiate CRC: the current firmware answers "crc=1"; older firmware falls back to legacy mode.
    s.write(b"OTA?\n")
    use_crc = False
    t0 = time.time()
    while time.time() - t0 < 2:
        ln = s.readline().decode(errors="replace").strip()
        if ln.startswith("OTA:ID"):
            use_crc = "crc=1" in ln
            break

    print(f"sending {path} ({size} bytes) to {port} @ {baud}  (crc={'on' if use_crc else 'off'})")
    for attempt in range(1, TRANSFER_TRIES + 1):
        if attempt > 1:
            print(f"-- attempt {attempt}/{TRANSFER_TRIES} --")
            time.sleep(1.0)
        if transfer(s, data, size, use_crc):
            s.close()
            print("done: the board rebooted into the new image.")
            return
    s.close()
    sys.exit("failed after several attempts (is the serial line too noisy?)")


if __name__ == "__main__":
    main()
