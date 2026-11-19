#!/usr/bin/env python3
"""Cutoff analysis for the mitigation section: how a latency bound separates direct from relayed rounds.

The unit is the campaign. The script reports three things:
  1. the phase check: where each campaign's round sends fall on the 50 ms connection grid;
  2. the operating curve of a bound T: honest rounds rejected (RTT above T, or no answer) and relayed
     rounds accepted (answered within T), pooled over all campaigns;
  3. a leave-one-campaign-out check: the bound is set from the other honest campaigns only, then tested
     on the held-out honest campaign and on all relayed rounds.

Writes data/analysis-20261118/cutoff.md. The phase section decides how far the numbers can be trusted:
if the sends are locked to one phase within a campaign, a campaign samples one phase, not a replicate.

Usage (from the repo root): python -I tools/analyze_cutoff.py
"""

import math
import pathlib
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


def load_campaigns():
    camps = []
    for spec in ac.CONDITIONS:
        for session, campaign in spec["campaigns"]:
            lines, _ = ac.load_campaign(session, campaign)
            veh, a_rows, b_rows, _ = ar.parse_rows(lines)
            camps.append(dict(name=spec["name"], honest=spec["tunnel"] == "direct",
                              rounds=ar.join_rounds(veh, a_rows, b_rows), campaign=campaign))
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


def main(argv=None):
    camps = load_campaigns()
    honest = [c for c in camps if c["honest"]]
    relayed = [c for c in camps if not c["honest"]]
    h_vals = [value_ms(r) for c in honest for r in c["rounds"]]
    r_vals = [value_ms(r) for c in relayed for r in c["rounds"]]

    md = ["# Cutoff analysis (mitigation, Milestone 4)", "",
          f"Honest rounds: {len(h_vals)} from {len(honest)} direct campaigns. Relayed rounds: {len(r_vals)} "
          f"from {len(relayed)} relayed campaigns. A round is rejected by a bound T when it has no answer or its "
          "RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.", "",
          "## 1. Phase of the round sends on the 50 ms connection grid", "",
          "Resultant length R of the send phases within a campaign: 1 means every send falls on the same phase.", "",
          "| Condition | Campaign | R | Mean phase (ms) |", "|---|---|---|---|"]
    for c in camps:
        R, mean_ms = phase_stats(c["rounds"])
        md.append(f"| {c['name']} | {c['campaign']} | {R:.3f} | {mean_ms:.1f} |")

    md += ["", "## 2. RTT per condition (answered rounds, ms)", "",
           "| Condition | n | min | median | p99 | max |", "|---|---|---|---|---|---|"]
    names = list(dict.fromkeys(c["name"] for c in camps))
    for name in names:
        vals = [value_ms(r) for c in camps if c["name"] == name for r in c["rounds"] if math.isfinite(value_ms(r))]
        q99 = ar.quantile(sorted(vals), 0.99)
        md.append(f"| {name} | {len(vals)} | {min(vals):.1f} | {statistics.median(vals):.1f} | {q99:.1f} | "
                  f"{max(vals):.1f} |")

    md += ["", "## 3. Operating curve of a bound (pooled, in-sample)", "",
           "| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |", "|---|---|---|"]
    for T in GRID_MS:
        frr = sum(1 for v in h_vals if v > T) / len(h_vals)
        far = sum(1 for v in r_vals if v <= T) / len(r_vals)
        md.append(f"| {T} | {frr * 100:.1f} % | {far * 100:.1f} % |")
    honest_answered = [v for v in h_vals if math.isfinite(v)]
    t_max = max(honest_answered)
    far_at_max = sum(1 for v in r_vals if v <= t_max) / len(r_vals)
    md += ["", f"The largest honest RTT is {t_max:.1f} ms. A bound at that value rejects no honest round in "
           f"sample, and accepts {far_at_max * 100:.1f} % of relayed rounds."]

    md += ["", "## 4. Leave-one-campaign-out check", "",
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

    md += ["", "## Notes", "",
           "- The honest distribution comes from three campaigns per setup. Each campaign sends its rounds on "
           "one phase of the 50 ms grid (section 1), so the tails here are the tails of those phases, not of all "
           "phases. These numbers are provisional until the start-offset change (M-19) is in place and the "
           "campaigns are repeated.",
           "- The bound is set from honest data only. Relayed data never set it.",
           "- A bound is valid for this stack and these distances. It is not a general defence (LIMITATIONS.md, "
           "limit 2)."]
    OUT.write_text("\n".join(md) + "\n", encoding="utf-8")
    print("\n".join(md))
    print(f"\nwritten: {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
