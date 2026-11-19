# Cutoff analysis (mitigation, Milestone 4)

The unit is the campaign. Sections 3 and 4 pool every campaign; sections 5 and 6 use the M-19 campaigns only, whose round grid starts at a random offset.

## 1. Phase of the round sends on the 50 ms grid

R is the resultant length of the send phases within a campaign: 1 means every send falls on the same phase. The phase is on the vehicle's clock, because the connection anchor is not logged. The start offset is the `grid start offset` line of the M-19 firmware; the campaigns before M-19 have none.

| Condition | Campaign | Start offset (ms) | R | Mean phase (ms) |
|---|---|---|---|---|
| baseline 0.52 | 20261118-194946_baseline | - | 0.941 | 7.1 |
| baseline 0.52 | 20261118-200802_baseline | - | 0.958 | 0.5 |
| baseline 0.52 | 20261118-201022_baseline | - | 0.950 | 11.6 |
| broadcast 0.52 | 20261118-195547_relay | - | 0.262 | 1.9 |
| broadcast 0.52 | 20261118-200913_relay | - | 0.957 | 27.7 |
| broadcast 0.52 | 20261118-201132_relay | - | 0.507 | 8.6 |
| unicast 0.52 | 20261118-202325_relay | - | 0.958 | 18.4 |
| unicast 0.52 | 20261118-205231_relay-unicast | - | 0.958 | 42.3 |
| unicast 0.52 | 20261118-205816_relay-unicast | - | 0.957 | 11.8 |
| unicast 2 dBm 0.52 | 20261118-210338_relay-unicast-tx8 | - | 0.958 | 41.0 |
| unicast 2 dBm 0.52 | 20261118-210525_relay-unicast-tx8 | - | 0.957 | 48.0 |
| unicast 2 dBm 0.52 | 20261118-210651_relay-unicast-tx8 | - | 0.958 | 27.0 |
| baseline 0.99 | 20261118-212452_baseline | - | 0.959 | 37.8 |
| baseline 0.99 | 20261118-212713_baseline | - | 0.955 | 1.5 |
| baseline 0.99 | 20261118-213133_baseline | - | 0.957 | 9.4 |
| unicast A-B 0.75 (car-key 0.99) | 20261118-212608_relay-unicast-ab075 | - | 0.959 | 45.3 |
| unicast A-B 0.75 (car-key 0.99) | 20261118-212942_relay-unicast-ab075 | - | 0.958 | 42.4 |
| unicast A-B 0.75 (car-key 0.99) | 20261118-213251_relay-unicast-ab075 | - | 0.955 | 21.0 |
| baseline 2.24 | 20261118-215833_baseline | - | 0.958 | 19.7 |
| baseline 2.24 | 20261118-220055_baseline | - | 0.958 | 1.4 |
| baseline 2.24 | 20261118-220430_baseline | - | 0.956 | 20.6 |
| unicast A-B 2 (car-key 2.24) | 20261118-215951_relay-unicast-ab200 | - | 0.935 | 16.1 |
| unicast A-B 2 (car-key 2.24) | 20261118-220325_relay-unicast-ab200 | - | 0.956 | 43.4 |
| unicast A-B 2 (car-key 2.24) | 20261118-220700_relay-unicast-ab200 | - | 0.958 | 40.3 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 20261118-222201_relay-unicast-ab200-tx8 | - | 0.959 | 43.0 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 20261118-222426_relay-unicast-ab200-tx8 | - | 0.952 | 41.0 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 20261118-222552_relay-unicast-ab200-tx8 | - | 0.958 | 31.7 |
| baseline 2.24 M-19 | 20261118-225643_baseline-m19 | 126.0 | 0.943 | 41.1 |
| baseline 2.24 M-19 | 20261118-230054_baseline-m19 | 592.9 | 0.957 | 40.7 |
| baseline 2.24 M-19 | 20261118-230427_baseline-m19 | 140.7 | 0.958 | 40.5 |
| baseline 2.24 M-19 | 20261118-230758_baseline-m19 | 729.1 | 0.958 | 36.7 |
| baseline 2.24 M-19 | 20261118-231143_baseline-m19 | 116.4 | 0.959 | 27.6 |
| baseline 2.24 M-19 | 20261118-231411_baseline-m19 | 94.2 | 0.958 | 35.8 |
| baseline 2.24 M-19 | 20261118-231631_baseline-m19 | 223.7 | 0.959 | 25.3 |
| baseline 2.24 M-19 | 20261118-232002_baseline-m19 | 21.5 | 0.958 | 14.1 |
| unicast A-B 2 M-19 (car-key 2.24) | 20261118-225945_relay-m19-ab200 | 402.3 | 0.941 | 42.9 |
| unicast A-B 2 M-19 (car-key 2.24) | 20261118-230321_relay-m19-ab200 | 859.9 | 0.958 | 1.6 |
| unicast A-B 2 M-19 (car-key 2.24) | 20261118-230653_relay-m19-ab200 | 498.5 | 0.958 | 39.7 |
| unicast A-B 2 M-19 (car-key 2.24) | 20261118-231026_relay-m19-ab200 | 456.7 | 0.956 | 31.5 |
| unicast A-B 2 M-19 (car-key 2.24) | 20261118-231257_relay-m19-ab200 | 697.0 | 0.951 | 42.6 |
| unicast A-B 2 M-19 (car-key 2.24) | 20261118-231526_relay-m19-ab200 | 926.9 | 0.958 | 23.1 |
| unicast A-B 2 M-19 (car-key 2.24) | 20261118-231858_relay-m19-ab200 | 607.9 | 0.950 | 3.8 |
| unicast A-B 2 M-19 (car-key 2.24) | 20261118-232116_relay-m19-ab200 | 763.4 | 0.958 | 8.6 |

## 2. RTT per condition (answered rounds, ms)

| Condition | n | min | median | p99 | max |
|---|---|---|---|---|---|
| baseline 0.52 | 150 | 57.7 | 76.9 | 107.8 | 128.2 |
| broadcast 0.52 | 139 | 167.7 | 366.1 | 448.6 | 458.8 |
| unicast 0.52 | 150 | 169.7 | 384.6 | 443.6 | 443.9 |
| unicast 2 dBm 0.52 | 150 | 164.2 | 367.4 | 435.3 | 468.4 |
| baseline 0.99 | 150 | 62.7 | 82.0 | 130.6 | 142.4 |
| unicast A-B 0.75 (car-key 0.99) | 150 | 112.4 | 376.6 | 463.8 | 528.6 |
| baseline 2.24 | 150 | 63.8 | 85.7 | 136.9 | 139.9 |
| unicast A-B 2 (car-key 2.24) | 150 | 158.2 | 364.5 | 456.5 | 456.7 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 150 | 113.4 | 316.2 | 430.7 | 468.4 |
| baseline 2.24 M-19 | 400 | 57.6 | 74.7 | 116.7 | 136.1 |
| unicast A-B 2 M-19 (car-key 2.24) | 400 | 115.3 | 397.5 | 456.8 | 550.5 |

## 3. Operating curve of a bound (pooled, in-sample)

Honest rounds: 850 from 17 direct campaigns. Relayed rounds: 1300 from 26 relayed campaigns. A round is rejected by a bound T when it has no answer or its RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.

| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |
|---|---|---|
| 100 | 10.0 % | 0.0 % |
| 110 | 2.2 % | 0.0 % |
| 120 | 1.4 % | 0.7 % |
| 130 | 1.1 % | 0.7 % |
| 140 | 0.1 % | 0.7 % |
| 150 | 0.0 % | 0.7 % |
| 160 | 0.0 % | 0.8 % |
| 170 | 0.0 % | 3.2 % |
| 180 | 0.0 % | 3.8 % |
| 200 | 0.0 % | 4.2 % |
| 250 | 0.0 % | 9.8 % |
| 300 | 0.0 % | 23.2 % |

The largest honest RTT is 142.4 ms. A bound at that value rejects no honest round in sample, and accepts 0.7 % of relayed rounds.

## 4. Leave-one-campaign-out check (pooled)

For each direct campaign h: T_h is the (1 - alpha) quantile of the honest rounds of the other direct campaigns, so h and the relayed rounds never set their own bound. Then h is tested (FRR) and all relayed rounds are tested (FAR).

| alpha | T_h min (ms) | T_h median (ms) | T_h max (ms) | Held-out FRR mean | Held-out FRR max | FAR (relayed, pooled) min | FAR median | FAR max |
|---|---|---|---|---|---|---|---|---|
| 0.01 | 128.0 | 130.4 | 130.4 | 1.2 % | 6.0 % | 0.7 % | 0.7 % | 0.7 % |
| 0.05 | 97.3 | 105.2 | 105.4 | 9.9 % | 94.0 % | 0.0 % | 0.0 % | 0.0 % |

## 5. Operating curve, M-19 campaigns only (in-sample)

Honest rounds: 400 from 8 direct campaigns. Relayed rounds: 400 from 8 relayed campaigns. A round is rejected by a bound T when it has no answer or its RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.

| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |
|---|---|---|
| 100 | 6.5 % | 0.0 % |
| 110 | 1.8 % | 0.0 % |
| 120 | 0.8 % | 1.5 % |
| 130 | 0.5 % | 1.5 % |
| 140 | 0.0 % | 1.5 % |
| 150 | 0.0 % | 1.5 % |
| 160 | 0.0 % | 1.5 % |
| 170 | 0.0 % | 2.8 % |
| 180 | 0.0 % | 3.2 % |
| 200 | 0.0 % | 3.2 % |
| 250 | 0.0 % | 10.5 % |
| 300 | 0.0 % | 25.8 % |

The largest honest RTT is 136.1 ms. A bound at that value rejects no honest round in sample, and accepts 1.5 % of relayed rounds.

## 6. Leave-one-campaign-out check, M-19 campaigns only

For each direct campaign h: T_h is the (1 - alpha) quantile of the honest rounds of the other direct campaigns, so h and the relayed rounds never set their own bound. Then h is tested (FRR) and all relayed rounds are tested (FAR).

| alpha | T_h min (ms) | T_h median (ms) | T_h max (ms) | Held-out FRR mean | Held-out FRR max | FAR (relayed, pooled) min | FAR median | FAR max |
|---|---|---|---|---|---|---|---|---|
| 0.01 | 107.9 | 117.9 | 117.9 | 1.8 % | 8.0 % | 0.0 % | 1.5 % | 1.5 % |
| 0.05 | 90.1 | 104.9 | 105.0 | 6.5 % | 38.0 % | 0.0 % | 0.0 % | 0.0 % |

## Notes

- Sections 3 and 4 mix the 2026-11-18 campaigns with the M-19 campaigns. The firmware before M-19 sent round 1 at START, so the phase of those campaigns followed from the connection timing. Sections 5 and 6 use the M-19 campaigns only, whose offsets are drawn at random.
- Within one campaign the sends stay on one phase, so every campaign's RTT is shifted by its own phase (section 1 shows R per campaign).
- The bound is set from honest data only. Relayed data never set it.
- The M-19 sections are at one distance (car-key 2.24 m, A-B 2 m). The 0.52 m and 0.99 m campaigns are in sections 1 to 4 only.
- A bound is valid for this stack and these distances. It is not a general defence (LIMITATIONS.md, limit 2).
