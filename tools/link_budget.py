#!/usr/bin/env python3
"""Link-budget estimate of how far the relay tunnel and the BLE legs can reach.

Reads the BLE RSSI of the direct baseline (car to key at 0.52, 0.99 and 2.24 m) and reports the
local path-loss exponents between those distances. Those exponents do not agree (see the output),
so the bench cannot calibrate one exponent. The ranges therefore use n = 2, 3 and 4 and the
datasheet-typical parameters below. The output is a model estimate, not a measurement: the bench
never separated the boards by more than about 2 m.

Usage (from the repo root): python -I tools/link_budget.py [--out data/analysis-20261118/link_budget.md]
"""

import argparse
import csv
import math
import pathlib
import statistics
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
C = 299792458.0
FREQ_HZ = 2.412e9                      # Wi-Fi channel 1, the channel the relays use
BASELINES = {"baseline 0.52": 0.52, "baseline 0.99": 0.99, "baseline 2.24": 2.24}
ESPNOW_TX_DBM = {"ESP-NOW tunnel, TX 19.5 dBm (default, typical)": 19.5, "ESP-NOW tunnel, TX 2 dBm (lowest used)": 2.0}
BLE_TX_DBM = 0.0                       # ESP32 datasheet v5.3, Table 5-8: RF transmit power, typical 0 dBm
SENS_ESPNOW_DBM = -98.0                # ESP32 datasheet v5.3, Table 5-6: 802.11b 1 Mbps sensitivity, typical
SENS_BLE_DBM = -89.0                   # ESP32 datasheet v5.3, Table 5-7: basic data rate (GFSK, = BLE 1M), typical
FADE_MARGIN_DB = 10.0                  # assumed
ANTENNA_GAIN_DBI = 0.0                 # assumed, per end
EXPONENTS = [("2 (open, line of sight)", 2.0), ("3 (indoor, obstructed)", 3.0), ("4 (dense indoor)", 4.0)]


def fspl_db(d_m):
    return 20 * math.log10(4 * math.pi * d_m * FREQ_HZ / C)


PL0 = fspl_db(1.0)


def local_exponent(d1, r1, d2, r2):
    return (r1 - r2) / (10 * math.log10(d2 / d1))


def range_m(p_tx_dbm, sens_dbm, n):
    budget = p_tx_dbm + 2 * ANTENNA_GAIN_DBI - sens_dbm - FADE_MARGIN_DB - PL0
    return 10 ** (budget / (10 * n))


def fmt_range(m):
    return f"{m / 1000:.1f} km" if m >= 1000 else f"{m:.0f} m"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rounds", default=str(REPO / "data" / "analysis-20261118" / "rounds.csv"))
    ap.add_argument("--out", default=str(REPO / "data" / "analysis-20261118" / "link_budget.md"))
    args = ap.parse_args(argv)

    rssi = {name: [] for name in BASELINES}
    with open(args.rounds, encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            if row["condition"] in rssi and row["result"] == "OK" and int(row["rssi_dbm"] or 0) != 0:
                rssi[row["condition"]].append(int(row["rssi_dbm"]))
    means = {name: statistics.mean(v) for name, v in rssi.items() if v}
    if len(means) < 2:
        print("need at least two baseline distances with RSSI", file=sys.stderr)
        return 1
    pts = sorted((BASELINES[name], means[name]) for name in means)
    segments = [(d1, r1, d2, r2, local_exponent(d1, r1, d2, r2)) for (d1, r1), (d2, r2) in zip(pts, pts[1:])]

    lines = ["# Link-budget estimate (model, not measured)", "",
             "The bench reached about 2 m between relay A and relay B, so every range below is an",
             "extrapolation. Path loss: PL(d) = PL0 + 10 n log10(d), with PL0 = FSPL(1 m) = "
             f"{PL0:.1f} dB at {FREQ_HZ / 1e9:.3f} GHz. The range is the distance where the received power",
             "equals the sensitivity plus the fade margin.", "",
             "## Measured RSSI of the direct baseline (BLE, mean of OK rounds)", "",
             "| Distance (m) | Mean RSSI (dBm) |", "|---|---|"]
    for d, r in pts:
        lines.append(f"| {d:.2f} | {r:.1f} |")
    lines += ["", "Local exponents between neighbouring points: " +
              "; ".join(f"{d1:.2f} to {d2:.2f} m: n = {n:.1f}" for d1, _, d2, _, n in segments) + ".",
              "They differ by a factor of about four, so one exponent does not describe this setup (antenna",
              "orientation and near-field effects). The bench cannot calibrate n, so the ranges use n = 2, 3 and 4.", "",
              "## Range estimates", "",
              "| Link | TX (dBm) | Sensitivity (dBm) | " + " | ".join(f"n = {label}" for label, _ in EXPONENTS) + " |",
              "|---|---|---|" + "---|" * len(EXPONENTS)]
    rows = [(name, p, SENS_ESPNOW_DBM) for name, p in ESPNOW_TX_DBM.items()] + \
           [("BLE legs (car to relay B, relay A to key)", BLE_TX_DBM, SENS_BLE_DBM)]
    for name, p, s in rows:
        cells = [fmt_range(range_m(p, s, n)) for _, n in EXPONENTS]
        lines.append(f"| {name} | {p:.1f} | {s:.0f} | " + " | ".join(cells) + " |")
    lines += ["", "## Assumptions", "",
              f"- Sensitivity: {SENS_ESPNOW_DBM:.0f} dBm (ESP-NOW, 802.11b 1 Mbps) and {SENS_BLE_DBM:.0f} dBm "
              "(BLE 1M PHY). Typical values from the ESP32 Series Datasheet v5.3 (Tables 5-6 and 5-7); the bench "
              "boards were not characterised.",
              f"- Fade margin {FADE_MARGIN_DB:.0f} dB; antenna gain {ANTENNA_GAIN_DBI:.0f} dBi at each end "
              "(the CYD PCB antennas were not characterised).",
              f"- BLE TX {BLE_TX_DBM:.0f} dBm, a typical setting; not measured on this bench.",
              "- TX 19.5 dBm is the typical ESP-NOW power (Table 5-6, 11b mode); TX 2 dBm is the lowest value the bench used (TXPOW 8).",
              "- The car-key distances 0.99 and 2.24 m are estimates from the layout."]
    out = pathlib.Path(args.out)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    print(f"\nwritten: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
