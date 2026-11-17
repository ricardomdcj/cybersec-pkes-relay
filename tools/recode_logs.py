#!/usr/bin/env python3
"""Writes UTF-8 (no BOM) copies of capture logs (D-01). The originals are not touched.

The older captures in data/ are UTF-16 (with BOM) or UTF-8 with BOM, which breaks grep and
diff. The copies keep the same names and line endings, only the encoding changes.

Usage: python -I tools/recode_logs.py data/*.log --out data/utf8
"""

import argparse
import pathlib
import sys


def decode(raw):
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):          # UTF-16 with BOM
        return raw.decode("utf-16")
    if raw[:3] == b"\xef\xbb\xbf":                      # UTF-8 with BOM
        return raw[3:].decode("utf-8")
    return raw.decode("utf-8")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", help="capture logs to convert")
    ap.add_argument("--out", required=True, help="folder for the UTF-8 copies")
    args = ap.parse_args(argv)

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    status = 0
    for name in args.files:
        src = pathlib.Path(name)
        try:
            text = decode(src.read_bytes())
        except UnicodeDecodeError as e:
            print(f"error: {src}: {e}", file=sys.stderr)
            status = 1
            continue
        dst = out / src.name
        with open(dst, "w", encoding="utf-8", newline="") as fh:   # newline="" keeps CRLF
            fh.write(text)
        print(f"{src} -> {dst} ({len(text)} chars)")
    return status


if __name__ == "__main__":
    sys.exit(main())
