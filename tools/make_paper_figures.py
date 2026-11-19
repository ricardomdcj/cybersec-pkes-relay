#!/usr/bin/env python3
"""Writes the paper's figures as TikZ (no pgfplots, no external converter) into paper/figures/.

Figures:
  fig_overhead.tex  campaign medians of the direct and relayed configurations, per layout and set
  fig_bound.tex     operating curve of the round-trip-time bound on the replication set (160 ms marked)
  fig_sweep.tex     success of the relayed rounds versus the injected delay (if delay_sweep.csv exists)
  fig_rssi.tex      campaign median RSSI of the direct and relayed configurations (M-19 family)

The data come from the campaign folders (analyze_cutoff.load_campaigns) and from
data/analysis-20261118/delay_sweep.csv. Usage (from the repo root): python -I tools/make_paper_figures.py
"""

import csv
import pathlib
import statistics
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import analyze_cutoff as ac2  # noqa: E402

REPO = HERE.parent
FIG = REPO / "paper" / "figures"
SWEEP_CSV = REPO / "data" / "analysis-20261118" / "delay_sweep.csv"
BOUND_T = 160.0


def fmt(v):
    return f"{v:.2f}".rstrip("0").rstrip(".") if isinstance(v, float) else str(v)


def axes(x0, y0, w, h, xmin, xmax, ymin, ymax, xticks, yticks, xlabel, ylabel, xfmt=None, yfmt=None):
    sx = lambda v: x0 + (v - xmin) / (xmax - xmin) * w  # noqa: E731
    sy = lambda v: y0 + (v - ymin) / (ymax - ymin) * h  # noqa: E731
    out = [f"\\draw[black!70] ({fmt(x0)},{fmt(y0)}) -- ({fmt(x0 + w)},{fmt(y0)});",
           f"\\draw[black!70] ({fmt(x0)},{fmt(y0)}) -- ({fmt(x0)},{fmt(y0 + h)});"]
    for t in xticks:
        X = sx(t)
        lab = xfmt(t) if xfmt else str(t)
        out.append(f"\\draw[black!70] ({fmt(X)},{fmt(y0)}) -- ++(0,-0.08);")
        out.append(f"\\node[font=\\scriptsize, below] at ({fmt(X)},{fmt(y0)}) {{{lab}}};")
    for t in yticks:
        Y = sy(t)
        lab = yfmt(t) if yfmt else str(t)
        out.append(f"\\draw[black!70] ({fmt(x0)},{fmt(Y)}) -- ++(-0.08,0);")
        out.append(f"\\node[font=\\scriptsize, left] at ({fmt(x0)},{fmt(Y)}) {{{lab}}};")
    out.append(f"\\node[font=\\small] at ({fmt(x0 + w / 2)},{fmt(y0 - 0.9)}) {{{xlabel}}};")
    out.append(f"\\node[font=\\small, rotate=90] at ({fmt(x0 - 1.1)},{fmt(y0 + h / 2)}) {{{ylabel}}};")
    return out, sx, sy


def wrap(name, body, caption_note):
    text = "\n".join(["% " + caption_note, "\\begin{tikzpicture}[>=Latex, font=\\footnotesize]"]
                     + body + ["\\end{tikzpicture}", ""])
    (FIG / name).write_text(text, encoding="utf-8")
    print("written:", FIG / name)


def campaigns_by_name():
    camps = ac2.load_campaigns()
    by = {}
    for c in camps:
        by.setdefault(c["name"], []).append(c)
    return by


def fig_overhead(by):
    groups = [
        ("2.24 M-19\nin sample", "baseline 2.24 M-19", "unicast A-B 2 M-19 (car-key 2.24)"),
        ("2.24 held-out", "baseline 2.24 held-out", "unicast A-B 2 held-out (car-key 2.24)"),
        ("2.24 replic.", "baseline 2.24 replication", "unicast A-B 2 replication (car-key 2.24)"),
        ("0.99 M-19", "baseline 0.99 M-19", "unicast A-B 0.75 M-19 (car-key 0.99)"),
        ("2.24 earlier", "baseline 2.24", "unicast A-B 2 (car-key 2.24)"),
        ("0.99 earlier", "baseline 0.99", "unicast A-B 0.75 (car-key 0.99)"),
        ("0.52 earlier", "baseline 0.52", "unicast 0.52"),
    ]
    body = []
    ax, sx, sy = axes(0, 0, 14.0, 6.0, 0, len(groups), 0, 500,
                      [i + 0.5 for i in range(len(groups))], [0, 100, 200, 300, 400, 500],
                      "layout and set", "round-trip time of answered rounds (ms)")
    body += ax
    for i, (label, dname, rname) in enumerate(groups):
        x = i + 0.5
        # axis label of the group, two lines when needed
        for j, line in enumerate(label.split("\n")):
            body.append(f"\\node[font=\\scriptsize, below] at ({fmt(sx(x))},{fmt(0 - 0.22 - 0.35 * j)}) {{{line}}};")
        for name, color, dx in ((dname, "black!55", -0.22), (rname, "blue!70!black", 0.22)):
            camps = by.get(name, [])
            meds = [statistics.median([r["rtt"] / 1000 for r in c["rounds"] if r["res"] != "TIMEOUT" and r["rtt"] > 0])
                    for c in camps if any(r["res"] != "TIMEOUT" for r in c["rounds"])]
            for k, m in enumerate(meds):
                jitter = ((k * 0.37) % 1 - 0.5) * 0.12
                body.append(f"\\fill[{color}] ({fmt(sx(x + dx + jitter))},{fmt(sy(m))}) circle (1.4pt);")
            if meds:
                body.append(f"\\draw[{color}, thick] ({fmt(sx(x + dx - 0.08))},{fmt(sy(statistics.median(meds)))}) -- "
                            f"({fmt(sx(x + dx + 0.08))},{fmt(sy(statistics.median(meds)))});")
    body.append("\\node[font=\\scriptsize, anchor=west] at (9.0,5.6) {\\textcolor{black!55}{$\\bullet$ direct}};")
    body.append("\\node[font=\\scriptsize, anchor=west] at (9.0,5.2) {\\textcolor{blue!70!black}{$\\bullet$ relayed}};")
    wrap("fig_overhead.tex", body, "Campaign medians (dots) and the median of each configuration (bar). Generated by tools/make_paper_figures.py.")


def fig_bound(by):
    honest = by.get("baseline 2.24 replication", [])
    relayed = by.get("unicast A-B 2 replication (car-key 2.24)", [])
    h_rounds = [r for c in honest for r in c["rounds"]]
    r_rounds = [r for c in relayed for r in c["rounds"]]

    def val(r):
        return r["rtt"] / 1000.0 if r["res"] == "OK" and r["rtt"] > 0 else float("inf")

    h_vals = [val(r) for r in h_rounds]
    r_vals = [val(r) for r in r_rounds]
    grid = list(range(100, 201, 5)) + [250, 300]
    points = [(t, sum(1 for v in h_vals if v > t) / len(h_vals) * 100,
               sum(1 for v in r_vals if v <= t) / len(r_vals) * 100) for t in grid]
    body = []
    ax, sx, sy = axes(0, 0, 12.0, 6.0, 100, 300, 0, 30, [100, 120, 140, 160, 180, 200, 250, 300], [0, 5, 10, 15, 20, 25, 30],
                      "round-trip bound $T$ (ms)", "share of rounds (\\%)")
    body += ax
    body.append(f"\\draw[black!45, dashed] ({fmt(sx(BOUND_T))},0) -- ({fmt(sx(BOUND_T))},{fmt(sy(30))});")
    body.append(f"\\node[font=\\scriptsize, anchor=west] at ({fmt(sx(BOUND_T) + 0.1)},{fmt(sy(29))}) {{$T=160$\\,ms}};")
    for series, color in ((1, "black!55"), (2, "blue!70!black")):
        pts = [f"({fmt(sx(p[0]))},{fmt(sy(min(p[series], 30)))})" for p in points]
        body.append(f"\\draw[{color}, thick] " + " -- ".join(pts) + ";")
    body.append("\\node[font=\\scriptsize, anchor=west] at (7.6,5.4) {\\textcolor{black!55}{honest rounds rejected}};")
    body.append("\\node[font=\\scriptsize, anchor=west] at (7.6,4.9) {\\textcolor{blue!70!black}{relayed rounds accepted}};")
    wrap("fig_bound.tex", body, "Operating curve of the 160 ms bound on the replication set (10 + 10 campaigns). Generated by tools/make_paper_figures.py.")


def fig_sweep():
    if not SWEEP_CSV.exists():
        print("skipped fig_sweep.tex: no delay_sweep.csv yet")
        return
    rows = list(csv.DictReader(open(SWEEP_CSV, encoding="utf-8")))
    pts = [(float(r["delay_ms"]), float(r["success_pct"]), float(r["wilson_lo_pct"]), float(r["wilson_hi_pct"]))
           for r in rows]
    xmax = max(p[0] for p in pts)
    body = []
    ax, sx, sy = axes(0, 0, 12.0, 6.0, 0, xmax, 0, 100, [int(v) for v in range(0, int(xmax) + 1, 500)],
                      [0, 25, 50, 75, 100], "injected delay $D$ (ms)", "relayed rounds authenticated (\\%)")
    body += ax
    for d, s, lo, hi in pts:
        body.append(f"\\draw[black!50] ({fmt(sx(d))},{fmt(sy(lo))}) -- ({fmt(sx(d))},{fmt(sy(hi))});")
        body.append(f"\\fill[blue!70!black] ({fmt(sx(d))},{fmt(sy(s))}) circle (1.6pt);")
    body.append(f"\\draw[blue!70!black, thick] " + " -- ".join(f"({fmt(sx(d))},{fmt(sy(s))})" for d, s, _, _ in pts) + ";")
    body.append(f"\\draw[black!45, dashed] (0,{fmt(sy(50))}) -- ({fmt(sx(xmax))},{fmt(sy(50))});")
    wrap("fig_sweep.tex", body, "Success of the relayed rounds at 0.99 m versus the delay Relay B injects (one campaign per delay; bars are 95 % Wilson intervals). Generated by tools/make_paper_figures.py.")


def fig_rssi(by):
    d_c = by.get("baseline 2.24 M-19", []) + by.get("baseline 2.24 held-out", []) + by.get("baseline 2.24 replication", [])
    r_c = (by.get("unicast A-B 2 M-19 (car-key 2.24)", []) + by.get("unicast A-B 2 held-out (car-key 2.24)", [])
           + by.get("unicast A-B 2 replication (car-key 2.24)", []))
    dm = [statistics.median([r["rssi"] for r in c["rounds"] if r["res"] == "OK"]) for c in d_c]
    rm = [statistics.median([r["rssi"] for r in c["rounds"] if r["res"] == "OK"]) for c in r_c]
    body = []
    ax, sx, sy = axes(0, 0, 5.0, 6.0, 0, 3, -70, -20, [], [-70, -60, -50, -40, -30, -20],
                      "", "campaign median RSSI (dBm)")
    body += ax
    for i, (vals, color, x) in enumerate(((dm, "black!55", 1.0), (rm, "blue!70!black", 2.0))):
        for k, m in enumerate(vals):
            jitter = ((k * 0.37) % 1 - 0.5) * 0.4
            body.append(f"\\fill[{color}] ({fmt(sx(x + jitter))},{fmt(sy(m))}) circle (1.4pt);")
    body.append(f"\\node[font=\\scriptsize] at ({fmt(sx(1.0))},{fmt(sy(-72))}) {{direct (28)}};")
    body.append(f"\\node[font=\\scriptsize] at ({fmt(sx(2.0))},{fmt(sy(-72))}) {{relayed (28)}};")
    wrap("fig_rssi.tex", body, "Campaign median RSSI of the direct and relayed configurations at 2.24 m (M-19 family). Generated by tools/make_paper_figures.py.")


def main(argv=None):
    FIG.mkdir(parents=True, exist_ok=True)
    by = campaigns_by_name()
    fig_overhead(by)
    fig_bound(by)
    fig_sweep()
    fig_rssi(by)
    return 0


if __name__ == "__main__":
    sys.exit(main())
