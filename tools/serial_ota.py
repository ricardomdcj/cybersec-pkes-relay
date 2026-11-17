#!/usr/bin/env python3
"""
OTA serial para a bancada PKES: regrava uma CYD pela UART0 (CH340) sem download mode.

Pre-requisito: a placa ja precisa estar rodando um firmware desta bancada (com o
componente serial_ota), gravado uma vez via esptool (bootstrap). Dai em diante:

    python tools/serial_ota.py COM6 vehicle/build/vehicle.bin

Fecha o monitor/porta antes. Protocolo em components/serial_ota/serial_ota.h.

Robustez (CH340 a 115200 sem flow control):
 - CRC-16 por bloco quando o firmware anuncia "crc=1" (pega bit-flip no conteudo que o
   base64 nao ve); firmware antigo cai no modo legado (so base64), p/ o rollout funcionar.
 - reenvio do bloco em NAK/timeout, e retry da transferencia inteira em falha final.
"""
import sys
import time
import base64

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial nao encontrado. Use o python do ESP-IDF, ou: pip install pyserial")

CHUNK = 512        # bytes por bloco (vira ~684 ch em base64, cabe no LINE_MAX do firmware)
TRANSFER_TRIES = 4  # tentativas da transferencia inteira


def crc16(d):   # CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), igual ao firmware
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
                return None            # falha: deixa o chamador reiniciar a transferencia
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
    if not wait("OTA:READY", timeout=30):   # 1o ACK demora: apaga a particao OTA
        print("  sem OTA:READY")
        return False

    sent = resends = 0
    for i in range(0, size, CHUNK):
        chunk = data[i:i + CHUNK]
        b64 = base64.b64encode(chunk).decode()
        msg = ("OTA:D %04X %s" % (crc16(chunk), b64)) if use_crc else ("OTA:D " + b64)
        for _ in range(8):   # reenvia o bloco se corromper
            send(msg)
            r = wait_ack(timeout=(45 if i == 0 else 15))
            if r == "ACK":
                break
            if r == "ERR":
                print("\n  firmware abortou (ERR)")
                return False
            resends += 1
        else:
            print(f"\n  8 tentativas sem ACK em {sent} bytes")
            return False
        sent += len(chunk)
        print(f"\r  {sent * 100 // size:3d}%  ({sent}/{size} bytes)", end="", flush=True)
    print()

    send("OTA:END")
    ok = wait("OTA:OK", timeout=20)
    if ok:
        print(f"{ok}  ({resends} bloco(s) reenviado(s))")
    return bool(ok)


def main():
    if len(sys.argv) < 3:
        sys.exit("uso: serial_ota.py <porta> <firmware.bin> [baud]")
    port, path = sys.argv[1], sys.argv[2]
    baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

    with open(path, "rb") as f:
        data = f.read()
    size = len(data)

    s = serial.Serial(port, baud, timeout=2)
    time.sleep(0.3)
    s.reset_input_buffer()

    # Negocia CRC: firmware novo responde "crc=1"; antigo cai no modo legado.
    s.write(b"OTA?\n")
    use_crc = False
    t0 = time.time()
    while time.time() - t0 < 2:
        ln = s.readline().decode(errors="replace").strip()
        if ln.startswith("OTA:ID"):
            use_crc = "crc=1" in ln
            break

    print(f"enviando {path} ({size} bytes) para {port} @ {baud}  (crc={'on' if use_crc else 'off'})")
    for attempt in range(1, TRANSFER_TRIES + 1):
        if attempt > 1:
            print(f"-- tentativa {attempt}/{TRANSFER_TRIES} --")
            time.sleep(1.0)
        if transfer(s, data, size, use_crc):
            s.close()
            print("pronto: a placa reiniciou na imagem nova.")
            return
    s.close()
    sys.exit("falhou apos varias tentativas (linha serial muito ruidosa?)")


if __name__ == "__main__":
    main()
