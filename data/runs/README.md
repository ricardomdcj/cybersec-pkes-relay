# Campaign runs

Each run is a session folder, `<YYYYmmdd-HHMMSS>_<label>-session/`, with:

- `<timestamp>_<label>/`: one 50-round vehicle campaign. `capture_COM8.log` is the vehicle UART
  (CSVHDR v2 rows) and `manifest.json` holds the label, distance, firmware commit and counts.
- `chain_COM5_COM6_COM7.log`: relay A (COM5), key (COM6) and relay B (COM7) UART, read passively
  during the campaign. Relay sessions only.
- `hold_COM8.log`: the helper that kept the vehicle in reset while the relays came up. Relay
  sessions only.
- `key_COM6.log`: key UART during the baseline campaign. Baseline sessions only.

## Conditions in this folder (distance 0.52 m)

`dist_m` is the car-key separation in every session. The relays were placed between the car and
the key, but their exact positions were not recorded.

| Session | Condition | Relay tunnel | TX power |
|---|---|---|---|
| 20261118-194943, -200758, -201019 | baseline (vehicle to key, direct) | none, relays silent | - |
| 20261118-195535, -200902, -201121 | relayed | broadcast (no MAC ACK) | default |
| 20261118-202314, -205220, -205758 | relayed | unicast (`RELAY_PEER_MAC`, MAC ACK) | default |
| 20261118-210207, -210459, -210625 | relayed | unicast | 2 dBm (`TXPOW 8` on both relays) |

Notes:

- The campaign label `relay` is used both by the broadcast sessions and by session 202314 (unicast).
  Use the table above, not the label, to tell them apart.
- Relayed sessions reset relay A and relay B at the start. The vehicle is held in reset until relay A
  is connected to the key. From session 210207 on, the bring-up also waits for the key to confirm
  relay A's connection before the vehicle is released.
- Sessions that were aborted or that did not go through the relays are not included here.

## Analysis

`tools/analyze_runs.py` and `tools/plot_runs.py` read the campaign folders (see `TESTING.md`,
section 5).
