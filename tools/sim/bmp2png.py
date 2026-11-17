"""Converts a 24-bit BMP from the simulator to PNG, so the frames open in any image viewer.
Stdlib only (no Pillow on the work machine).

Usage: python tools/sim/bmp2png.py <frame.bmp> [<frame.png>]
       (the PNG defaults to the same name with .png)
"""
import os
import struct
import sys
import zlib


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        raise ValueError("not a BMP file")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp != 24:
        raise ValueError("only 24-bit BMP is supported (got %d)" % bpp)
    bottom_up = height > 0
    height = abs(height)
    stride = (width * 3 + 3) & ~3
    rows = []
    for y in range(height):
        file_row = (height - 1 - y) if bottom_up else y
        start = offset + file_row * stride
        bgr = data[start:start + width * 3]
        rgb = bytearray(len(bgr))
        rgb[0::3] = bgr[2::3]
        rgb[1::3] = bgr[1::3]
        rgb[2::3] = bgr[0::3]
        rows.append(bytes(rgb))
    return width, height, rows


def png_chunk(tag, payload):
    return (struct.pack(">I", len(payload)) + tag + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))


def write_png(path, width, height, rows):
    raw = b"".join(b"\x00" + r for r in rows)   # filter type 0 on every scanline
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit RGB
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(png_chunk(b"IHDR", ihdr))
        f.write(png_chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(png_chunk(b"IEND", b""))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    src = argv[1]
    dst = argv[2] if len(argv) > 2 else os.path.splitext(src)[0] + ".png"
    width, height, rows = read_bmp(src)
    write_png(dst, width, height, rows)
    print("wrote", dst)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
