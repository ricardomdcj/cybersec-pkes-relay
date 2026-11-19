#!/usr/bin/env python3
"""Latency-injection sweep: success of the relayed round versus the delay Relay B adds (D, ms).

Each campaign is labelled relay-dNNNN (D in ms) at 0.99 m. Relay B holds every vehicle challenge
for D ms before it goes on the tunnel (command "DELAY <ms>", relays/main/relay_b.c). The vehicle
authenticates a round only if its response arrives within its timeout (ROUND_TIMEOUT_MS = 2000 ms,
vehicle/main/main.c). So the relay stops authenticating once D plus the honest relayed RTT exceeds
the timeout: the threshold is about timeout minus the relayed RTT, whatever the distance.

Writes data/analysis-20261118/delay_sweep.md and delay_sweep.csv (for the paper's plot).
Usage (from the repo root): python -I tools/analyze_delay.py
"""

import csv
import json
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
RUNS = REPO / "data" / "runs"
OUT_MD = REPO / "data" / "analysis-20261118" / "delay_sweep.md"
OUT_CSV = REPO / "data" / "analysis-20261118" / "delay_sweep.csv"
TIMEOUT_MS = 2000  # vehicle/main/main.c ROUND_TIMEOUT_MS
LABEL_RE = re.compile(r"relay-d(\d+)$")
DELAY_LOG_RE = re.compile(r"DELAY set=(\d+) ms")


def find_sweep():
    """Yield (D, session, campaign) for every campaign labelled relay-dNNNN."""
    found = []
    for session in sorted(RUNS.glob("*-session")):
        for campaign in sorted(session.glob("*/")):
            manifest = campaign / "manifest.json"
            if not manifest.exists():
                continue
            label = json.loads(manifest.read_text(encoding="utf-8")).get("label", "")
            m = LABEL_RE.search(label)
            if m:
                found.append((int(m.group(1)), session.name, campaign.name))
    return sorted(found)


def wilson_pct(k, n):
    lo, hi = ar.wilson(k, n)
    return (lo or 0.0) * 100, (hi or 0.0) * 100


def main(argv=None):
    rows = []
    for d, session, campaign in find_sweep():
        lines, manifest = ac.load_campaign(session, campaign)
        veh, a_rows, b_rows, _ = ar.parse_rows(lines)
        rounds = ar.join_rounds(veh, a_rows, b_rows)
        n = len(rounds)
        ok = sum(1 for r in rounds if r["res"] == "OK")
        fail = sum(1 for r in rounds if r["res"] == "FAIL")
        timeout = sum(1 for r in rounds if r["res"] == "TIMEOUT")
        rtt_ok = [r["rtt"] / 1000.0 for r in rounds if r["res"] == "OK" and r["rtt"] > 0]
        logged = [int(m.group(1)) for m in (DELAY_LOG_RE.search(l) for l in lines) if m]
        lo, hi = wilson_pct(ok, n)
        rows.append(dict(d=d, campaign=campaign, n=n, ok=ok, fail=fail, timeout=timeout,
                         success=ok / n * 100 if n else float("nan"), lo=lo, hi=hi,
                         med=statistics.median(rtt_ok) if rtt_ok else float("nan"),
                         rmin=min(rtt_ok) if rtt_ok else float("nan"),
                         rmax=max(rtt_ok) if rtt_ok else float("nan"),
                         logged=logged[0] if logged else None))

    if not rows:
        print("no delay campaigns found (label relay-dNNNN)")
        return 1

    base = next((r for r in rows if r["d"] == 0), None)
    base_med = base["med"] if base else float("nan")
    expected = TIMEOUT_MS - base_med if base else float("nan")

    # threshold: the delay where success first drops below 50 %, by linear interpolation
    d50 = None
    for a, b in zip(rows, rows[1:]):
        if a["success"] >= 50 > b["success"]:
            span = a["success"] - b["success"]
            d50 = a["d"] + (a["success"] - 50) / span * (b["d"] - a["d"]) if span else a["d"]
            break

    md = ["# Latency-injection sweep (relay B holds each challenge for D ms, 0.99 m)", "",
          f"The verifier authenticates a round when its response arrives within {TIMEOUT_MS} ms. "
          "Each row is one campaign of 50 rounds, unicast relay, default TX power, M-19 firmware.", "",
          "| D (ms) | Rounds | OK | FAIL | TIMEOUT | Success (95 % Wilson) | Median RTT of OK rounds (ms) | "
          "Min / max of OK (ms) | Logged DELAY (ms) | Campaign |",
          "|---|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        md.append(f"| {r['d']} | {r['n']} | {r['ok']} | {r['fail']} | {r['timeout']} | "
                  f"{r['success']:.1f} % ({r['lo']:.1f} to {r['hi']:.1f}) | {r['med']:.1f} | "
                  f"{r['rmin']:.1f} / {r['rmax']:.1f} | {r['logged'] if r['logged'] is not None else '-'} | "
                  f"{r['campaign']} |")
    md += ["", f"Honest relayed RTT at D = 0: median {base_med:.1f} ms. Expected threshold, timeout minus that "
           f"median: {expected:.0f} ms of injected delay.", ""]
    if d50 is not None:
        md.append(f"Measured threshold (success falls below 50 %): D50 = {d50:.0f} ms of injected delay, "
                  f"so the relay tolerates about {d50:.0f} ms of added latency at this timeout.")
    else:
        md.append("The success rate did not cross 50 % in the swept range.")
    md += ["", "The relay's effectiveness limit is set by the verifier's timeout and by the honest relayed RTT, not "
           "by the distance: the timeout is a bench constant, and a real vehicle may use a different one.", ""]

    OUT_MD.write_text("\n".join(md) + "\n", encoding="utf-8")
    with open(OUT_CSV, "w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["delay_ms", "rounds", "ok", "fail", "timeout", "success_pct", "wilson_lo_pct",
                    "wilson_hi_pct", "median_rtt_ok_ms", "rtt_ok_min_ms", "rtt_ok_max_ms", "campaign"])
        for r in rows:
            w.writerow([r["d"], r["n"], r["ok"], r["fail"], r["timeout"], f"{r['success']:.2f}",
                        f"{r['lo']:.2f}", f"{r['hi']:.2f}", f"{r['med']:.2f}", f"{r['rmin']:.2f}",
                        f"{r['rmax']:.2f}", r["campaign"]])
    print("\n".join(md))
    print(f"written: {OUT_MD} and {OUT_CSV}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
