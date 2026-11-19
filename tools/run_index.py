#!/usr/bin/env python3
"""Writes data/runs/INDEX.csv: one row per campaign, and one row per session that produced none.

It covers every run under data/runs/ and data/runs/_invalid/, so invalid and aborted runs stay in
the history with their reason. Valid campaigns come from manifest.json. The relay mode and the TX
power come from the session's chain log, and the condition comes from tools/analyze_conditions.py.

Usage (from the repo root): python -I tools/run_index.py
"""

import csv
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import analyze_conditions as ac  # noqa: E402

REPO = HERE.parent
RUNS = REPO / "data" / "runs"
INVALID = RUNS / "_invalid"
CHAIN = "chain_COM5_COM6_COM7.log"
COLUMNS = ["session", "campaign", "status", "reason", "started", "condition", "label", "car_key_m",
           "tunnel", "relay_mode", "tx_power", "rounds", "ok", "timeout", "csvb", "firmware_commit",
           "firmware_build", "folder"]
REASONS = {
    "20261118-195201_relay-session": "aborted: relay A did not connect within 60 s (first bring-up)",
    "20261118-195355_relay-session": "aborted: reset pulse failed, COM5 busy",
    "20261118-205713_relay-unicast-session": "aborted: stability check counted an old disconnect",
    "20261118-213041_baseline-session": "aborted: COM8 busy when the capture opened (bridge probing)",
    "20261119-000348_relay-ho-ab200-session": "aborted: passive logger could not open the three ports, before the campaign",
    "20261118-205109_relay-unicast-session": "invalid: vehicle connected directly to the key, no relay lines",
    "20261118-205332_relay-unicast-tx8-session": "invalid: vehicle connected directly to the key, no relay lines",
    "20261118-205917_relay-unicast-tx8-session": "invalid: vehicle connected directly to the key, no relay lines",
}


def condition_table():
    table = {}
    for spec in ac.CONDITIONS:
        for session, campaign in spec["campaigns"]:
            table[(session, campaign)] = spec
    return table


def chain_facts(session_dir):
    chain = session_dir / CHAIN
    if not chain.exists():
        return ("none" if (session_dir / "key_COM6.log").exists() else ""), "default", 0
    text = chain.read_text(encoding="utf-8", errors="replace")
    if "unicast to" in text:
        mode = "unicast"
    elif "using broadcast" in text:
        mode = "broadcast"
    else:
        mode = ""
    tx = "2 dBm" if "TXPOW set=8" in text else "default"
    csvb = sum(1 for line in text.splitlines() if "CSVB," in line)
    return mode, tx, csvb


def campaign_row(session_dir, campaign_dir, status, table):
    manifest = json.loads((campaign_dir / "manifest.json").read_text(encoding="utf-8"))
    mode, tx, csvb = chain_facts(session_dir)
    spec = table.get((session_dir.name, campaign_dir.name))
    return {
        "session": session_dir.name,
        "campaign": campaign_dir.name,
        "status": status,
        "reason": REASONS.get(session_dir.name, ""),
        "started": manifest.get("started_local", ""),
        "condition": spec["name"] if spec else "",
        "label": manifest.get("label", ""),
        "car_key_m": manifest.get("dist_m", ""),
        "tunnel": spec["tunnel"] if spec else ("direct" if mode == "none" else mode),
        "relay_mode": mode,
        "tx_power": tx,
        "rounds": manifest.get("rows_received", ""),
        "ok": manifest.get("rows_ok", ""),
        "timeout": manifest.get("rows_timeout", ""),
        "csvb": csvb,
        "firmware_commit": manifest.get("firmware_commit", ""),
        "firmware_build": manifest.get("firmware_build", ""),
        "folder": campaign_dir.relative_to(REPO).as_posix(),
    }


def aborted_row(session_dir):
    return {
        "session": session_dir.name, "campaign": "", "status": "aborted",
        "reason": REASONS.get(session_dir.name, "aborted: no campaign was captured"),
        "started": "", "condition": "", "label": "", "car_key_m": "", "tunnel": "",
        "relay_mode": "", "tx_power": "", "rounds": "", "ok": "", "timeout": "", "csvb": "",
        "firmware_commit": "", "firmware_build": "", "folder": session_dir.relative_to(REPO).as_posix(),
    }


def main(argv=None):
    table = condition_table()
    rows = []
    for session_dir in sorted(RUNS.glob("*-session")):
        rows += [campaign_row(session_dir, c, "valid", table)
                 for c in sorted(session_dir.glob("*/")) if (c / "manifest.json").exists()]
    for session_dir in sorted(INVALID.glob("*-session")):
        rows += [campaign_row(session_dir, c, "invalid", table)
                 for c in sorted(session_dir.glob("*/")) if (c / "manifest.json").exists()]
    for session_dir in sorted((INVALID / "aborted").glob("*-session")):
        if not any((c / "manifest.json").exists() for c in session_dir.glob("*/")):
            rows.append(aborted_row(session_dir))
    rows.sort(key=lambda r: (r["session"], r["campaign"]))
    out = RUNS / "INDEX.csv"
    with open(out, "w", encoding="utf-8", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)
    counts = {s: sum(1 for r in rows if r["status"] == s) for s in ("valid", "invalid", "aborted")}
    print(f"{len(rows)} rows written to {out}: {counts}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
