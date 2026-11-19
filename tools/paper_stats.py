#!/usr/bin/env python3
"""Statistics for the paper's results: campaign-level tests, effect sizes, bootstrap, power.

The unit of inference is the campaign. Rounds inside one campaign share one phase of the
connection grid (cutoff.md, section 1), so round-level p-values would be pseudo-replicated.
The script therefore:
  - tests the campaign medians (Mann-Whitney; exact for small samples, normal approximation
    otherwise) and reports Cliff's delta, which equals 2*AUC - 1;
  - bootstraps by resampling campaigns within each condition (2000 resamples, fixed seed);
  - corrects the campaign-level p-values across the comparisons with Holm;
  - reports round-level Cliff's delta and AUC as descriptive values only;
  - gives the sample size needed for a difference Delta equal to half the BLE connection
    interval (25 ms, the definition in the paper's method), from the observed campaign spread.

Pure Python: the bench's Python has no numpy, scipy or statsmodels.

Writes data/analysis-20261118/paper_stats.md.
Usage (from the repo root): python -I tools/paper_stats.py
"""

import math
import pathlib
import random
import statistics
import sys
from itertools import combinations

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import analyze_cutoff as ac2  # noqa: E402  (reuses the campaign loader and the RTT convention)

REPO = HERE.parent
OUT = REPO / "data" / "analysis-20261118" / "paper_stats.md"
SEED = 20261119
BOOT = 2000
Z975 = 1.959964
Z80 = 0.841621
DELTA_MS = 25.0  # half the BLE connection interval (50 ms), the paper's definition of Delta

# (label, direct campaign name, relayed campaign name)
GROUPS = [
    ("2.24 m, M-19 in-sample", "baseline 2.24 M-19", "unicast A-B 2 M-19 (car-key 2.24)"),
    ("2.24 m, held-out", "baseline 2.24 held-out", "unicast A-B 2 held-out (car-key 2.24)"),
    ("2.24 m, replication", "baseline 2.24 replication", "unicast A-B 2 replication (car-key 2.24)"),
    ("2.24 m, pre-M-19 firmware", "baseline 2.24", "unicast A-B 2 (car-key 2.24)"),
    ("0.99 m, pre-M-19 firmware", "baseline 0.99", "unicast A-B 0.75 (car-key 0.99)"),
    ("0.52 m, pre-M-19 firmware", "baseline 0.52", "unicast 0.52"),
]
POOLED_LABEL = "2.24 m, M-19 family pooled"  # the three M-19-firmware sets together (not in Holm)


def campaign_record(camp):
    rounds = camp["rounds"]
    answered = [r["rtt"] / 1000.0 for r in rounds if r["res"] != "TIMEOUT" and r["rtt"] > 0]
    rssi = [r["rssi"] for r in rounds if r["res"] != "TIMEOUT" and r.get("rssi")]
    return dict(name=camp["name"], campaign=camp["campaign"], rtt=answered, rssi=rssi,
                timeouts=sum(1 for r in rounds if r["res"] == "TIMEOUT"), n=len(rounds),
                median=statistics.median(answered) if answered else float("nan"))


def auc(x, y):
    """P(X > Y) + 0.5 P(X = Y), by average ranks. Cliff's delta of X against Y is 2*auc - 1."""
    n, m = len(x), len(y)
    vals = sorted(list(x) + list(y))
    first, last = {}, {}
    for i, v in enumerate(vals):
        first.setdefault(v, i)
        last[v] = i
    rank = {v: (first[v] + last[v]) / 2 + 1 for v in first}
    rx = sum(rank[v] for v in x)
    return (rx - n * (n + 1) / 2) / (n * m)


def exact_mw_p(x, y):
    """Two-sided exact Mann-Whitney p-value (no ties), by enumerating rank sums."""
    n, m = len(x), len(y)
    order = sorted(x + y)
    rank = {v: i + 1 for i, v in enumerate(order)}
    s_obs = sum(rank[v] for v in x)
    counts = {}
    total = 0
    for comb in combinations(range(1, n + m + 1), n):
        s = sum(comb)
        counts[s] = counts.get(s, 0) + 1
        total += 1
    lo = sum(c for s, c in counts.items() if s <= s_obs) / total
    hi = sum(c for s, c in counts.items() if s >= s_obs) / total
    return min(1.0, 2 * min(lo, hi))


def mann_whitney_p(x, y):
    """Two-sided p-value for campaign medians: exact when small, normal approximation otherwise."""
    n, m = len(x), len(y)
    if math.comb(n + m, n) <= 200000 and len(set(x + y)) == n + m:
        return exact_mw_p(x, y), "exact"
    u = auc(x, y) * n * m
    mu = n * m / 2
    sd = math.sqrt(n * m * (n + m + 1) / 12)
    z = (u - mu) / sd
    p = math.erfc(abs(z) / math.sqrt(2))
    return p, "normal"


def holm(pvals):
    order = sorted(range(len(pvals)), key=lambda i: pvals[i])
    adj = [0.0] * len(pvals)
    running = 0.0
    k = len(pvals)
    for rank, i in enumerate(order):
        running = max(running, min(1.0, (k - rank) * pvals[i]))
        adj[i] = running
    return adj


def cluster_boot(d_camps, r_camps, stat, reps=BOOT, seed=SEED):
    """Resample campaigns with replacement within each condition; return 2.5 and 97.5 percentiles."""
    rng = random.Random(seed)
    out = []
    for _ in range(reps):
        d = [rng.choice(d_camps) for _ in d_camps]
        r = [rng.choice(r_camps) for _ in r_camps]
        v = stat(d, r)
        if v == v:  # skip NaN
            out.append(v)
    out.sort()
    return out[int(0.025 * len(out))], out[int(0.975 * len(out)) - 1]


def pool(camps, key):
    vals = []
    for c in camps:
        vals += c[key]
    return vals


def st_overhead(d, r):
    return statistics.median(pool(r, "rtt")) - statistics.median(pool(d, "rtt"))


def st_auc_rounds_rtt(d, r):
    return auc(pool(r, "rtt"), pool(d, "rtt"))


def st_auc_rounds_rssi(d, r):
    return auc(pool(r, "rssi"), pool(d, "rssi"))


def st_auc_campaign_rtt(d, r):
    return auc([c["median"] for c in r], [c["median"] for c in d])


def st_auc_campaign_rssi(d, r):
    return auc([statistics.median(c["rssi"]) for c in r], [statistics.median(c["rssi"]) for c in d])


def n_per_group(sigma, delta=DELTA_MS):
    return 2 * (Z975 + Z80) ** 2 * sigma ** 2 / delta ** 2


def f1(v):
    return "n/a" if v is None or v != v else f"{v:.1f}"


def f3(v):
    return "n/a" if v is None or v != v else f"{v:.3f}"


def main(argv=None):
    camps = ac2.load_campaigns()
    by_name = {}
    for c in camps:
        by_name.setdefault(c["name"], []).append(campaign_record(c))

    md = ["# Statistics for the results (campaign as the unit)", "",
          "Unit: the campaign. Rounds inside a campaign share one phase of the connection grid, so the tests "
          "use campaign medians, and the bootstrap resamples campaigns within each condition (2000 resamples, "
          f"seed {SEED}). Cliff's delta equals 2 x AUC - 1; a positive value means the relayed rounds are "
          "longer. Round-level values are descriptive only.", ""]

    # 1. overhead and campaign-level tests per group
    md += ["## 1. Overhead and campaign-level comparison", "",
           "| Group | Campaigns (direct, relayed) | Median direct (ms) | Median relayed (ms) | Overhead (ms), 95 % CI | "
           "Campaign-level MW p (test) | Cliff's delta, campaigns, 95 % CI | Round-level Cliff's delta (descriptive) |",
           "|---|---|---|---|---|---|---|---|"]
    pvals, labels = [], []
    rows = {}
    for label, dname, rname in GROUPS:
        d_c = by_name.get(dname, [])
        r_c = by_name.get(rname, [])
        if not d_c or not r_c:
            md.append(f"| {label} | missing | - | - | - | - | - | - |")
            continue
        d_med = [c["median"] for c in d_c]
        r_med = [c["median"] for c in r_c]
        p, how = mann_whitney_p(r_med, d_med)
        delta_c = 2 * auc(r_med, d_med) - 1
        lo_o, hi_o = cluster_boot(d_c, r_c, st_overhead)
        lo_d, hi_d = cluster_boot(d_c, r_c, lambda d, r: 2 * st_auc_campaign_rtt(d, r) - 1)
        round_delta = 2 * st_auc_rounds_rtt(d_c, r_c) - 1
        md.append(f"| {label} | {len(d_c)}, {len(r_c)} | {f1(statistics.median(pool(d_c, 'rtt')))} | "
                  f"{f1(statistics.median(pool(r_c, 'rtt')))} | "
                  f"{f1(st_overhead(d_c, r_c))} ({f1(lo_o)} to {f1(hi_o)}) | "
                  f"{p:.4f} ({how}) | {f3(delta_c)} ({f3(lo_d)} to {f3(hi_d)}) | {f3(round_delta)} |")
        rows[label] = (d_c, r_c, p)
        pvals.append(p)
        labels.append(label)

    # pooled M-19 family (descriptive)
    fam_d = [c for lab in ("2.24 m, M-19 in-sample", "2.24 m, held-out", "2.24 m, replication")
             for c in by_name.get(dict((g[0], g[1]) for g in GROUPS)[lab], [])]
    fam_r = [c for lab in ("2.24 m, M-19 in-sample", "2.24 m, held-out", "2.24 m, replication")
             for c in by_name.get(dict((g[0], g[2]) for g in GROUPS)[lab], [])]
    if fam_d and fam_r:
        d_med = [c["median"] for c in fam_d]
        r_med = [c["median"] for c in fam_r]
        p, how = mann_whitney_p(r_med, d_med)
        lo_o, hi_o = cluster_boot(fam_d, fam_r, st_overhead)
        lo_d, hi_d = cluster_boot(fam_d, fam_r, lambda d, r: 2 * st_auc_campaign_rtt(d, r) - 1)
        md.append(f"| {POOLED_LABEL} | {len(fam_d)}, {len(fam_r)} | {f1(statistics.median(pool(fam_d, 'rtt')))} | "
                  f"{f1(statistics.median(pool(fam_r, 'rtt')))} | "
                  f"{f1(st_overhead(fam_d, fam_r))} ({f1(lo_o)} to {f1(hi_o)}) | {p:.2e} ({how}) | "
                  f"{f3(2 * auc(r_med, d_med) - 1)} ({f3(lo_d)} to {f3(hi_d)}) | "
                  f"{f3(2 * st_auc_rounds_rtt(fam_d, fam_r) - 1)} |")

    # 2. Holm correction across the independent groups
    adj = holm(pvals)
    md += ["", "## 2. Holm correction across the groups (campaign-level p-values)", "",
           "Family: the six groups in table 1 (the pooled row is not included, to avoid counting the same campaigns twice).",
           "", "| Group | Raw p | Holm-adjusted p |", "|---|---|---|"]
    for lab, p, a in zip(labels, pvals, adj):
        md.append(f"| {lab} | {p:.4f} | {a:.4f} |")
    md += ["", "With three campaigns per side, the smallest two-sided exact p-value is 2/20 = 0.10: the pre-M-19 sets "
           "cannot reach p < 0.05 with any outcome. The M-19 sets (8 or 10 campaigns per side) can.", ""]

    # 3. RQ3: discrimination by RTT and RSSI, campaign level and round level
    md += ["## 3. Discrimination by RTT and RSSI (RQ3), 2.24 m, M-19 family", "",
           "AUC is P(relayed > direct) with ties counted half; 0.5 means no discrimination, 1.0 perfect separation "
           "in the direction relayed > direct. Campaign-level AUC uses one value per campaign; the interval is the "
           "cluster bootstrap over campaigns.", "",
           "| Signal | Level | AUC (95 % CI) |", "|---|---|---|"]
    d_fam = fam_d
    r_fam = fam_r
    if d_fam and r_fam:
        lo, hi = cluster_boot(d_fam, r_fam, st_auc_campaign_rtt)
        md.append(f"| RTT (median per campaign) | campaign, 28 vs 28 | {f3(st_auc_campaign_rtt(d_fam, r_fam))} ({f3(lo)} to {f3(hi)}) |")
        lo, hi = cluster_boot(d_fam, r_fam, st_auc_campaign_rssi)
        md.append(f"| RSSI (median per campaign) | campaign, 28 vs 28 | {f3(st_auc_campaign_rssi(d_fam, r_fam))} ({f3(lo)} to {f3(hi)}) |")
        lo, hi = cluster_boot(d_fam, r_fam, st_auc_rounds_rssi)
        md.append(f"| RSSI (rounds) | round, descriptive | {f3(st_auc_rounds_rssi(d_fam, r_fam))} ({f3(lo)} to {f3(hi)}) |")
        md.append(f"| RTT (rounds) | round, descriptive | {f3(st_auc_rounds_rtt(d_fam, r_fam))} |")
    md += ["", "The paper's RQ3 criterion (AUC above 0.5 in every campaign) cannot be applied as written: each "
           "campaign is a single condition, so a per-campaign AUC has one class only. The campaign-level AUC above "
           "replaces it.", ""]

    # 4. failures per configuration
    md += ["## 4. Timeouts per configuration (H2)", "",
           "| Condition | Campaigns | Rounds | Timeouts | Timeouts per 100 rounds |", "|---|---|---|---|---|"]
    for name in sorted(by_name):
        cs = by_name[name]
        n_rounds = sum(c["n"] for c in cs)
        n_to = sum(c["timeouts"] for c in cs)
        md.append(f"| {name} | {len(cs)} | {n_rounds} | {n_to} | {n_to / n_rounds * 100:.1f} |")
    bcast = [c for c in by_name.get("broadcast 0.52", [])]
    if bcast:
        t = sum(c["timeouts"] for c in bcast)
        n = sum(c["n"] for c in bcast)
        md += ["", f"Broadcast relay (no MAC ACK), 0.52 m: {t} timeouts in {n} rounds ({t / n * 100:.1f} %). "
               "The unicast relay (MAC ACK) had none at any distance. Broadcast was only run at 0.52 m, so the "
               "failure cannot be separated from the distance on this bench; it does point to the transport "
               "(lost frames without acknowledgement) rather than to the distance (H2).", ""]

    # 5. power
    md += ["## 5. Sample size for Delta = 25 ms (half the BLE connection interval)", "",
           "n per group = 2 (z(0.975) + z(0.80))^2 sigma^2 / Delta^2, with sigma the campaign-to-campaign "
           "standard deviation of the campaign medians (alpha 0.05, two-sided, power 0.80).", "",
           "| Comparison | sigma (ms) | n per group for Delta = 25 ms |", "|---|---|---|"]
    def sd_of(camps_list):
        meds = [c["median"] for c in camps_list]
        return statistics.stdev(meds) if len(meds) > 1 else float("nan")
    if fam_d and fam_r:
        sd_d = sd_of(fam_d)
        sd_r = sd_of(fam_r)
        pooled_sd = math.sqrt((sd_d ** 2 + sd_r ** 2) / 2)
        md.append(f"| Direct campaigns (M-19 family) | {sd_d:.1f} | {n_per_group(sd_d):.1f} |")
        md.append(f"| Relayed campaigns (M-19 family) | {sd_r:.1f} | {n_per_group(sd_r):.1f} |")
        md.append(f"| Two relayed configurations (relayed SD) | {sd_r:.1f} | {n_per_group(sd_r):.1f} |")
        md.append(f"| Direct vs relayed (pooled SD, overhead about 300 ms) | {pooled_sd:.1f} | "
                  f"{n_per_group(pooled_sd):.1f} (overhead is far larger than Delta) |")
    md += ["", "The paper's current wording ('30 repetitions per cell') underestimates the campaigns needed to resolve "
           "25 ms between two relayed configurations, because the campaign-to-campaign spread of relayed medians is "
           "about ten times the spread of the direct ones (the phase of each campaign). The overhead itself is "
           "resolved with a handful of campaigns.", ""]

    md += ["## Not covered here", "",
           "- Logistic regression of the decision on distance and T_max (the paper's RQ2 model): needs the "
           "per-round decision for each T_max, which the bench only yields by applying cutoffs (cutoff.md, "
           "sections 3 to 9). Not fitted.",
           "- Mixed model with campaign as a random effect: approximated here by the campaign-level tests and "
           "the cluster bootstrap. A fitted mixed model needs statsmodels, not installed on the bench.", ""]

    OUT.write_text("\n".join(md) + "\n", encoding="utf-8")
    print("\n".join(md))
    print(f"\nwritten: {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
