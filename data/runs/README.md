# Campaign runs

Each run is a session folder, `<YYYYmmdd-HHMMSS>_<label>-session/`, with:

- `<timestamp>_<label>/`: one 50-round vehicle campaign. `capture_COM8.log` is the vehicle UART
  (CSVHDR v2 rows) and `manifest.json` holds the label, distance, firmware commit and counts.
- `chain_COM5_COM6_COM7.log`: relay A (COM5), key (COM6) and relay B (COM7) UART, read passively
  during the campaign. Relay sessions only.
- `hold_COM8.log`: the helper that kept the vehicle in reset while the relays came up. Relay
  sessions only.
- `key_COM6.log`: key UART during the baseline campaign. Baseline sessions only.

`dist_m` is the car-key separation in every session. For relayed sessions, the relay A-B separation
is in the label.

## Conditions at 0.52 m car-key

The relays were placed between the car and the key, but their exact positions were not recorded.

| Session | Condition | Relay tunnel | TX power |
|---|---|---|---|
| 20261118-194943, -200758, -201019 | baseline (vehicle to key, direct) | none, relays silent | - |
| 20261118-195535, -200902, -201121 | relayed | broadcast (no MAC ACK) | default |
| 20261118-202314, -205220, -205758 | relayed | unicast (`RELAY_PEER_MAC`, MAC ACK) | default |
| 20261118-210207, -210459, -210625 | relayed | unicast | 2 dBm (`TXPOW 8` on both relays) |

## Physical separation at 0.99 m car-key

Layout on the table, in order: car, relay B, relay A, key. Car to relay B 12 cm, relay A to key 12 cm,
relay A to relay B 75 cm.

| Session | Condition | Relay tunnel | Label |
|---|---|---|---|
| 20261118-212448, -212709, -213129 | baseline (vehicle to key, direct) | none, relays silent | `baseline` |
| 20261118-212551, -212812, -213233 | relayed | unicast, TX power default | `relay-unicast-ab075` |

Notes:

- The campaign label `relay` is used both by the broadcast sessions and by session 202314 (unicast).
  Use the tables above, not the label, to tell them apart.
- Relayed sessions reset relay A and relay B at the start. The vehicle is held in reset until relay A
  is connected to the key and the key confirms it. Sessions 210207 onward and the 0.99 m relayed
  sessions use this bring-up. The earlier relayed sessions used a looser bring-up; every session here
  has relay lines (`CSVA`/`CSVB`) for all rounds that were answered.
- Sessions that were aborted or that did not go through the relays are not included here.

## Physical separation at about 2 m

Layout on the table, in order: car, relay B, relay A, key. Car to relay B 12 cm, relay B to relay A
about 2 m (estimated by eye), relay A to key 12 cm. `dist_m` is the car-key separation, estimated at
2.24 m from those distances. The USB cables (2 to 3 m) set the limit: the boards already sit on both
sides of the computer, so about 2 m between relay A and relay B is the largest separation this bench
reaches.

| Session | Condition | Relay tunnel | Label |
|---|---|---|---|
| 20261118-215829, -220051, -220426 | baseline (vehicle to key, direct) | none, relays silent | `baseline` |
| 20261118-215933, -220155, -220529 | relayed | unicast, TX power default | `relay-unicast-ab200` |
| 20261118-222135, -222302, -222527 | relayed | unicast, TXPOW 8 (2 dBm) on both relays | `relay-unicast-ab200-tx8` |

## M-19 campaigns at about 2 m (firmware b79a463)

Same layout as the 2 m section above. The vehicle runs the M-19 firmware (commit `b79a463`): each START
draws one start offset in [0, 1 s) and sends the first round after it, logged as `grid start offset <us>`.
The connection interval was 40 units (50 ms) in every campaign (`CONN` line and the `itvl_1_25ms` column),
so the offset sets the phase of each campaign. Eight baseline and eight relayed campaigns, 400 rounds per
condition, all answered OK, no timeouts.

| Session | Condition | Label | Start offset (ms) |
|---|---|---|---|
| 20261118-225639, -230050, -230423, -230754, -231139, -231408, -231627, -231958 | baseline (vehicle to key, direct) | `baseline-m19` | 126.0, 592.9, 140.7, 729.1, 116.4, 94.2, 223.7, 21.5 |
| 20261118-225814, -230151, -230523, -230856, -231239, -231507, -231727, -232058 | relayed, unicast, default TX | `relay-m19-ab200` | 402.3, 859.9, 498.5, 456.7, 697.0, 926.9, 607.9, 763.4 |

Notes:

- The baseline and relayed campaigns alternated in time.
- Relay bring-up took one attempt in three relayed campaigns and three attempts in five. The three-attempt
  cases reset relay A, as in the earlier sessions.
- Three campaigns logged drops before START (reason 574, the stale key slot): 4 drops in 20261118-225639,
  2 in 20261118-230754 and 1 in 20261118-231239. No campaign was interrupted.

## Held-out campaigns at 2.24 m (firmware b79a463)

These campaigns were recorded after the 150 ms bound had been read from the M-19 campaigns. The plan,
`data/analysis-20261118/heldout-150ms-plan.md` (commit 11803fb), was committed before the first one. Same
layout and firmware as the M-19 section above. Ten direct baseline campaigns and ten relayed campaigns
(unicast, default TX): every round was answered, with no timeouts.

| Session | Condition | Label |
|---|---|---|
| 20261119-000248, -000405, -000732, -001100, -001316, -001644, -002011, -002340, -002707, -002924 | baseline (vehicle to key, direct) | `baseline-ho` |
| 20261119-000505, -000833, -001201, -001417, -001744, -002112, -002440, -002808, -003025, -003207 | relayed, unicast, default TX | `relay-ho-ab200` |

Notes:

- The first relayed attempt (20261119-000348) aborted before its campaign: the passive logger could not open
  the three ports. It is under `_invalid/aborted/`. One replacement relayed campaign (20261119-003207) was run
  to reach the planned ten. The replacement was decided on the count, before any RTT was read.
- The relay bring-up took one attempt in four relayed campaigns and three attempts in six.
- Three campaigns logged drops before START (1, 4 and 1 drops). None logged a drop after START. Every CSV row
  has `itvl_1_25ms` = 40.

## Replication campaigns at 2.24 m (firmware b79a463)

Recorded after the 160 ms bound was fixed in `data/analysis-20261118/replication-160ms-plan.md` (commit
de7e0d3). Same layout and firmware as the M-19 and held-out sections. Ten direct baseline campaigns and ten
relayed campaigns (unicast, default TX): every round was answered, with no timeouts.

| Session | Condition | Label |
|---|---|---|
| 20261119-003839, -004102, -004431, -004647, -004902, -005118, -005445, -005700, -005915, -010131 | baseline (vehicle to key, direct) | `baseline-rep` |
| 20261119-003939, -004202, -004532, -004747, -005002, -005218, -005545, -005801, -010016, -010230 | relayed, unicast, default TX | `relay-rep-ab200` |

Notes:

- No campaign aborted, so no session was moved to `_invalid/`.
- The relay bring-up took one attempt in six relayed campaigns, two in one and three in three.
- One campaign (20261119-005118) logged a drop before START. None logged a drop after START. Every CSV row
  has `itvl_1_25ms` = 40.

## M-19 campaigns at 0.99 m (vehicle b79a463, relays 1cf7f0c)

The same M-19 vehicle firmware as the 2 m section (commit `b79a463`). The relays ran firmware `1cf7f0c`
(relay A's identifier carries a `+mod` suffix; relay B's does not). Layout: the car and Relay B 12 cm apart,
Relay A and the key 12 cm apart, the relays 75 cm apart (`unicast A-B 0.75`), so the car-to-key distance is
about 0.99 m. Ten direct baseline and ten relayed campaigns, 500 rounds per condition, all answered OK, no
timeouts.

| Session | Condition | Label | Start offset (ms) |
|---|---|---|---|
| 20261119-070054, -070422, -071049, -071416, -071747, -072010, -072227, -072442, -072810, -073140 | baseline (vehicle to key, direct) | `baseline-m19-099` | 300.2, 721.6, 407.1, 566.3, 566.2, 382.4, 754.4, 778.5, 658.8, 262.8 |
| 20261119-070154, -071149, -071519, -071848, -072111, -072328, -072543, -072912, -073240, -073536 | relayed, unicast, default TX | `relay-m19-ab075` | 482.4, 940.2, 810.5, 778.4, 928.2, 113.8, 649.7, 760.2, 471.4, 90.8 |

Notes:

- The baseline and relayed campaigns alternated in time.
- Session 20261119-070523 (relayed) recorded no rows: its capture was interrupted and no relayed `CSVB` line
  was logged. It is under `_invalid/aborted/`. The series loop did not catch it, because `run_relay.ps1` exited
  0 with a warning; the script now exits 1 in that case. Session 20261119-073536 was recorded at the end of the
  series as the replacement, so the relayed condition has the planned ten campaigns.
- The start offsets are the `grid start offset` lines of each `capture_COM8.log`.

## Latency sweep at 0.99 m (relay B holds each challenge; vehicle M-22)

Vehicle image `d795f50+mod` (M-22: the source of `vehicle/` is commit `d795f50`; `+mod` records uncommitted
analysis files in the working tree at build time). Relays `8d27b6a` (the `DELAY` firmware). Layout as the 0.99 m
M-19 section (car-key 0.99 m, relays 0.75 m apart). Each campaign is one relayed run with `COM7 DELAY <ms>`,
confirmed in the relay log as `DELAY set=<ms> ms`, labelled `relay-dNNNN`. The analysis is
`tools/analyze_delay.py`, written to `data/analysis-20261118/delay_sweep.md` and `delay_sweep.csv`.

| Session | D (ms) | OK / TIMEOUT | Late answers discarded by the vehicle | Dropped by relay B |
|---|---|---|---|---|
| 20261119-080112 | 0 | 50 / 0 | 0 | 0 |
| 20261119-080701 | 500 | 50 / 0 | 0 | 0 |
| 20261119-080828 | 1000 | 50 / 0 | 0 | 0 |
| 20261119-081107 | 1400 | 50 / 0 | 0 | 0 |
| 20261119-081352 | 1500 | 50 / 0 | 0 | 0 |
| 20261119-081655 | 1600 | 36 / 14 | 8 | 0 |
| 20261119-083552 | 1625 | 45 / 5 | 2 | 0 |
| 20261119-082006 | 1650 | 27 / 23 | 22 | 0 |
| 20261119-083805 | 1675 | 18 / 32 | 31 | 0 |
| 20261119-082330 | 1700 | 8 / 42 | 36 | 1 |
| 20261119-082544 | 1750 | 8 / 42 | 9 | 32 |
| 20261119-082758 | 1800 | 0 / 50 | 17 | 32 |
| 20261119-083013 | 1900 | 0 / 50 | 1 | 48 |
| 20261119-083325 | 2200 | 0 / 50 | 0 | 0 |

Notes:

- No row is FAIL in any sweep campaign. A late answer reaches the vehicle only when relay B still holds the
  earlier challenge's round; otherwise relay B drops it (`response for round N dropped`). The vehicle discards
  the ones that reach it (`late response to the timed-out challenge`, M-22).
- The expected threshold is 2000 ms minus the honest relayed median at D = 0 (358.6 ms): 1641 ms. The 50 % point
  is between 1650 and 1675 ms (interpolated 1656 ms).
- Replicates at 1625 and 1675 ms were added after the first pass, to place the 50 % point. They are campaigns
  like the others.
- Aborted attempts, all moved to `_invalid/aborted/`: 20261119-080022 and -080056 (before the D = 0 run);
  -080431, -080447, -080503, -080519, -080535 (D = 500); -080554 and -080610 (D = 1000); -082219, -082235 and
  -082251 (D = 1700). Each failed at the passive logger opening the ports. Two causes: the deskrab status-line
  bridge respawned and held COM5, COM6 and COM8, and loggers left by a failed attempt held COM6. `run_relay.ps1`
  now keeps the bridge stopped during bring-up and kills its own logger and holder on abort; `run_sweep.ps1`
  clears leftovers between attempts. After the fix, every campaign from D = 1700 to 2200 ms and the replicates
  succeeded on the first attempt.

## Run history

`INDEX.csv` has one row per campaign and one row per aborted session. The columns give the session,
the status (`valid`, `invalid` or `aborted`), the reason, the condition, the car-key distance, the relay
mode and TX power, the outcome counts, the firmware commit from the manifest, and the folder. Invalid
and aborted runs stay under `_invalid/` with their logs, so no bench data is dropped. Regenerate the
index with `python -I tools/run_index.py` after adding a run.

## Analysis

`tools/analyze_runs.py` and `tools/plot_runs.py` read the campaign folders (see `TESTING.md`,
section 5). `tools/analyze_conditions.py` pools the campaigns of each condition, and
`tools/analyze_cutoff.py` writes the cutoff analysis; its sections 5 and 6 use the M-19 campaigns only.
