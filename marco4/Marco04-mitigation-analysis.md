# Marco 04: mitigation analysis (draft)

Nov 19, 2026 · Ricardo

This file is a one-time export of the Claude Docs document "Marco 04 mitigation analysis (draft)" (revision 40). The three charts in the document are drawn widgets; here they appear as links. The live document is https://claude.ai/artifact/51yMkRjv81mfPeAF2MQyoQ.

## 1. What the bench measures

The relay adds about 275 to 323 ms to the median round, depending on the setup, against an honest median of 75 to 90 ms. In 2650 unicast rounds across nine setups, every round was answered, and no round failed the HMAC check.

The bench detects latency, not distance. Light covers 30 cm in one nanosecond, so the timing difference comes from the relay's processing and its hops. The distance labels were typed by the operator; they are not measurements. The longest relay separation was about 2 m, so the failure distance was not reached (LIMITATIONS.md, limit 1).

Source: data/analysis-20261118/summary.md and delay_sweep.md; commit 53cb063 on main (PR #5).

## 2. Where a latency bound separates the rounds

Chart (replication bound): typed in chat from data/analysis-20261118/cutoff.md, section 9b; 500 direct and 500 relayed replication rounds.

The 160 ms bound was fixed before a replication set of ten direct and ten relayed campaigns, recorded at 2.24 m. It rejected no honest round in 500 (highest 143.3 ms) and accepted 13 of 500 relayed rounds (2.6 %, 95 % interval 1.5 to 4.4 %). Across the 28 direct campaigns at 2.24 m, 160 ms rejects none of 1400 honest rounds and accepts 26 of 1400 relayed rounds (1.9 %). The 150 ms bound, read from the in-sample set, failed its held-out test: one honest round took 150.6 ms. That is the highest honest RTT of the 28 campaigns, so the margin at 160 ms is about 9 ms. In the held-out and replication sets, two relayed campaigns in each contributed every round accepted at 160 ms; the other eight in each set have no round below 164 ms.

## 3. Robustness across campaigns

A bound set from the other campaigns is less stable than the in-sample curve. Among the eight in-sample M-19 campaigns, a 1 % bound set from the other seven lands at 108 to 118 ms and rejects 1.8 % of the left-out honest rounds on average, and 8.0 % at most. A 5 % bound lands at 90 to 105 ms and rejects 6.5 % on average, and up to 38 % of one left-out campaign. The held-out test in section 2 agrees: 150 ms, read from these eight campaigns, rejected one honest round in 500 on ten new campaigns. The replication set, fixed at 160 ms, rejected none in 500.

| Level | Campaigns | Bound from the other campaigns (ms) | Left-out honest rounds rejected | Relayed rounds accepted |
| --- | --- | --- | --- | --- |
| 1 % | In-sample M-19 (8 direct, 8 relayed) | 107.9 to 117.9 | 1.8 % mean, 8.0 % max | 0.0 to 1.5 % |
| 5 % | In-sample M-19 (8 direct, 8 relayed) | 90.1 to 105.0 | 6.5 % mean, 38.0 % max | 0.0 % |
| 1 % | All (37 direct, 46 relayed) | 116.6 to 119.7 | 1.0 % mean, 6.0 % max | 0.8 % |
| 5 % | All (37 direct, 46 relayed) | 105.2 to 105.9 | 5.5 % mean, 26.0 % max | 0.0 % |

The phase is the likely cause of the spread. Within each campaign the send times stay on one phase of the 50 ms connection grid: R is 0.93 or more in 81 of 83 campaigns, and the two exceptions are broadcast campaigns. The M-19 firmware draws a new start offset per campaign, so the campaigns sample different phases. Across the 56 M-19-firmware campaigns at 2.24 m, the honest medians span 61 to 103 ms and the relayed medians span 263 to 449 ms. The relayed spread is wider, as expected if each of the two BLE legs of the relay has its own connection phase (to verify).

Source: data/analysis-20261118/cutoff.md, sections 1, 4, 6, 7 and 9 (commit b0ef668); per-campaign medians from data/analysis-20261118/summary.md.

## 4. Distance bounding

Distance bounding times single bits and bounds the distance from the round trip. Light covers 30 cm in one nanosecond, so a 15 cm bound needs about 1 ns of timing precision. BLE link-layer timing is at the microsecond level at best, so it cannot resolve centimetres (to verify). Hancke and Kuhn (SecureComm 2005, pp. 67 to 73, DOI 10.1109/SECURECOMM.2005.56) proposed the protocol for RFID. The 2005 date matches the conference dates; one BibTeX export says 2006.

The relay adds about 280 ms per round, roughly 3 x 10^8 times the propagation time over 30 cm. A latency bound catches this relay for that reason, and section 2 measures it. A faster relay would pass the same bound, so the bound describes this stack only (LIMITATIONS.md, limit 2). Distance bounding measures proximity directly, but it needs the per-bit timing that this BLE stack does not give.

## 5. Ranging with UWB

UWB ranging measures the time of flight of very short pulses, which resolves distance to centimetres. IEEE 802.15.4z defines secure ranging for it (to verify before publishing). CCC Digital Key Release 3.0 adds UWB ranging after the BLE check (2021 press coverage; the specification itself needs CCC membership).

A UWB check would catch a relay that holds the key metres away, whatever latency the relay adds. The bench has no UWB hardware and does not test it, so its results say nothing about UWB. They cover latency and the BLE and ESP-NOW links only.

## 6. Estimated reach of the relay tunnel

Chart (reach ranges): data/analysis-20261118/link_budget.md; typed in chat; sensitivities and gains are datasheet-typical and assumed.

The bench reached 2 m, far below these estimates. The estimates depend on the path-loss exponent and on the antennas, which were not characterised. At exponent 4 (dense indoor), the 2 dBm tunnel reaches 18 m and the BLE legs 9 m. At exponent 3 (obstructed indoor), they reach 46 m and 20 m. The BLE legs set how close each relay must sit to its car or key; the tunnel sets how far apart the two relays can be.

## 7. Threats to validity

- The pre-shared key is a bench constant in the source, so an attacker here can compute valid responses. The relay does not need the key, so the thesis does not depend on it (LIMITATIONS.md, limit 5).
- The unit is the campaign: three per setup for the 2026-11-18 setups, eight for the in-sample 2.24 m M-19 setup, and ten for the 0.99 m M-19, held-out and replication setups; the sweep has one campaign per delay and two replicates. Stated explicitly: each condition has 10 campaigns (500 rounds), below the course's floor of 30 repetitions per cell at campaign level. Section 9 gives the campaigns needed for a 25 ms difference between relayed configurations; the final paper will extend the sets. The bootstrap intervals treat rounds as independent, so they are optimistic (limit 3).
- Each campaign samples one phase of the 50 ms connection grid. M-19 draws that phase at random, but the 56 campaigns at 2.24 m are only 56 samples of it, and in the held-out and replication sets, two relayed campaigns in each supply the rounds below 160 ms (section 2; LIMITATIONS.md, limit 9).
- The 150 ms bound failed its held-out criterion: one honest round in 500 took 150.6 ms. The 160 ms bound was then fixed in advance and tested on the replication set (section 9): no honest round above it, and 2.6 % of relayed rounds accepted. The bound still describes this stack and these distances only (LIMITATIONS.md, limit 2).
- The distance labels were typed by the operator, not measured. The largest relay separation was about 2 m.
- Relays A and B are reset at the start of each relayed campaign, and relay A again when the key does not confirm its connection. The vehicle is released only after the key confirms.
- The vehicle's run header records its firmware: 1cf7f0c before M-19, b79a463 for the M-19, held-out, replication and 0.99 m campaigns, and d795f50+mod for the sweep (M-22). The relay builds are in the chain logs (RUN_META), not in the manifests.

## 8. Next measurements

1. Done: the start offset (M-19, commit b79a463) and eight in-sample campaigns per condition at 2.24 m.
2. Done: the held-out test, ten campaigns per condition. 150 ms failed; 160 ms was evaluated afterwards as a separate row.
3. Done: the replication, ten campaigns per condition, with 160 ms fixed in advance. No honest round above it; 2.6 % of relayed rounds accepted.
4. Done: the M-19 firmware at 0.99 m (ten direct and ten relayed campaigns; overhead 289 ms, section 9) and the latency sweep at 0.99 m (section 10). Done: the M-19 firmware at 0.52 m (ten direct, ten relayed; overhead 282 ms, interval 248 to 310 ms). Across 0.52, 0.99 and 2.24 m the overhead does not move beyond the campaign spread.
5. If an RF attenuator is available, attenuate the tunnel to look for its failure point, since the cables stop the separation near 2 m.

Step 4 decides whether the bound moves with distance. It needs the boards moved, so it waits for the bench. Step 5 needs an attenuator. The reach in section 6 stays a model until step 5 or a longer link.

## 9. Effect sizes, uncertainty and sample size

The unit is the campaign: rounds inside one campaign share one phase of the connection grid. At 2.24 m, the 28 direct and 28 relayed campaigns with the M-19 firmware give a median RTT of 79.1 ms without relay and 358.5 ms with it: an overhead of 279.5 ms (95 % bootstrap interval over campaigns, 262.5 to 296.6 ms). The campaign medians separate completely (Cliff's delta 1.00). The three sets of 8 or 10 campaigns per side measure the overhead at 323 ms (in sample), 276 ms (held-out) and 275 ms (replication). Each set has an exact Mann-Whitney p of 0.0002 or less, and the Holm-adjusted p is 0.0006 or less.

At 0.99 m with the M-19 firmware (ten direct and ten relayed campaigns), the overhead is 289.2 ms (95 % interval 246.9 to 301.7 ms). The campaign medians separate completely (Cliff's delta 1.00). The exact Mann-Whitney p is 0.0000, and the Holm-adjusted p is 0.0001 across seven groups.

The three pre-M-19 sets have three campaigns per side. Their smallest possible two-sided exact p is 0.10, so they are reported as descriptive, not as tests.

RSSI separates the two configurations completely (campaign AUC 1.00), but the layout is the likely cause: the vehicle talks to Relay B at 12 cm (median about -46 dBm) and to the key at 2.24 m (about -56 dBm). RSSI therefore says nothing about relays on this bench (H3).

Every unicast round at 2.24 m was answered (1400 of 1400). The broadcast relay, which has no MAC acknowledgement and was run only at 0.52 m, lost 11 of 150 rounds (7.3 %). This points to the transport rather than to distance (H2).

For a difference of 25 ms (half the BLE connection interval) between two relayed configurations, the campaign-to-campaign spread of relayed medians (SD 54 ms) needs about 73 campaigns per configuration at 80 % power. The direct campaigns (SD 14 ms) need about 5. The 8 to 10 campaigns per condition used here cannot resolve differences of that size between relayed configurations.

Source: data/analysis-20261118/paper_stats.md (tools/paper_stats.py, commit 37305ff).

## 10. Latency injection: where the relay stops

Chart (latency sweep at 0.99 m): typed in chat from data/analysis-20261118/delay_sweep.csv; 14 campaigns of 50 rounds at 0.99 m.

Relay B holds each vehicle challenge for D ms before the tunnel, at 0.99 m. The verifier accepts a round only if its answer arrives within 2 s. Success is 100 % up to 1500 ms and 0 % from 1800 ms. The 50 % point lies between 1650 and 1675 ms (interpolated 1656 ms). The honest relayed median is 358.6 ms at D = 0, so the prediction is 1641 ms.

Late answers do not decide the outcome. Relay B drops an answer that arrives after it has received the next challenge (32 drops at 1750 ms). The vehicle discards the late answers that reach it under M-22 (36 at 1700 ms). No sweep campaign has a FAIL row.

Source: data/analysis-20261118/delay_sweep.md and delay_sweep.csv; the 14 campaigns relay-d0000 to relay-d2200 under data/runs/.

## Sources

- Campaign data and index: data/runs/INDEX.csv (117 valid campaigns; invalid and aborted runs under data/runs/_invalid/), merged to main as PR #5 (commit 596e4e1).
- Analysis: data/analysis-20261118/summary.md, cutoff.md, link_budget.md and delay_sweep.md, with tools/analyze_conditions.py, tools/analyze_cutoff.py, tools/analyze_delay.py and tools/link_budget.py (commit 53cb063).
- Test plans, each committed before its campaigns: data/analysis-20261118/heldout-150ms-plan.md (commit 11803fb) and replication-160ms-plan.md (commit de7e0d3).
- Firmware: vehicle/main/main.c, the M-19 start offset (commit b79a463) and the M-22 late-answer rule (commit d795f50).
- Scope and limits: LIMITATIONS.md and TASKS.md (milestones 3 and 4).
- Hancke and Kuhn (2005), distance bounding: checked against the proceedings metadata. IEEE 802.15.4z, UWB ranging: still to verify. CCC Digital Key Release 3.0: UWB confirmed from press coverage.
- Francillon, Danev and Čapkun, NDSS 2011: https://www.s3.eurecom.fr/docs/ndss11_francillon.pdf (Table 3 and sections 4.2 and 4.3 checked: 120 ns, 35 µs to tens of ms, 11 to 436 µs).
- Staat et al., arXiv:2202.06554: https://arxiv.org/abs/2202.06554 (introduction: attack effective with the phone more than 65 m away).
- Khan, NCC Group technical advisory, May 2022: https://nccgroup.com/us/research-blog/technical-advisory-tesla-ble-phone-as-a-key-passive-entry-vulnerable-to-relay-attacks (8 ms added round trip, up to 80 ms, phone about 25 m away).
