#!/usr/bin/env python3
"""Cutoff analysis for the mitigation section: how a latency bound separates direct from relayed rounds.

The unit is the campaign. The script reports:
  1. the phase check: where each campaign's round sends fall on the vehicle's 50 ms clock, and the start
     offset that the M-19 firmware logs (campaigns from before M-19 have no offset line);
  2. the RTT of each condition;
  3. the operating curve of a bound T: honest rounds rejected (RTT above T, or no answer) and relayed
     rounds accepted (answered within T), pooled over all campaigns;
  4. a leave-one-campaign-out check: the bound is set from the other honest campaigns only, then tested
     on the held-out honest campaign and on all relayed rounds;
  5. and 6. the same operating curve and leave-one-campaign-out check on the M-19 campaigns only, which
     start each campaign at a random offset of the round grid.

Writes data/analysis-20261118/cutoff.md. Within one campaign the sends stay on one phase, because 1000 ms
is a multiple of the 50 ms connection interval. The connection anchor is not logged, so the phase in
section 1 is on the vehicle's clock.

Usage (from the repo root): python -I tools/analyze_cutoff.py
"""

import math
import pathlib
import re
import statistics
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import analyze_conditions as ac  # noqa: E402
import analyze_runs as ar  # noqa: E402

REPO = HERE.parent
OUT = REPO / "data" / "analysis-20261118" / "cutoff.md"
GRID_MS = [100, 110, 120, 130, 140, 150, 160, 170, 180, 200, 250, 300]
ALPHAS = [0.01, 0.05]
PERIOD_US = 50000
OFFSET_RE = re.compile(r"grid start offset (\d+) us")
M19 = "M-19"


def load_campaigns():
    camps = []
    for spec in ac.CONDITIONS:
        for session, campaign in spec["campaigns"]:
            lines, _ = ac.load_campaign(session, campaign)
            veh, a_rows, b_rows, _ = ar.parse_rows(lines)
            offset = next((int(m.group(1)) for m in map(OFFSET_RE.search, lines) if m), None)
            camps.append(dict(name=spec["name"], honest=spec["tunnel"] == "direct",
                              rounds=ar.join_rounds(veh, a_rows, b_rows), campaign=campaign,
                              offset_us=offset))
    return camps


def value_ms(r):
    return r["rtt"] / 1000.0 if r["res"] == "OK" and r["rtt"] > 0 else math.inf


def phase_stats(rounds):
    th = [2 * math.pi * ((int(r["t_tx"]) % PERIOD_US) / PERIOD_US) for r in rounds]
    cx, sx = sum(map(math.cos, th)), sum(map(math.sin, th))
    resultant = math.hypot(cx, sx) / len(th)
    mean_ms = (math.degrees(math.atan2(sx, cx)) % 360) / 360 * 50.0
    return resultant, mean_ms


def threshold_for(train_values, alpha):
    s = sorted(train_values)
    return s[math.ceil((1 - alpha) * len(s)) - 1]


def fmt(v):
    return "no bound" if math.isinf(v) else f"{v:.1f}"


def operating_lines(honest, relayed, heading):
    h_vals = [value_ms(r) for c in honest for r in c["rounds"]]
    r_vals = [value_ms(r) for c in relayed for r in c["rounds"]]
    md = [f"## {heading}", "",
          f"Honest rounds: {len(h_vals)} from {len(honest)} direct campaigns. Relayed rounds: {len(r_vals)} "
          f"from {len(relayed)} relayed campaigns. A round is rejected by a bound T when it has no answer or its "
          "RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.", "",
          "| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |", "|---|---|---|"]
    for T in GRID_MS:
        frr = sum(1 for v in h_vals if v > T) / len(h_vals)
        far = sum(1 for v in r_vals if v <= T) / len(r_vals)
        md.append(f"| {T} | {frr * 100:.1f} % | {far * 100:.1f} % |")
    t_max = max(v for v in h_vals if math.isfinite(v))
    far_at_max = sum(1 for v in r_vals if v <= t_max) / len(r_vals)
    md += ["", f"The largest honest RTT is {t_max:.1f} ms. A bound at that value rejects no honest round in "
           f"sample, and accepts {far_at_max * 100:.1f} % of relayed rounds."]
    return md


def loco_lines(honest, relayed, heading):
    r_vals = [value_ms(r) for c in relayed for r in c["rounds"]]
    md = [f"## {heading}", "",
          "For each direct campaign h: T_h is the (1 - alpha) quantile of the honest rounds of the other direct "
          "campaigns, so h and the relayed rounds never set their own bound. Then h is tested (FRR) and all "
          "relayed rounds are tested (FAR).", "",
          "| alpha | T_h min (ms) | T_h median (ms) | T_h max (ms) | Held-out FRR mean | Held-out FRR max | "
          "FAR (relayed, pooled) min | FAR median | FAR max |",
          "|---|---|---|---|---|---|---|---|---|"]
    for alpha in ALPHAS:
        Ts, frrs, fars = [], [], []
        for h in honest:
            train = [value_ms(r) for c in honest if c is not h for r in c["rounds"]]
            T = threshold_for(train, alpha)
            Ts.append(T)
            held = [value_ms(r) for r in h["rounds"]]
            frrs.append(sum(1 for v in held if v > T) / len(held))
            fars.append(sum(1 for v in r_vals if v <= T) / len(r_vals))
        finite = [t for t in Ts if math.isfinite(t)]
        t_min = fmt(min(finite)) if finite else "no bound"
        t_med = fmt(statistics.median(finite)) if finite else "no bound"
        t_max_cv = fmt(max(finite)) if finite else "no bound"
        md.append(f"| {alpha} | {t_min} | {t_med} | {t_max_cv} | {statistics.mean(frrs) * 100:.1f} % | "
                  f"{max(frrs) * 100:.1f} % | {min(fars) * 100:.1f} % | {statistics.median(fars) * 100:.1f} % | "
                  f"{max(fars) * 100:.1f} % |")
    return md


def main(argv=None):
    camps = load_campaigns()
    honest = [c for c in camps if c["honest"]]
    relayed = [c for c in camps if not c["honest"]]
    m19 = [c for c in camps if M19 in c["name"]]
    m19_honest = [c for c in m19 if c["honest"]]
    m19_relayed = [c for c in m19 if not c["honest"]]

    md = ["# Cutoff analysis (mitigation, Milestone 4)", "",
          "The unit is the campaign. Sections 3 and 4 pool every campaign; sections 5 and 6 use the M-19 "
          "campaigns only, whose round grid starts at a random offset.", "",
          "## 1. Phase of the round sends on the 50 ms grid", "",
          "R is the resultant length of the send phases within a campaign: 1 means every send falls on the same "
          "phase. The phase is on the vehicle's clock, because the connection anchor is not logged. The start "
          "offset is the `grid start offset` line of the M-19 firmware; the campaigns before M-19 have none.", "",
          "| Condition | Campaign | Start offset (ms) | R | Mean phase (ms) |", "|---|---|---|---|---|"]
    for c in camps:
        R, mean_ms = phase_stats(c["rounds"])
        off = "-" if c["offset_us"] is None else f"{c['offset_us'] / 1000:.1f}"
        md.append(f"| {c['name']} | {c['campaign']} | {off} | {R:.3f} | {mean_ms:.1f} |")

    md += ["", "## 2. RTT per condition (answered rounds, ms)", "",
           "| Condition | n | min | median | p99 | max |", "|---|---|---|---|---|---|"]
    names = list(dict.fromkeys(c["name"] for c in camps))
    for name in names:
        vals = [value_ms(r) for c in camps if c["name"] == name for r in c["rounds"] if math.isfinite(value_ms(r))]
        q99 = ar.quantile(sorted(vals), 0.99)
        md.append(f"| {name} | {len(vals)} | {min(vals):.1f} | {statistics.median(vals):.1f} | {q99:.1f} | "
                  f"{max(vals):.1f} |")

    md += [""] + operating_lines(honest, relayed, "3. Operating curve of a bound (pooled, in-sample)")
    md += [""] + loco_lines(honest, relayed, "4. Leave-one-campaign-out check (pooled)")
    md += [""] + operating_lines(m19_honest, m19_relayed, "5. Operating curve, M-19 campaigns only (in-sample)")
    md += [""] + loco_lines(m19_honest, m19_relayed, "6. Leave-one-campaign-out check, M-19 campaigns only")

    md += ["", "## Notes", "",
           "- Sections 3 and 4 mix the 2026-11-18 campaigns with the M-19 campaigns. The firmware before M-19 sent "
           "round 1 at START, so the phase of those campaigns followed from the connection timing. Sections 5 and "
           "6 use the M-19 campaigns only, whose offsets are drawn at random.",
           "- Within one campaign the sends stay on one phase, so every campaign's RTT is shifted by its own phase "
           "(section 1 shows R per campaign).",
           "- The bound is set from honest data only. Relayed data never set it.",
           "- The M-19 sections are at one distance (car-key 2.24 m, A-B 2 m). The 0.52 m and 0.99 m campaigns "
           "are in sections 1 to 4 only.",
           "- A bound is valid for this stack and these distances. It is not a general defence (LIMITATIONS.md, "
           "limit 2)."]
    OUT.write_text("\n".join(md) + "\n", encoding="utf-8")
    print("\n".join(md))
    print(f"\nwritten: {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
