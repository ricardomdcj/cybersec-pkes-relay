#!/usr/bin/env python3
"""Pools the campaigns of each condition and writes rounds.csv and summary.md.

tools/analyze_runs.py treats each campaign folder as one condition. This script pools the three
repeats of each setup, joins the relay lines from the session's chain log (CSVA/CSVB) and writes:
  rounds.csv   one row per round (same columns as analyze_runs.py)
  summary.md   outcomes, RTT, round legs and overhead per setup

Then draw the charts with: python -I tools/plot_runs.py <out>/rounds.csv

Usage (from the repo root, ESP-IDF Python or any Python 3):
  python -I tools/analyze_conditions.py [--out data/analysis-20261118]
"""

import argparse
import json
import pathlib
import statistics
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import analyze_runs as ar  # noqa: E402

REPO = HERE.parent
RUNS = REPO / "data" / "runs"
CHAIN = "chain_COM5_COM6_COM7.log"

CONDITIONS = [
    dict(name="baseline 0.52", dist=0.52, tunnel="direct", campaigns=[
        ("20261118-194943_baseline-session", "20261118-194946_baseline"),
        ("20261118-200758_baseline-session", "20261118-200802_baseline"),
        ("20261118-201019_baseline-session", "20261118-201022_baseline")]),
    dict(name="broadcast 0.52", dist=0.52, tunnel="broadcast", campaigns=[
        ("20261118-195535_relay-session", "20261118-195547_relay"),
        ("20261118-200902_relay-session", "20261118-200913_relay"),
        ("20261118-201121_relay-session", "20261118-201132_relay")]),
    dict(name="unicast 0.52", dist=0.52, tunnel="unicast", campaigns=[
        ("20261118-202314_relay-session", "20261118-202325_relay"),
        ("20261118-205220_relay-unicast-session", "20261118-205231_relay-unicast"),
        ("20261118-205758_relay-unicast-session", "20261118-205816_relay-unicast")]),
    dict(name="unicast 2 dBm 0.52", dist=0.52, tunnel="unicast, TXPOW 8", campaigns=[
        ("20261118-210207_relay-unicast-tx8-session", "20261118-210338_relay-unicast-tx8"),
        ("20261118-210459_relay-unicast-tx8-session", "20261118-210525_relay-unicast-tx8"),
        ("20261118-210625_relay-unicast-tx8-session", "20261118-210651_relay-unicast-tx8")]),
    dict(name="baseline 0.99", dist=0.99, tunnel="direct", campaigns=[
        ("20261118-212448_baseline-session", "20261118-212452_baseline"),
        ("20261118-212709_baseline-session", "20261118-212713_baseline"),
        ("20261118-213129_baseline-session", "20261118-213133_baseline")]),
    dict(name="unicast A-B 0.75 (car-key 0.99)", dist=0.99, tunnel="unicast, A-B 0.75 m", campaigns=[
        ("20261118-212551_relay-unicast-ab075-session", "20261118-212608_relay-unicast-ab075"),
        ("20261118-212812_relay-unicast-ab075-session", "20261118-212942_relay-unicast-ab075"),
        ("20261118-213233_relay-unicast-ab075-session", "20261118-213251_relay-unicast-ab075")]),
    dict(name="baseline 2.24", dist=2.24, tunnel="direct", campaigns=[
        ("20261118-215829_baseline-session", "20261118-215833_baseline"),
        ("20261118-220051_baseline-session", "20261118-220055_baseline"),
        ("20261118-220426_baseline-session", "20261118-220430_baseline")]),
    dict(name="unicast A-B 2 (car-key 2.24)", dist=2.24, tunnel="unicast, A-B 2 m", campaigns=[
        ("20261118-215933_relay-unicast-ab200-session", "20261118-215951_relay-unicast-ab200"),
        ("20261118-220155_relay-unicast-ab200-session", "20261118-220325_relay-unicast-ab200"),
        ("20261118-220529_relay-unicast-ab200-session", "20261118-220700_relay-unicast-ab200")]),
    dict(name="unicast A-B 2 TX 2 dBm (car-key 2.24)", dist=2.24, tunnel="unicast, A-B 2 m, TXPOW 8", campaigns=[
        ("20261118-222135_relay-unicast-ab200-tx8-session", "20261118-222201_relay-unicast-ab200-tx8"),
        ("20261118-222302_relay-unicast-ab200-tx8-session", "20261118-222426_relay-unicast-ab200-tx8"),
        ("20261118-222527_relay-unicast-ab200-tx8-session", "20261118-222552_relay-unicast-ab200-tx8")]),
    dict(name="baseline 2.24 M-19", dist=2.24, tunnel="direct", campaigns=[
        ("20261118-225639_baseline-m19-session", "20261118-225643_baseline-m19"),
        ("20261118-230050_baseline-m19-session", "20261118-230054_baseline-m19"),
        ("20261118-230423_baseline-m19-session", "20261118-230427_baseline-m19"),
        ("20261118-230754_baseline-m19-session", "20261118-230758_baseline-m19"),
        ("20261118-231139_baseline-m19-session", "20261118-231143_baseline-m19"),
        ("20261118-231408_baseline-m19-session", "20261118-231411_baseline-m19"),
        ("20261118-231627_baseline-m19-session", "20261118-231631_baseline-m19"),
        ("20261118-231958_baseline-m19-session", "20261118-232002_baseline-m19")]),
    dict(name="unicast A-B 2 M-19 (car-key 2.24)", dist=2.24, tunnel="unicast, A-B 2 m", campaigns=[
        ("20261118-225814_relay-m19-ab200-session", "20261118-225945_relay-m19-ab200"),
        ("20261118-230151_relay-m19-ab200-session", "20261118-230321_relay-m19-ab200"),
        ("20261118-230523_relay-m19-ab200-session", "20261118-230653_relay-m19-ab200"),
        ("20261118-230856_relay-m19-ab200-session", "20261118-231026_relay-m19-ab200"),
        ("20261118-231239_relay-m19-ab200-session", "20261118-231257_relay-m19-ab200"),
        ("20261118-231507_relay-m19-ab200-session", "20261118-231526_relay-m19-ab200"),
        ("20261118-231727_relay-m19-ab200-session", "20261118-231858_relay-m19-ab200"),
        ("20261118-232058_relay-m19-ab200-session", "20261118-232116_relay-m19-ab200")]),
    dict(name="baseline 2.24 held-out", dist=2.24, tunnel="direct", campaigns=[
        ("20261119-000248_baseline-ho-session", "20261119-000252_baseline-ho"),
        ("20261119-000405_baseline-ho-session", "20261119-000408_baseline-ho"),
        ("20261119-000732_baseline-ho-session", "20261119-000736_baseline-ho"),
        ("20261119-001100_baseline-ho-session", "20261119-001104_baseline-ho"),
        ("20261119-001316_baseline-ho-session", "20261119-001320_baseline-ho"),
        ("20261119-001644_baseline-ho-session", "20261119-001648_baseline-ho"),
        ("20261119-002011_baseline-ho-session", "20261119-002015_baseline-ho"),
        ("20261119-002340_baseline-ho-session", "20261119-002344_baseline-ho"),
        ("20261119-002707_baseline-ho-session", "20261119-002711_baseline-ho"),
        ("20261119-002924_baseline-ho-session", "20261119-002928_baseline-ho")]),
    dict(name="unicast A-B 2 held-out (car-key 2.24)", dist=2.24, tunnel="unicast, A-B 2 m", campaigns=[
        ("20261119-000505_relay-ho-ab200-session", "20261119-000635_relay-ho-ab200"),
        ("20261119-000833_relay-ho-ab200-session", "20261119-001003_relay-ho-ab200"),
        ("20261119-001201_relay-ho-ab200-session", "20261119-001219_relay-ho-ab200"),
        ("20261119-001417_relay-ho-ab200-session", "20261119-001547_relay-ho-ab200"),
        ("20261119-001744_relay-ho-ab200-session", "20261119-001914_relay-ho-ab200"),
        ("20261119-002112_relay-ho-ab200-session", "20261119-002243_relay-ho-ab200"),
        ("20261119-002440_relay-ho-ab200-session", "20261119-002610_relay-ho-ab200"),
        ("20261119-002808_relay-ho-ab200-session", "20261119-002826_relay-ho-ab200"),
        ("20261119-003025_relay-ho-ab200-session", "20261119-003043_relay-ho-ab200"),
        ("20261119-003207_relay-ho-ab200-session", "20261119-003225_relay-ho-ab200")]),
    dict(name="baseline 2.24 replication", dist=2.24, tunnel="direct", campaigns=[
        ("20261119-003839_baseline-rep-session", "20261119-003843_baseline-rep"),
        ("20261119-004102_baseline-rep-session", "20261119-004105_baseline-rep"),
        ("20261119-004431_baseline-rep-session", "20261119-004435_baseline-rep"),
        ("20261119-004647_baseline-rep-session", "20261119-004651_baseline-rep"),
        ("20261119-004902_baseline-rep-session", "20261119-004906_baseline-rep"),
        ("20261119-005118_baseline-rep-session", "20261119-005121_baseline-rep"),
        ("20261119-005445_baseline-rep-session", "20261119-005449_baseline-rep"),
        ("20261119-005700_baseline-rep-session", "20261119-005704_baseline-rep"),
        ("20261119-005915_baseline-rep-session", "20261119-005919_baseline-rep"),
        ("20261119-010131_baseline-rep-session", "20261119-010134_baseline-rep")]),
    dict(name="unicast A-B 2 replication (car-key 2.24)", dist=2.24, tunnel="unicast, A-B 2 m", campaigns=[
        ("20261119-003939_relay-rep-ab200-session", "20261119-004004_relay-rep-ab200"),
        ("20261119-004202_relay-rep-ab200-session", "20261119-004334_relay-rep-ab200"),
        ("20261119-004532_relay-rep-ab200-session", "20261119-004549_relay-rep-ab200"),
        ("20261119-004747_relay-rep-ab200-session", "20261119-004805_relay-rep-ab200"),
        ("20261119-005002_relay-rep-ab200-session", "20261119-005020_relay-rep-ab200"),
        ("20261119-005218_relay-rep-ab200-session", "20261119-005348_relay-rep-ab200"),
        ("20261119-005545_relay-rep-ab200-session", "20261119-005603_relay-rep-ab200"),
        ("20261119-005801_relay-rep-ab200-session", "20261119-005819_relay-rep-ab200"),
        ("20261119-010016_relay-rep-ab200-session", "20261119-010034_relay-rep-ab200"),
        ("20261119-010230_relay-rep-ab200-session", "20261119-010401_relay-rep-ab200")]),
]

COMPARISONS = [
    ("baseline 0.52", "unicast 0.52", "relayed unicast minus direct, car-key 0.52 m"),
    ("baseline 0.52", "broadcast 0.52", "relayed broadcast minus direct, car-key 0.52 m"),
    ("baseline 0.52", "unicast 2 dBm 0.52", "relayed unicast 2 dBm minus direct, car-key 0.52 m"),
    ("baseline 0.99", "unicast A-B 0.75 (car-key 0.99)", "relayed unicast minus direct, car-key 0.99 m"),
    ("baseline 2.24", "unicast A-B 2 (car-key 2.24)", "relayed unicast minus direct, car-key 2.24 m"),
    ("broadcast 0.52", "unicast 0.52", "unicast minus broadcast, car-key 0.52 m"),
    ("unicast 0.52", "unicast 2 dBm 0.52", "2 dBm minus default power, car-key 0.52 m"),
    ("baseline 2.24", "unicast A-B 2 TX 2 dBm (car-key 2.24)", "relayed unicast 2 dBm minus direct, car-key 2.24 m"),
    ("unicast A-B 2 (car-key 2.24)", "unicast A-B 2 TX 2 dBm (car-key 2.24)", "2 dBm minus default power, A-B 2 m"),
    ("baseline 0.52", "baseline 0.99", "direct baseline, 0.99 m minus 0.52 m"),
    ("baseline 0.99", "baseline 2.24", "direct baseline, 2.24 m minus 0.99 m"),
    ("baseline 2.24 M-19", "unicast A-B 2 M-19 (car-key 2.24)", "relayed unicast minus direct, car-key 2.24 m, M-19 firmware"),
    ("baseline 2.24 held-out", "unicast A-B 2 held-out (car-key 2.24)", "relayed unicast minus direct, car-key 2.24 m, held-out campaigns"),
    ("baseline 2.24 replication", "unicast A-B 2 replication (car-key 2.24)", "relayed unicast minus direct, car-key 2.24 m, replication campaigns"),
]


def load_campaign(session, campaign):
    folder = RUNS / session / campaign
    lines = []
    for f in sorted(folder.glob("capture_*.log")):
        lines += ar.read_text(f)
    chain = RUNS / session / CHAIN
    if chain.exists():
        lines += ar.read_text(chain)
    return lines, json.loads((folder / "manifest.json").read_text(encoding="utf-8"))


def build(spec):
    rounds, manifests, medians = [], [], []
    for session, campaign in spec["campaigns"]:
        lines, manifest = load_campaign(session, campaign)
        veh, a_rows, b_rows, _ = ar.parse_rows(lines)
        part = ar.join_rounds(veh, a_rows, b_rows)
        ok = [r["rtt"] for r in part if r["res"] == "OK"]
        medians.append(statistics.median(ok) if ok else None)
        rounds += part
        manifests.append(manifest)
    for r in rounds:
        r["condition"] = spec["name"]
        r["dist"] = f"{spec['dist']:.2f}"
    manifest = dict(manifests[0], dist_m=f"{spec['dist']:.2f}", label=spec["name"])
    report = ar.condition_report(spec["name"], rounds, manifest)
    report.update(
        tunnel=spec["tunnel"],
        campaigns=len(spec["campaigns"]),
        campaign_medians=medians,
        csvb=sum(r["turn_b_us"] is not None for r in rounds),
        tx_failures=sum((r["txfailures_a"] or 0) + (r["txfailures_b"] or 0) for r in rounds),
    )
    return report, rounds


def summary_markdown(conds):
    by = {c["name"]: c for c in conds}
    md = ["# Campaign summary, 2026-11-18", "",
          "Each condition pools the campaigns of one setup: three campaigns of 50 rounds, eight for the M-19 setups, and ten for the held-out and replication setups (random start offset of the round grid). Distance is the "
          "car-key separation. For relayed conditions, CSVB rounds equal the OK rounds (every answered round "
          "went through the relays).", ""]
    md += ["## Outcomes", "",
           "| Condition | Tunnel | Car-key (m) | Campaigns | Rounds | OK | FAIL | TIMEOUT | CSVB rounds | Success (95 % CI) |",
           "|---|---|---|---|---|---|---|---|---|---|"]
    for c in conds:
        lo, hi = c["success_ci"]
        md.append(f"| {c['name']} | {c['tunnel']} | {c['manifest']['dist_m']} | {c['campaigns']} | {c['n']} | "
                  f"{c['counts']['OK']} | {c['counts']['FAIL']} | {c['counts']['TIMEOUT']} | {c['csvb']} | "
                  f"{c['success'] * 100:.1f} % ({lo * 100:.1f}-{hi * 100:.1f}) |")
    md += ["", "## Vehicle RTT, answered rounds (ms)", "",
           "| Condition | n | min | Q1 | median (95 % CI) | Q3 | p90 | max | mean |",
           "|---|---|---|---|---|---|---|---|---|"]
    for c in conds:
        d = c["rtt"]
        ci = d["med_ci"]
        md.append(f"| {c['name']} | {d['n']} | {ar.fmt_ms(d['min'])} | {ar.fmt_ms(d['q1'])} | "
                  f"{ar.fmt_ms(d['med'])} ({ar.fmt_ms(ci[0])}-{ar.fmt_ms(ci[1])}) | {ar.fmt_ms(d['q3'])} | "
                  f"{ar.fmt_ms(d['p90'])} | {ar.fmt_ms(d['max'])} | {ar.fmt_ms(d['mean'])} |")
    width = max(len(c["campaign_medians"]) for c in conds)
    md += ["", "## Campaign spread: median RTT of OK rounds per campaign (ms)", "",
           "| Condition | " + " | ".join(f"campaign {i + 1}" for i in range(width)) + " |",
           "|---|" + "---|" * width]
    for c in conds:
        cells = ["-" if m is None else ar.fmt_ms(m) for m in c["campaign_medians"]]
        cells += ["-"] * (width - len(cells))
        md.append(f"| {c['name']} | " + " | ".join(cells) + " |")
    md += ["", "## Round legs, median (IQR) in ms", "",
           "| Condition | vehicle<->B | tunnel | A<->key |", "|---|---|---|---|"]
    for c in conds:
        cells = []
        for leg in ("vehicle<->B", "tunnel", "A<->key"):
            d = c["legs"].get(leg)
            cells.append("-" if not d else f"{ar.fmt_ms(d['med'])} ({ar.fmt_ms(d['q1'])}-{ar.fmt_ms(d['q3'])})")
        md.append(f"| {c['name']} | " + " | ".join(cells) + " |")
    md += ["", "## Tunnel counters", "",
           "| Condition | tx_failures (relay A + B) | TIMEOUT |", "|---|---|---|"]
    for c in conds:
        md.append(f"| {c['name']} | {c['tx_failures']} | {c['counts']['TIMEOUT']} |")
    md += ["", "## Comparisons (difference of medians, ms)", "",
           "| Comparison | Reference median | Compared median | Difference (95 % CI) |", "|---|---|---|---|"]
    for ref, other, text in COMPARISONS:
        a, b = by[ref]["answered"], by[other]["answered"]
        diff = statistics.median(b) - statistics.median(a)
        lo, hi = ar.boot_diff_ci(a, b)
        md.append(f"| {text} | {ar.fmt_ms(statistics.median(a))} | {ar.fmt_ms(statistics.median(b))} | "
                  f"{ar.fmt_ms(diff)} ({ar.fmt_ms(lo)} to {ar.fmt_ms(hi)}) |")
    md += ["", "## Notes", "",
           "- TIMEOUT rounds are excluded from the RTT statistics and counted in the success rate.",
           "- Wilson 95 % CI for the success rate; bootstrap (2000 resamples, fixed seed) for medians and "
           "differences.",
           "- Round legs follow analyze_runs.py: vehicle<->B = vehicle RTT - turnaround of B; tunnel = "
           "turnaround of B - turnaround of A; A<->key = turnaround of A.",
           "- Broadcast sessions ran with RELAY_PEER_MAC unset; unicast sessions with it set (branch "
           "relay-unicast-peer-mac).",
           "- 2 dBm sessions applied TXPOW 8 to both relays after the bring-up; the relay logs confirm it.",
           "- Car-key separations of 2.24 m are estimated from the layout (12 cm + about 2 m + 12 cm).",
           "- Rounds are not independent: the RTT moves in plateaus within a campaign. The bootstrap "
           "intervals assume independent rounds, so they are optimistic.",
           "- The spread between campaigns (campaign spread table) is larger than most differences between "
           "settings. Three campaigns per condition do not resolve differences below about 50 ms; the M-19 setups have eight and the held-out and replication setups ten."]
    return "\n".join(md) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=str(REPO / "data" / "analysis-20261118"))
    args = ap.parse_args(argv)
    out = pathlib.Path(args.out)
    conds, all_rounds = [], []
    for spec in CONDITIONS:
        report, rounds = build(spec)
        conds.append(report)
        all_rounds += rounds
    ar.write_outputs(out, conds, all_rounds)
    (out / "summary.md").write_text(summary_markdown(conds), encoding="utf-8")
    print(f"outputs in: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
