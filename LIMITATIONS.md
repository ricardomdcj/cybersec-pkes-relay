# Limitations and discussion notes

What the bench can and cannot show. Read this before you write any claim about the results.
This is a working note for the paper (`marco3-pkes`), not paper text. The numbers come from the
2026-11-16 pilot, in `data/analysis/summary.md`.

## What the pilot shows

One session per condition, 20 rounds each, older firmware (no `RUN_META`; round spacing of about
1.3-1.4 s, which fits the RTT + 1 s pacing that M-16 replaced).

| Condition | Rounds OK | Median RTT (95% CI) |
|---|---|---|
| Baseline (vehicle to key) | 20 of 20 | 91.0 ms (87.9-91.1) |
| Relayed (vehicle to Relay B to Relay A to key) | 20 of 20 | 334.7 ms (291.0-386.4) |
| Overhead (difference of medians) | | 243.7 ms (200.0-295.4) |

The vehicle accepts the relayed responses: the HMAC check passes. That is the thesis. The response is
bound to the nonce, not to time or place.

## Limits

1. **Timing, not distance.** Propagation over 30 cm is about 1 ns one way. The relay adds about 240 ms,
   so the bench detects the relay through its processing and hop latency, not through the separation of
   the boards. The `dist_m` column is the label the operator typed with `RUN dist=`. It is not a
   measurement.
2. **A timing limit is not a general defence.** In the pilot, the baseline maximum is 97.5 ms and the
   relayed minimum is 263 ms, so a limit between them would separate these rounds. That is in-sample and
   holds only for this stack. A faster relay with fewer hops could stay below such a limit. Any limit must
   be checked on sessions that were not used to set it. The 150 ms value that the M-19 campaigns suggest
   was read from those same campaigns, so it is in-sample too (`data/analysis-20261118/cutoff.md`,
   sections 5 and 6).
3. **Replication.** Each condition is one session. The bootstrap intervals in `tools/analyze_runs.py`
   treat rounds as independent, so they describe the spread within one session only. Use at least three
   sessions per condition and treat the session as the unit (D-08).
4. **Success rate.** 20 of 20 successes give a 95% Wilson lower bound of 83.9%. 50 of 50 give 92.9%.
   "Always succeeds" needs more than a count.
5. **Threat model.** The PSK is a bench constant in the source (`components/pkes_contract/ble_contract.h`).
   The key has no pairing, no encryption requirement and no rate limit. Its flash is not encrypted, and
   secure boot is off in the generated `key/sdkconfig`. An attacker can therefore compute valid responses.
   The relay does not need the PSK, so the thesis does not depend on it, but the paper has to say so.
6. **Timing structure (not verified).** Relay A's turnaround in the pilot falls into three clusters: 9
   rounds near 93 ms, 3 near 193 ms and 8 near 243 ms, about 50 ms apart. This may come from the BLE
   connection-event grid, since the vehicle requests 30-50 ms. The pilot does not log the negotiated
   interval, so the cause is not established. The v2 CSV logs `itvl_1_25ms` (M-19).
7. **Round grid.** The 1 s grid keeps the rounds of one campaign on one connection phase, because the
   50 ms interval divides 1000 ms. The M-19 firmware (`b79a463`) draws a start offset per campaign, so the
   campaigns sample different phases. The rounds inside one campaign are still correlated.
8. **Firmware.** The pilot used older firmware. The M-19 firmware (`b79a463`) has run: 16 campaigns at
   2.24 m (`data/runs/README.md`). The other checkpoints still need their own campaigns.
9. **The phase sets the overlap.** The relayed RTT depends on the connection phase of each campaign, as the
   honest RTT does. In the M-19 campaigns at 2.24 m the honest maximum is 136 ms. One relayed campaign of
   eight reaches 115 ms (6 of 50 rounds at or below 150 ms); the other seven start at 165 ms or more. So the
   1.5 % of relayed rounds accepted at 150 ms rests on one phase. More campaigns at more phases are needed
   before that rate is quoted.

## Mitigations to discuss (not built)

- **Distance bounding** (Hancke and Kuhn, 2005; verify the citation). It needs fast per-bit timing, which
  a BLE link cannot give. Discuss it; do not build it.
- **UWB ranging** (IEEE 802.15.4z; verify the citation). Car digital keys use UWB ranging (verify). The
  bench does not test it.
- **Timing limits.** See limit 2.
- **Power analysis of the key** is outside the scope (`SCOPE.md`).

## Methods drafts (adapt them; state how the separations were set and measured)

- Distance: "The separation between the boards was set by the operator and recorded as the distance label
  of each run. The bench did not measure it."
- Threat model: "The pre-shared key is a fixed bench constant, so an attacker in this study can compute
  valid responses. The relay does not need the key."
- Statistics: "Each condition was run in N sessions of 50 rounds. Intervals are bootstrap intervals over
  the rounds of each session, and the spread between sessions is reported separately."

## Not claimed

- That the relay can be detected at any distance, or by any timing limit beyond this bench.
- That the HMAC length matters for the relay. It does not.
- Anything about the current firmware before its campaigns have run.
- That a relay's false-accept rate is known from one campaign. The rate depends on the connection phase,
  so it needs more campaigns and more distances.
