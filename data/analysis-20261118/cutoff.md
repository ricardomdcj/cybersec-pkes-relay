# Cutoff analysis (mitigation, Milestone 4)

Honest rounds: 450 from 9 direct campaigns. Relayed rounds: 900 from 18 relayed campaigns. A round is rejected by a bound T when it has no answer or its RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.

## 1. Phase of the round sends on the 50 ms connection grid

Resultant length R of the send phases within a campaign: 1 means every send falls on the same phase.

| Condition | Campaign | R | Mean phase (ms) |
|---|---|---|---|
| baseline 0.52 | 20261118-194946_baseline | 0.941 | 7.1 |
| baseline 0.52 | 20261118-200802_baseline | 0.958 | 0.5 |
| baseline 0.52 | 20261118-201022_baseline | 0.950 | 11.6 |
| broadcast 0.52 | 20261118-195547_relay | 0.262 | 1.9 |
| broadcast 0.52 | 20261118-200913_relay | 0.957 | 27.7 |
| broadcast 0.52 | 20261118-201132_relay | 0.507 | 8.6 |
| unicast 0.52 | 20261118-202325_relay | 0.958 | 18.4 |
| unicast 0.52 | 20261118-205231_relay-unicast | 0.958 | 42.3 |
| unicast 0.52 | 20261118-205816_relay-unicast | 0.957 | 11.8 |
| unicast 2 dBm 0.52 | 20261118-210338_relay-unicast-tx8 | 0.958 | 41.0 |
| unicast 2 dBm 0.52 | 20261118-210525_relay-unicast-tx8 | 0.957 | 48.0 |
| unicast 2 dBm 0.52 | 20261118-210651_relay-unicast-tx8 | 0.958 | 27.0 |
| baseline 0.99 | 20261118-212452_baseline | 0.959 | 37.8 |
| baseline 0.99 | 20261118-212713_baseline | 0.955 | 1.5 |
| baseline 0.99 | 20261118-213133_baseline | 0.957 | 9.4 |
| unicast A-B 0.75 (car-key 0.99) | 20261118-212608_relay-unicast-ab075 | 0.959 | 45.3 |
| unicast A-B 0.75 (car-key 0.99) | 20261118-212942_relay-unicast-ab075 | 0.958 | 42.4 |
| unicast A-B 0.75 (car-key 0.99) | 20261118-213251_relay-unicast-ab075 | 0.955 | 21.0 |
| baseline 2.24 | 20261118-215833_baseline | 0.958 | 19.7 |
| baseline 2.24 | 20261118-220055_baseline | 0.958 | 1.4 |
| baseline 2.24 | 20261118-220430_baseline | 0.956 | 20.6 |
| unicast A-B 2 (car-key 2.24) | 20261118-215951_relay-unicast-ab200 | 0.935 | 16.1 |
| unicast A-B 2 (car-key 2.24) | 20261118-220325_relay-unicast-ab200 | 0.956 | 43.4 |
| unicast A-B 2 (car-key 2.24) | 20261118-220700_relay-unicast-ab200 | 0.958 | 40.3 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 20261118-222201_relay-unicast-ab200-tx8 | 0.959 | 43.0 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 20261118-222426_relay-unicast-ab200-tx8 | 0.952 | 41.0 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 20261118-222552_relay-unicast-ab200-tx8 | 0.958 | 31.7 |

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

## 3. Operating curve of a bound (pooled, in-sample)

| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |
|---|---|---|
| 100 | 13.1 % | 0.0 % |
| 110 | 2.7 % | 0.0 % |
| 120 | 2.0 % | 0.3 % |
| 130 | 1.6 % | 0.3 % |
| 140 | 0.2 % | 0.3 % |
| 150 | 0.0 % | 0.3 % |
| 160 | 0.0 % | 0.6 % |
| 170 | 0.0 % | 3.3 % |
| 180 | 0.0 % | 4.0 % |
| 200 | 0.0 % | 4.7 % |
| 250 | 0.0 % | 9.4 % |
| 300 | 0.0 % | 22.1 % |

The largest honest RTT is 142.4 ms. A bound at that value rejects no honest round in sample, and accepts 0.3 % of relayed rounds.

## 4. Leave-one-campaign-out check

For each direct campaign h: T_h is the (1 - alpha) quantile of the honest rounds of the other direct campaigns, so h and the relayed rounds never set their own bound. Then h is tested (FRR) and all relayed rounds are tested (FAR).

| alpha | T_h min (ms) | T_h median (ms) | T_h max (ms) | Held-out FRR mean | Held-out FRR max | FAR (relayed, pooled) min | FAR median | FAR max |
|---|---|---|---|---|---|---|---|---|
| 0.01 | 130.4 | 136.4 | 136.4 | 1.1 % | 4.0 % | 0.3 % | 0.3 % | 0.3 % |
| 0.05 | 96.5 | 105.5 | 106.6 | 13.1 % | 94.0 % | 0.0 % | 0.0 % | 0.0 % |

## Notes

- The honest distribution comes from three campaigns per setup. Each campaign sends its rounds on one phase of the 50 ms grid (section 1), so the tails here are the tails of those phases, not of all phases. These numbers are provisional until the start-offset change (M-19) is in place and the campaigns are repeated.
- The bound is set from honest data only. Relayed data never set it.
- A bound is valid for this stack and these distances. It is not a general defence (LIMITATIONS.md, limit 2).
