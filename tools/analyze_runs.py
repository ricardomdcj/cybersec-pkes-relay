#!/usr/bin/env python3
"""Joins the rows of the campaigns and computes the statistics (D-04a and D-04b).

Input: campaign folders (data/runs/<id>/, containing capture_*.log) or single capture files
(old logs in data/). Each input becomes one condition (label + distance).

Rows are joined per round by the nonce (4 bytes): the vehicle lines (CSV), Relay A (CSVA) and
Relay B (CSVB). Each round is then broken down into legs:
  vehicle<->B  = vehicle RTT - turnaround of B   (BLE between the vehicle and B, round trip)
  tunnel       = turnaround of B - turnaround of A  (ESP-NOW round trip + queueing)
  A<->key      = turnaround of A                 (BLE between A and the key + HMAC)

Statistics: counts per result, Wilson CI for the success rate, median, quartiles, p90 and a
95 % bootstrap CI for the median. Across conditions: difference of medians (overhead) with a
bootstrap CI. The bootstrap uses a fixed seed, so it is reproducible.

Both the current (English) and the older (Portuguese) log formats are accepted: VEHICLE or
VEICULO, FAIL or FALHA, and CSV v1 or v2.

Usage (ESP-IDF Python with -I, or any Python 3):
  python -I tools/analyze_runs.py data/runs/<baseline> data/runs/<relay> --out data/analysis
  python -I tools/analyze_runs.py data/baseline_20261116-220429.log data/relayed_20261116-220429.log

Output: rounds.csv (one row per round) and summary.md (tables) in the --out folder.
"""

import argparse
import csv
import json
import math
import pathlib
import random
import re
import statistics
import sys

RE_VEH = re.compile(r"(?:VEHICLE|VEICULO): CSV,(.*)$")
RE_A = re.compile(r"RELAY_A: CSVA,(.*)$")
RE_B = re.compile(r"RELAY_B: CSVB,(.*)$")
RE_SUM = re.compile(r"(?:VEHICLE|VEICULO): (=== .*)$")
BOOTSTRAP_N = 2000
SEED = 20261117
RESULT_MAP = {"FALHA": "FAIL"}   # older logs used the Portuguese word


def read_text(path):
    raw = pathlib.Path(path).read_bytes()
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):          # UTF-16 (older captures)
        text = raw.decode("utf-16")
    else:
        text = raw.decode("utf-8-sig", errors="replace")
    return text.replace("\r", "").splitlines()


def load_condition(path):
    """Returns (name, manifest or {}, list of capture files)."""
    p = pathlib.Path(path)
    if p.is_dir():
        manifest_path = p / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
        files = sorted(p.glob("capture_*.log")) or sorted(p.glob("*.log"))
        name = manifest.get("label") or p.name
        return name, manifest, files
    return p.stem, {}, [p]


def parse_rows(lines):
    """Extracts the CSV rows of a log. Accepts the v1 and v2 vehicle formats and the relay
    format (CSVB with 4, 5 or 6 fields)."""
    veh, a_rows, b_rows, summaries = [], {}, {}, []
    for line in lines:
        m = RE_VEH.search(line)
        if m:
            f = m.group(1).split(",")
            if len(f) >= 10:          # v2: label,dist,round,nonce,t_tx,rtt,result,rssi,itvl,mtu
                veh.append(dict(label=f[0], dist=f[1], round=int(f[2]), nonce=f[3],
                                t_tx=int(f[4]), rtt=int(f[5]),
                                res=RESULT_MAP.get(f[6], f[6]), rssi=int(f[7]),
                                itvl=int(f[8]), mtu=int(f[9])))
            elif len(f) >= 6:         # v1: label,round,nonce,rtt,result,rssi
                veh.append(dict(label=f[0], dist="-", round=int(f[1]), nonce=f[2],
                                t_tx=None, rtt=int(f[3]),
                                res=RESULT_MAP.get(f[4], f[4]), rssi=int(f[5]),
                                itvl=None, mtu=None))
            continue
        m = RE_A.search(line)
        if m:
            f = m.group(1).split(",")
            a_rows[f[1]] = dict(seq=int(f[0]), turn_a=int(f[2]), rssi=int(f[3]),
                                txfailures=int(f[4]) if len(f) >= 5 else None)
            continue
        m = RE_B.search(line)
        if m:
            f = m.group(1).split(",")
            b_rows[f[1]] = dict(seq=int(f[0]), turn_b=int(f[2]), rssi=int(f[3]),
                                turn_a=int(f[4]) if len(f) >= 5 else None,
                                txfailures=int(f[5]) if len(f) >= 6 else None)
            continue
        m = RE_SUM.search(line)
        if m:
            summaries.append(m.group(1))
    return veh, a_rows, b_rows, summaries


def join_rounds(veh, a_rows, b_rows):
    """One row per vehicle round, with the relay legs when they exist."""
    out = []
    for v in veh:
        a = a_rows.get(v["nonce"])
        b = b_rows.get(v["nonce"])
        rec = dict(v)
        rec["turn_b_us"] = b["turn_b"] if b else None
        rec["turn_a_us"] = (b["turn_a"] if b and b["turn_a"] is not None
                            else (a["turn_a"] if a else None))
        rec["txfailures_b"] = b["txfailures"] if b else None
        rec["txfailures_a"] = a["txfailures"] if a else None
        if rec["turn_b_us"] is not None and rec["turn_a_us"] is not None:
            rec["tunnel_us"] = rec["turn_b_us"] - rec["turn_a_us"]
        else:
            rec["tunnel_us"] = None
        if rec["turn_b_us"] is not None and v["res"] != "TIMEOUT" and v["rtt"] > 0:
            rec["veh_b_us"] = v["rtt"] - rec["turn_b_us"]
        else:
            rec["veh_b_us"] = None
        out.append(rec)
    return out


# ---------- statistics (stdlib only) -----------------------------------------

def quantile(xs, q):
    """Quantile with linear interpolation (the numpy default)."""
    s = sorted(xs)
    if not s:
        return None
    pos = (len(s) - 1) * q
    lo, hi = math.floor(pos), math.ceil(pos)
    return s[lo] + (s[hi] - s[lo]) * (pos - lo)


def wilson(k, n, z=1.959964):
    if n == 0:
        return None, None
    p = k / n
    den = 1 + z * z / n
    centre = (p + z * z / (2 * n)) / den
    margin = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / den
    return max(0.0, centre - margin), min(1.0, centre + margin)


def boot_median_ci(xs, seed=SEED):
    if len(xs) < 2:
        return None, None
    rng = random.Random(seed)
    n = len(xs)
    meds = sorted(statistics.median(rng.choices(xs, k=n)) for _ in range(BOOTSTRAP_N))
    return meds[int(0.025 * BOOTSTRAP_N)], meds[int(0.975 * BOOTSTRAP_N)]


def boot_diff_ci(a, b, seed=SEED):
    """95 % bootstrap CI of the difference of medians (b - a), independent samples."""
    if len(a) < 2 or len(b) < 2:
        return None, None
    rng = random.Random(seed)
    diffs = []
    for _ in range(BOOTSTRAP_N):
        ra = rng.choices(a, k=len(a))
        rb = rng.choices(b, k=len(b))
        diffs.append(statistics.median(rb) - statistics.median(ra))
    diffs.sort()
    return diffs[int(0.025 * BOOTSTRAP_N)], diffs[int(0.975 * BOOTSTRAP_N)]


def fmt_ms(us):
    return "-" if us is None else f"{us / 1000:.1f}"


def describe(xs):
    if not xs:
        return None
    med = statistics.median(xs)
    lo, hi = boot_median_ci(xs)
    return dict(n=len(xs), min=min(xs), med=med, q1=quantile(xs, .25), q3=quantile(xs, .75),
                p90=quantile(xs, .90), max=max(xs), mean=statistics.mean(xs),
                sd=statistics.pstdev(xs) if len(xs) > 1 else 0.0,
                med_ci=(lo, hi))


# ---------- report ------------------------------------------------------------

def condition_report(name, rounds, manifest):
    res_counts = {"OK": 0, "FAIL": 0, "TIMEOUT": 0}
    for r in rounds:
        res_counts[r["res"]] = res_counts.get(r["res"], 0) + 1
    n = len(rounds)
    answered = [r["rtt"] for r in rounds if r["res"] != "TIMEOUT" and r["rtt"] > 0]
    ok_rtt = [r["rtt"] for r in rounds if r["res"] == "OK" and r["rtt"] > 0]
    lo, hi = wilson(res_counts["OK"], n)
    legs = {
        "vehicle<->B": [r["veh_b_us"] for r in rounds if r["veh_b_us"] is not None],
        "tunnel": [r["tunnel_us"] for r in rounds if r["tunnel_us"] is not None],
        "A<->key": [r["turn_a_us"] for r in rounds if r["turn_a_us"] is not None],
    }
    return dict(name=name, manifest=manifest, n=n, counts=res_counts,
                success=(res_counts["OK"] / n if n else None), success_ci=(lo, hi),
                rtt=describe(answered), rtt_ok=ok_rtt, answered=answered,
                legs={k: describe(v) for k, v in legs.items() if v})


def write_outputs(out_dir, conds, all_rounds):
    out_dir.mkdir(parents=True, exist_ok=True)
    cols = ["condition", "label", "dist_m", "round", "nonce", "t_tx_us", "result",
            "rtt_us", "rssi_dbm", "itvl_1_25ms", "mtu", "turn_b_us", "turn_a_us",
            "tunnel_us", "vehicle_b_us", "txfailures_a", "txfailures_b"]
    with open(out_dir / "rounds.csv", "w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(cols)
        for r in all_rounds:
            w.writerow([r["condition"], r["label"], r["dist"], r["round"], r["nonce"],
                        r["t_tx"], r["res"], r["rtt"], r["rssi"], r["itvl"], r["mtu"],
                        r["turn_b_us"], r["turn_a_us"], r["tunnel_us"], r["veh_b_us"],
                        r["txfailures_a"], r["txfailures_b"]])

    md = ["# Campaign summary", ""]
    md += ["| Condition | Dist (m) | Rounds | OK | FAIL | TIMEOUT | Success (95 % CI) |",
           "|---|---|---|---|---|---|---|"]
    for c in conds:
        lo, hi = c["success_ci"]
        suc = "-" if c["success"] is None else f"{c['success']*100:.1f} % ({lo*100:.1f}-{hi*100:.1f})"
        md.append(f"| {c['name']} | {c['manifest'].get('dist_m', '-')} | {c['n']} | "
                  f"{c['counts']['OK']} | {c['counts']['FAIL']} | {c['counts']['TIMEOUT']} | {suc} |")
    md += ["", "## Vehicle RTT (answered rounds, ms)", "",
           "| Condition | n | min | Q1 | median (95 % CI) | Q3 | p90 | max | mean | sd |",
           "|---|---|---|---|---|---|---|---|---|---|"]
    for c in conds:
        d = c["rtt"]
        if not d:
            md.append(f"| {c['name']} | 0 | | | | | | | | |")
            continue
        ci = d["med_ci"]
        md.append(f"| {c['name']} | {d['n']} | {fmt_ms(d['min'])} | {fmt_ms(d['q1'])} | "
                  f"{fmt_ms(d['med'])} ({fmt_ms(ci[0])}-{fmt_ms(ci[1])}) | {fmt_ms(d['q3'])} | "
                  f"{fmt_ms(d['p90'])} | {fmt_ms(d['max'])} | {fmt_ms(d['mean'])} | {fmt_ms(d['sd'])} |")

    md += ["", "## Round legs (median, ms)", "",
           "| Condition | vehicle<->B | tunnel | A<->key |", "|---|---|---|---|"]
    for c in conds:
        cell = []
        for leg in ("vehicle<->B", "tunnel", "A<->key"):
            d = c["legs"].get(leg)
            cell.append("-" if not d else f"{fmt_ms(d['med'])} (IQR {fmt_ms(d['q1'])}-{fmt_ms(d['q3'])})")
        md.append(f"| {c['name']} | " + " | ".join(cell) + " |")

    base = conds[0]
    if len(conds) > 1 and base["rtt"]:
        md += ["", f"## Overhead relative to '{base['name']}' (difference of medians, ms)", "",
               "| Condition | difference of medians (95 % CI) |", "|---|---|"]
        for c in conds[1:]:
            if not c["rtt"]:
                continue
            lo, hi = boot_diff_ci(base["answered"], c["answered"])
            diff = statistics.median(c["answered"]) - statistics.median(base["answered"])
            md.append(f"| {c['name']} | {fmt_ms(diff)} ({fmt_ms(lo)} to {fmt_ms(hi)}) |")

    md += ["", "Notes: TIMEOUT rounds are excluded from the RTT statistics and included in the "
           "success rate. CI of the median and of the difference by bootstrap (2000 resamples, "
           f"seed {SEED}). CI of the rate by Wilson 95 %."]
    (out_dir / "summary.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return "\n".join(md)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="campaign folders or capture files")
    ap.add_argument("--out", default=None, help="output folder (default: analysis/ next to the first input)")
    args = ap.parse_args(argv)

    conds, all_rounds = [], []
    for entry in args.inputs:
        name, manifest, files = load_condition(entry)
        lines = []
        for f in files:
            lines += read_text(f)
        veh, a_rows, b_rows, summaries = parse_rows(lines)
        rounds = join_rounds(veh, a_rows, b_rows)
        if not rounds:
            print(f"warning: no vehicle CSV rows in {entry}", file=sys.stderr)
            continue
        for r in rounds:
            r["condition"] = name
            r["dist"] = manifest.get("dist_m") if r["dist"] in (None, "-") else r["dist"]
        conds.append(condition_report(name, rounds, manifest))
        all_rounds += rounds

    if not conds:
        print("no condition with data", file=sys.stderr)
        return 1

    out_dir = pathlib.Path(args.out) if args.out else pathlib.Path(args.inputs[0]).resolve().parent / "analysis"
    print(write_outputs(out_dir, conds, all_rounds))
    print(f"\noutputs in: {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
