---
name: pkes-relay-project
description: "pkes-relay PKES BLE relay bench (UNIFEI EC343E): repo state and checkpoints as of 2026-11-17, what is left to flash and check on hardware, the Methods draft"
metadata:
  node_type: memory
  type: project
  modified: 2026-11-17T16:53:00.912Z
  originSessionId: c8263742-ff5a-43af-bea4-c75f2b09cf4a
---

UNIFEI coursework bench: a PKES relay attack over BLE, measured for the paper `marco3-pkes`. Four CYD boards
(ESP32-2432S028, classic ESP32): key, vehicle, Relay A and Relay B. The knowledge lives in the repo, so update
these files there, not here: `CLAUDE.md`, `TESTING.md` (the procedure and the home checklist), `IMPROVEMENTS.md`
(tasks and checkpoints), `TASKS.md`, `SCOPE.md`, `components/cyd/LAYOUT.md`, `tools/sim/README.md`.

**Repo and branches (2026-11-17)**
- `origin` = github.com/ricardomdcj/cybersec-pkes-relay (private). The old name `pkes-relay` redirects.
- Checkpoints `ckpt/01` to `ckpt/11` are pushed. `cyd-port` = the newest checkpoint (`ckpt/11-memory-notes`) and
  holds every checkpoint commit since `main`. The user merged PR #2 (`cyd-port` at 761d7d7) into `main` on GitHub (ac1ba9f). `main` was not changed further.
- Firmware builds for `esp32` only. Checkpoint 09 holds V-01 (`LAYOUT.md`), the header shift, and the relay_b
  warning fix. Checkpoint 10 is docs only.

**Hardware: nothing is validated on a board yet.** The user does the flashing (the boards are at home; one CYD was at
work). I don't flash. Tonight's steps are in `TESTING.md` under "Before you start on a new machine": one build folder
per relay role, and the relay peer MAC is optional (zeros = broadcast tunnel). Still to check on hardware: the
timeout path calls `ble_gap_conn_rssi` and `ble_gattc_write_flat` from the esp_timer task; the full chain
key to A to B to vehicle has never run; the panel look of the header and the 12 px hex lines.

**Paper.** A Methods draft is in Claude Docs: https://claude.ai/artifact/YShbWxaTY32dnmwm3jt2uH (seven sections,
brackets for numbers still to fill: campaign counts, commit hash, results, how separations are measured). The user
implements it in the paper.

**Data.** `data/` has the 2026-11-16 pilot captures (CSV v1 with Portuguese column names, label `run`, 20 rounds,
older firmware), their UTF-8 copies in `data/utf8/`, and the analyzer output and SVG charts in `data/analysis/`.
New campaigns go to `data/runs/` and then to their own branch, `data/campaign-YYYYMMDD`.

**Toolchain.** ESP-IDF v6.1 only (`C:\esp\v6.1`). The v5.4 paths sit behind three version guards (listed in
`TASKS.md`) and were not rebuilt. Local `sdkconfig` files are gitignored; regenerate them with
`idf.py set-target esp32` in each folder.

**Memory copies.** The memory notes for this project are copied to `memory/` in the repo (checkpoint 11), so a session
on another machine can read them.
