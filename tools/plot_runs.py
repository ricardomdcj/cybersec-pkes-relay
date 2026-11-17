#!/usr/bin/env python3
"""Draws SVG charts from the rounds of one or more campaigns (D-05a and D-05b).

Input: rounds.csv written by tools/analyze_runs.py. Output, in --out (default: next to rounds.csv):
  rtt_timeseries.svg  RTT per round, one panel per condition, coloured by result (OK, FAIL,
                      TIMEOUT). TIMEOUT rounds are drawn as a cross on the baseline.
  rtt_histogram.svg   distribution of the answered RTTs, one colour per condition, as a % of
                      each condition's answered rounds (so conditions with different n compare)
  rtt_boxplot.svg     box plot per condition: median, quartiles, whiskers at 1.5 IQR, outliers

Stdlib only: the SVG is written as text, so no matplotlib is needed. All times are in milliseconds.
The quartiles use the same linear interpolation as tools/analyze_runs.py, so the charts and
summary.md agree.

Usage: python -I tools/plot_runs.py data/analysis/rounds.csv [--out data/analysis/plots]
"""

import argparse
import csv
import html
import math
import pathlib
import statistics
import sys

RESULT_COLORS = {"OK": "#1aa260", "FAIL": "#d64545", "TIMEOUT": "#e0a100"}
CONDITION_COLORS = ["#2962ff", "#7c4dff", "#00b8d4", "#f4b400", "#607d8b", "#ff7043"]
FONT = "font-family:Helvetica,Arial,sans-serif"


# ---------- helpers -----------------------------------------------------------

def quantile(xs, q):
    """Linear interpolation between order statistics (the numpy default)."""
    s = sorted(xs)
    pos = (len(s) - 1) * q
    lo, hi = math.floor(pos), math.ceil(pos)
    return s[lo] + (s[hi] - s[lo]) * (pos - lo)


def nice_step(raw):
    exp = 10 ** math.floor(math.log10(raw))
    for m in (1, 2, 2.5, 5, 10):
        if m * exp >= raw:
            return m * exp
    return 10 * exp


def nice_max(v):
    return nice_step(v) if v > 0 else 1.0


def tick_values(lo, hi, n=5):
    step = nice_step((hi - lo) / n) if hi > lo else 1.0
    v = math.ceil(lo / step - 1e-9) * step
    out = []
    while v <= hi + 1e-9:
        out.append(round(v, 10))
        v += step
    return out


def esc(s):
    return html.escape(str(s), quote=True)


class Svg:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.parts = []

    def text(self, x, y, s, size=12, anchor="start", color="#222", weight="normal", rotate=None):
        tr = f' transform="rotate({rotate} {x:.1f} {y:.1f})"' if rotate else ""
        self.parts.append(
            f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" text-anchor="{anchor}" '
            f'fill="{color}" font-weight="{weight}"{tr} style="{FONT}">{esc(s)}</text>')

    def line(self, x1, y1, x2, y2, color="#888", width=1, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.parts.append(
            f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
            f'stroke="{color}" stroke-width="{width}"{d}/>')

    def rect(self, x, y, w, h, fill, opacity=1.0, stroke="none"):
        self.parts.append(
            f'<rect x="{x:.1f}" y="{y:.1f}" width="{max(w, 0):.1f}" height="{max(h, 0):.1f}" '
            f'fill="{fill}" fill-opacity="{opacity}" stroke="{stroke}"/>')

    def circle(self, cx, cy, r, fill, opacity=1.0, stroke="none"):
        self.parts.append(
            f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r}" fill="{fill}" '
            f'fill-opacity="{opacity}" stroke="{stroke}"/>')

    def render(self):
        head = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
                f'viewBox="0 0 {self.w} {self.h}">')
        return "\n".join([head, '<rect width="100%" height="100%" fill="#ffffff"/>']
                         + self.parts + ["</svg>"]) + "\n"


def load(path):
    """Rows of rounds.csv grouped by condition, in the order they appear."""
    with open(path, encoding="utf-8", newline="") as fh:
        rows = list(csv.DictReader(fh))
    conds = {}
    for r in rows:
        conds.setdefault(r["condition"], []).append(r)
    return conds


def answered_ms(rows):
    """RTTs in ms of the rounds that got an answer (TIMEOUT and the -1 marks are left out)."""
    return [int(r["rtt_us"]) / 1000 for r in rows
            if r["result"] != "TIMEOUT" and int(r["rtt_us"]) > 0]


def legend_item(s, x, y, color, label, size=12):
    s.circle(x, y - 4, 5, color)
    s.text(x + 12, y, label, size)


# ---------- charts ------------------------------------------------------------

def timeseries(conds, path):
    W, panel_h, top = 900, 250, 50
    left, right = 72, 210
    pw = W - left - right
    s = Svg(W, top + panel_h * len(conds) + 6)
    s.text(left, 28, "Vehicle RTT per round (ms), one panel per condition", 15, weight="bold")
    lx = W - 250
    legend_item(s, lx, 28, RESULT_COLORS["OK"], "OK")
    legend_item(s, lx + 60, 28, RESULT_COLORS["FAIL"], "FAIL")
    s.text(lx + 118, 28, "x", 12, anchor="middle", color=RESULT_COLORS["TIMEOUT"], weight="bold")
    s.text(lx + 130, 28, "TIMEOUT", 12)

    for i, (name, rows) in enumerate(conds.items()):
        y0 = top + i * panel_h
        pt, pb = y0 + 22, y0 + panel_h - 46
        ph = pb - pt
        ans = answered_ms(rows)
        rounds = [int(r["round"]) for r in rows]
        nmax = max(rounds)
        vmax = nice_max(max(ans) * 1.08) if ans else 10.0

        def Y(v):
            return pb - v / vmax * ph

        def X(rnd):
            return left + (rnd - 0.5) / nmax * pw

        for t in tick_values(0, vmax, 4):
            s.line(left, Y(t), left + pw, Y(t), "#e6e6e6", 1)
            s.text(left - 8, Y(t) + 4, f"{t:g}", 11, anchor="end", color="#555")
        s.line(left, pt, left, pb, "#555")
        s.line(left, pb, left + pw, pb, "#555")
        step = max(1, math.ceil(nmax / 10))
        for rnd in range(step, nmax + 1, step):
            s.text(X(rnd), pb + 16, str(rnd), 11, anchor="middle", color="#555")
        s.text(left + pw / 2, pb + 32, "round", 11, anchor="middle", color="#555")
        s.text(18, (pt + pb) / 2, "RTT (ms)", 11, anchor="middle", color="#555", rotate=-90)

        for r in rows:
            rnd, res = int(r["round"]), r["result"]
            if res == "TIMEOUT":
                cx, cy, d = X(rnd), pb, 4
                col = RESULT_COLORS["TIMEOUT"]
                s.line(cx - d, cy - d, cx + d, cy + d, col, 2)
                s.line(cx - d, cy + d, cx + d, cy - d, col, 2)
            else:
                v = int(r["rtt_us"]) / 1000
                s.circle(X(rnd), Y(v), 3.5, RESULT_COLORS.get(res, "#999"), 0.9)

        counts = {k: sum(1 for r in rows if r["result"] == k) for k in ("OK", "FAIL", "TIMEOUT")}
        if ans:
            med = statistics.median(ans)
            s.line(left, Y(med), left + pw, Y(med), "#333", 1, dash="5,4")
            q1, q3 = quantile(ans, .25), quantile(ans, .75)
            info = [f"median {med:.1f} ms", f"IQR {q1:.1f} to {q3:.1f} ms",
                    f"min {min(ans):.1f}  max {max(ans):.1f} ms"]
        else:
            info = ["no answered rounds"]
        tx = left + pw + 16
        s.text(tx, pt + 6, name, 11, weight="bold")
        s.text(tx, pt + 22, f"n={len(rows)}  OK {counts['OK']}  FAIL {counts['FAIL']}  "
               f"TIMEOUT {counts['TIMEOUT']}", 11)
        for k, line in enumerate(info):
            s.text(tx, pt + 40 + k * 16, line, 11, color="#333")
    path.write_text(s.render(), encoding="utf-8")


def histogram(conds, path):
    data = {name: answered_ms(rows) for name, rows in conds.items()}
    allv = [v for xs in data.values() for v in xs]
    if not allv:
        return False
    lo, hi = min(allv), max(allv)
    bw = nice_step(max(hi - lo, 1.0) / 20)
    start = math.floor(lo / bw) * bw
    nb = int(math.floor((hi - start) / bw)) + 1

    pct = {}
    for name, xs in data.items():
        counts = [0] * nb
        for v in xs:
            counts[min(int((v - start) / bw), nb - 1)] += 1
        pct[name] = [c / len(xs) * 100 for c in counts] if xs else [0] * nb
    ymax = nice_max(max(max(p) for p in pct.values()) * 1.1)

    W, H, top, bottom = 900, 420, 50, 62
    left, right = 72, 250
    pw, ph = W - left - right, H - top - bottom
    s = Svg(W, H)
    s.text(left, 28, "Distribution of answered RTTs (% of each condition's rounds)", 15, weight="bold")

    def X(v):
        return left + (v - start) / (nb * bw) * pw

    def Y(p):
        return top + ph - p / ymax * ph

    for t in tick_values(0, ymax, 4):
        s.line(left, Y(t), left + pw, Y(t), "#e6e6e6")
        s.text(left - 8, Y(t) + 4, f"{t:g}", 11, anchor="end", color="#555")
    s.line(left, top, left, top + ph, "#555")
    s.line(left, top + ph, left + pw, top + ph, "#555")
    for t in tick_values(start, start + nb * bw, 10):
        if start - 1e-9 <= t <= start + nb * bw + 1e-9:
            s.text(X(t), top + ph + 16, f"{t:g}", 11, anchor="middle", color="#555")
    s.text(left + pw / 2, top + ph + 36, "RTT (ms)", 11, anchor="middle", color="#555")
    s.text(18, top + ph / 2, "% of rounds", 11, anchor="middle", color="#555", rotate=-90)

    for i, (name, _) in enumerate(conds.items()):
        col = CONDITION_COLORS[i % len(CONDITION_COLORS)]
        for j, p in enumerate(pct[name]):
            if p > 0:
                s.rect(left + j * pw / nb, Y(p), pw / nb, top + ph - Y(p), col, 0.45, col)

    lx = left + pw + 18
    for i, name in enumerate(conds):
        xs = data[name]
        col = CONDITION_COLORS[i % len(CONDITION_COLORS)]
        s.rect(lx, top + 6 + i * 52, 12, 12, col, 0.45, col)
        s.text(lx + 18, top + 16 + i * 52, name, 11, weight="bold")
        med = f"median {statistics.median(xs):.1f} ms" if xs else "no answered rounds"
        s.text(lx + 18, top + 32 + i * 52, f"n={len(xs)}  {med}", 11, color="#333")
    path.write_text(s.render(), encoding="utf-8")
    return True


def boxplot(conds, path):
    items = [(name, answered_ms(rows)) for name, rows in conds.items()]
    items = [(n, xs) for n, xs in items if xs]
    if not items:
        return False
    stats = []
    for name, xs in items:
        q1, med, q3 = quantile(xs, .25), statistics.median(xs), quantile(xs, .75)
        iqr = q3 - q1
        inside = [x for x in xs if q1 - 1.5 * iqr <= x <= q3 + 1.5 * iqr]
        stats.append(dict(name=name, n=len(xs), q1=q1, med=med, q3=q3,
                          lo=min(inside), hi=max(inside),
                          out=[x for x in xs if x < min(inside) or x > max(inside)]))
    allv = [v for _, xs in items for v in xs]
    pad = max((max(allv) - min(allv)) * 0.12, 1.0)
    vlo, vhi = min(allv) - pad, max(allv) + pad

    W, H, top, bottom = 760, 420, 50, 62
    left, right = 72, 30
    pw, ph = W - left - right, H - top - bottom
    s = Svg(W, H)
    s.text(left, 28, "RTT per condition, answered rounds (ms)", 15, weight="bold")

    def Y(v):
        return top + ph - (v - vlo) / (vhi - vlo) * ph

    for t in tick_values(vlo, vhi, 5):
        s.line(left, Y(t), left + pw, Y(t), "#e6e6e6")
        s.text(left - 8, Y(t) + 4, f"{t:g}", 11, anchor="end", color="#555")
    s.line(left, top, left, top + ph, "#555")
    s.line(left, top + ph, left + pw, top + ph, "#555")

    slot = pw / len(stats)
    box_w = min(110, slot * 0.5)
    for i, st in enumerate(stats):
        col = CONDITION_COLORS[i % len(CONDITION_COLORS)]
        cx = left + (i + 0.5) * slot
        s.line(cx, Y(st["hi"]), cx, Y(st["q3"]), col, 1.5)
        s.line(cx, Y(st["q1"]), cx, Y(st["lo"]), col, 1.5)
        s.line(cx - 12, Y(st["hi"]), cx + 12, Y(st["hi"]), col, 1.5)
        s.line(cx - 12, Y(st["lo"]), cx + 12, Y(st["lo"]), col, 1.5)
        s.rect(cx - box_w / 2, Y(st["q3"]), box_w, Y(st["q1"]) - Y(st["q3"]), col, 0.3, col)
        s.line(cx - box_w / 2, Y(st["med"]), cx + box_w / 2, Y(st["med"]), col, 3)
        s.text(cx + box_w / 2 + 6, Y(st["med"]) + 4, f"{st['med']:.1f}", 11, color="#333")
        for o in st["out"]:
            s.circle(cx, Y(o), 3, "none", 1, col)
        s.text(cx, top + ph + 18, st["name"], 11, anchor="middle", weight="bold")
        s.text(cx, top + ph + 34, f"n={st['n']}", 11, anchor="middle", color="#555")
    path.write_text(s.render(), encoding="utf-8")
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rounds", help="rounds.csv written by tools/analyze_runs.py")
    ap.add_argument("--out", default=None, help="output folder (default: next to rounds.csv)")
    args = ap.parse_args(argv)

    src = pathlib.Path(args.rounds)
    out = pathlib.Path(args.out) if args.out else src.parent
    out.mkdir(parents=True, exist_ok=True)
    conds = load(src)
    if not conds:
        print(f"no rounds in {src}", file=sys.stderr)
        return 1

    timeseries(conds, out / "rtt_timeseries.svg")
    written = ["rtt_timeseries.svg"]
    if histogram(conds, out / "rtt_histogram.svg"):
        written.append("rtt_histogram.svg")
    if boxplot(conds, out / "rtt_boxplot.svg"):
        written.append("rtt_boxplot.svg")
    for name in written:
        print(out / name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
