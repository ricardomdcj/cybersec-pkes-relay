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
2.24 m from those distances.

| Session | Condition | Relay tunnel | Label |
|---|---|---|---|
| 20261118-215829, -220051, -220426 | baseline (vehicle to key, direct) | none, relays silent | `baseline` |
| 20261118-215933, -220155, -220529 | relayed | unicast, TX power default | `relay-unicast-ab200` |

## Analysis

`tools/analyze_runs.py` and `tools/plot_runs.py` read the campaign folders (see `TESTING.md`,
section 5).
