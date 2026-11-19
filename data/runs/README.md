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
