# Cutoff analysis (mitigation, Milestone 4)

The unit is the campaign. Sections 3, 4 and 8 pool every campaign, the held-out ones included; sections 5 and 6 use the M-19 in-sample campaigns only, whose round grid starts at a random offset; section 7 is the held-out test of the bound.

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
| baseline 2.24 held-out | 20261119-000252_baseline-ho | 605.0 | 0.943 | 2.5 |
| baseline 2.24 held-out | 20261119-000408_baseline-ho | 415.9 | 0.957 | 21.4 |
| baseline 2.24 held-out | 20261119-000736_baseline-ho | 814.8 | 0.957 | 46.2 |
| baseline 2.24 held-out | 20261119-001104_baseline-ho | 680.9 | 0.959 | 24.4 |
| baseline 2.24 held-out | 20261119-001320_baseline-ho | 777.9 | 0.959 | 22.4 |
| baseline 2.24 held-out | 20261119-001648_baseline-ho | 273.1 | 0.955 | 38.5 |
| baseline 2.24 held-out | 20261119-002015_baseline-ho | 560.4 | 0.958 | 7.8 |
| baseline 2.24 held-out | 20261119-002344_baseline-ho | 211.8 | 0.958 | 6.3 |
| baseline 2.24 held-out | 20261119-002711_baseline-ho | 324.9 | 0.955 | 45.5 |
| baseline 2.24 held-out | 20261119-002928_baseline-ho | 638.5 | 0.957 | 32.0 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-000635_relay-ho-ab200 | 730.9 | 0.958 | 32.9 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-001003_relay-ho-ab200 | 433.8 | 0.947 | 47.3 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-001219_relay-ho-ab200 | 875.8 | 0.958 | 5.9 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-001547_relay-ho-ab200 | 360.3 | 0.954 | 2.8 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-001914_relay-ho-ab200 | 670.9 | 0.954 | 16.0 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-002243_relay-ho-ab200 | 849.6 | 0.956 | 47.6 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-002610_relay-ho-ab200 | 258.3 | 0.950 | 4.8 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-002826_relay-ho-ab200 | 396.3 | 0.955 | 35.1 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-003043_relay-ho-ab200 | 99.3 | 0.957 | 48.3 |
| unicast A-B 2 held-out (car-key 2.24) | 20261119-003225_relay-ho-ab200 | 648.8 | 0.958 | 46.8 |
| baseline 2.24 replication | 20261119-003843_baseline-rep | 222.4 | 0.959 | 25.0 |
| baseline 2.24 replication | 20261119-004105_baseline-rep | 694.4 | 0.956 | 7.3 |
| baseline 2.24 replication | 20261119-004435_baseline-rep | 628.8 | 0.946 | 3.7 |
| baseline 2.24 replication | 20261119-004651_baseline-rep | 548.8 | 0.944 | 9.8 |
| baseline 2.24 replication | 20261119-004906_baseline-rep | 502.8 | 0.958 | 27.8 |
| baseline 2.24 replication | 20261119-005121_baseline-rep | 548.6 | 0.958 | 47.2 |
| baseline 2.24 replication | 20261119-005449_baseline-rep | 44.3 | 0.956 | 38.0 |
| baseline 2.24 replication | 20261119-005704_baseline-rep | 410.9 | 0.958 | 6.6 |
| baseline 2.24 replication | 20261119-005919_baseline-rep | 798.6 | 0.957 | 43.4 |
| baseline 2.24 replication | 20261119-010134_baseline-rep | 2.8 | 0.939 | 46.1 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-004004_relay-rep-ab200 | 489.4 | 0.956 | 34.7 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-004334_relay-rep-ab200 | 363.3 | 0.959 | 21.4 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-004549_relay-rep-ab200 | 428.7 | 0.959 | 40.0 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-004805_relay-rep-ab200 | 335.0 | 0.957 | 37.2 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-005020_relay-rep-ab200 | 455.1 | 0.941 | 45.0 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-005348_relay-rep-ab200 | 592.9 | 0.958 | 35.1 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-005603_relay-rep-ab200 | 806.6 | 0.958 | 24.4 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-005819_relay-rep-ab200 | 55.2 | 0.947 | 2.1 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-010034_relay-rep-ab200 | 274.6 | 0.942 | 44.1 |
| unicast A-B 2 replication (car-key 2.24) | 20261119-010401_relay-rep-ab200 | 128.8 | 0.938 | 48.0 |

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
| baseline 2.24 held-out | 500 | 57.6 | 81.2 | 107.3 | 150.6 |
| unicast A-B 2 held-out (car-key 2.24) | 500 | 121.0 | 356.7 | 456.6 | 512.0 |
| baseline 2.24 replication | 500 | 57.5 | 80.2 | 107.7 | 143.3 |
| unicast A-B 2 replication (car-key 2.24) | 500 | 107.4 | 354.7 | 454.0 | 454.5 |

## 3. Operating curve of a bound (pooled over all campaigns, held-out ones included)

Honest rounds: 1850 from 37 direct campaigns. Relayed rounds: 2300 from 46 relayed campaigns. A round is rejected by a bound T when it has no answer or its RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.

| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |
|---|---|---|
| 100 | 12.8 % | 0.0 % |
| 110 | 1.4 % | 0.4 % |
| 120 | 0.9 % | 0.8 % |
| 130 | 0.6 % | 1.0 % |
| 140 | 0.2 % | 1.0 % |
| 150 | 0.1 % | 1.0 % |
| 160 | 0.0 % | 1.3 % |
| 170 | 0.0 % | 2.9 % |
| 180 | 0.0 % | 3.7 % |
| 200 | 0.0 % | 4.6 % |
| 250 | 0.0 % | 9.9 % |
| 300 | 0.0 % | 26.1 % |

The largest honest RTT is 150.6 ms. A bound at that value rejects no honest round in sample, and accepts 1.0 % of relayed rounds.

## 4. Leave-one-campaign-out check (pooled)

For each direct campaign h: T_h is the (1 - alpha) quantile of the honest rounds of the other direct campaigns, so h and the relayed rounds never set their own bound. Then h is tested (FRR) and all relayed rounds are tested (FAR).

| alpha | T_h min (ms) | T_h median (ms) | T_h max (ms) | Held-out FRR mean | Held-out FRR max | FAR (relayed, pooled) min | FAR median | FAR max |
|---|---|---|---|---|---|---|---|---|
| 0.01 | 116.6 | 119.7 | 119.7 | 1.0 % | 6.0 % | 0.8 % | 0.8 % | 0.8 % |
| 0.05 | 105.2 | 105.8 | 105.9 | 5.5 % | 26.0 % | 0.0 % | 0.0 % | 0.0 % |

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

## 7. Held-out check of the 150 ms bound

T = 150 ms was fixed in `heldout-150ms-plan.md` before these campaigns were recorded. Held-out: 10 direct and 10 relayed campaigns, 500 honest and 500 relayed rounds.

Criterion 1, honest false rejects: 1 of 500 honest rounds above T or unanswered (0.2 %), expected 0. Not met: the bound fails as a false-reject limit out of sample.

Criterion 2, relayed accepts: 5 of 500 relayed rounds at or below T (1.0 %), 95 % Wilson interval 0.4 to 2.3 %. Reported as found; the plan sets no pass threshold for it.

The largest held-out honest RTT is 150.6 ms.

| Direct campaign | Rounds above T or unanswered | Max answered RTT (ms) |
|---|---|---|
| 20261119-000252_baseline-ho | 1 | 150.6 |
| 20261119-000408_baseline-ho | 0 | 95.3 |
| 20261119-000736_baseline-ho | 0 | 73.0 |
| 20261119-001104_baseline-ho | 0 | 81.9 |
| 20261119-001320_baseline-ho | 0 | 86.3 |
| 20261119-001648_baseline-ho | 0 | 71.4 |
| 20261119-002015_baseline-ho | 0 | 144.4 |
| 20261119-002344_baseline-ho | 0 | 100.0 |
| 20261119-002711_baseline-ho | 0 | 107.4 |
| 20261119-002928_baseline-ho | 0 | 122.7 |

| Relayed campaign | Answered rounds | Min (ms) | Median (ms) | Max (ms) | Rounds at or below T |
|---|---|---|---|---|---|
| 20261119-000635_relay-ho-ab200 | 50 | 174.8 | 427.1 | 430.5 | 0 |
| 20261119-001003_relay-ho-ab200 | 50 | 203.7 | 300.5 | 448.0 | 0 |
| 20261119-001219_relay-ho-ab200 | 50 | 297.3 | 394.8 | 399.9 | 0 |
| 20261119-001547_relay-ho-ab200 | 50 | 300.9 | 447.3 | 495.0 | 0 |
| 20261119-001914_relay-ho-ab200 | 50 | 186.1 | 311.5 | 389.8 | 0 |
| 20261119-002243_relay-ho-ab200 | 50 | 256.8 | 358.5 | 512.0 | 0 |
| 20261119-002610_relay-ho-ab200 | 50 | 202.8 | 347.4 | 498.1 | 0 |
| 20261119-002826_relay-ho-ab200 | 50 | 121.0 | 274.4 | 420.7 | 5 |
| 20261119-003043_relay-ho-ab200 | 50 | 159.5 | 359.7 | 363.2 | 0 |
| 20261119-003225_relay-ho-ab200 | 50 | 258.4 | 357.7 | 457.0 | 0 |

Separate row, not a replacement: the next grid value above the held-out maximum is T = 160 ms, chosen after the held-out maximum was seen. Held-out: 0 of 500 honest rounds rejected (0.0 %), 7 of 500 relayed rounds accepted (1.4 %).

## 7b. Operating curve on the held-out campaigns

Honest rounds: 500 from 10 direct campaigns. Relayed rounds: 500 from 10 relayed campaigns. A round is rejected by a bound T when it has no answer or its RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.

| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |
|---|---|---|
| 100 | 10.2 % | 0.0 % |
| 110 | 0.8 % | 0.0 % |
| 120 | 0.6 % | 0.0 % |
| 130 | 0.4 % | 1.0 % |
| 140 | 0.4 % | 1.0 % |
| 150 | 0.2 % | 1.0 % |
| 160 | 0.0 % | 1.4 % |
| 170 | 0.0 % | 2.2 % |
| 180 | 0.0 % | 3.0 % |
| 200 | 0.0 % | 5.0 % |
| 250 | 0.0 % | 10.4 % |
| 300 | 0.0 % | 35.4 % |

The largest honest RTT is 150.6 ms. A bound at that value rejects no honest round in sample, and accepts 1.0 % of relayed rounds.

## 8. Operating curve, all M-19 firmware campaigns at 2.24 m (in-sample, held-out and replication pooled)

Honest rounds: 1400 from 28 direct campaigns. Relayed rounds: 1400 from 28 relayed campaigns. A round is rejected by a bound T when it has no answer or its RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.

| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |
|---|---|---|
| 100 | 12.7 % | 0.0 % |
| 110 | 1.0 % | 0.6 % |
| 120 | 0.6 % | 1.1 % |
| 130 | 0.4 % | 1.5 % |
| 140 | 0.2 % | 1.5 % |
| 150 | 0.1 % | 1.5 % |
| 160 | 0.0 % | 1.9 % |
| 170 | 0.0 % | 2.6 % |
| 180 | 0.0 % | 3.5 % |
| 200 | 0.0 % | 4.5 % |
| 250 | 0.0 % | 10.1 % |
| 300 | 0.0 % | 28.7 % |

The largest honest RTT is 150.6 ms. A bound at that value rejects no honest round in sample, and accepts 1.5 % of relayed rounds.

## 9. Replication of the 160 ms bound (fixed before these campaigns)

T = 160 ms was fixed in `replication-160ms-plan.md` before these campaigns were recorded. Replication: 10 direct and 10 relayed campaigns, 500 honest and 500 relayed rounds.

Criterion 1, honest false rejects: 0 of 500 honest rounds above T or unanswered (0.0 %), expected 0. Met.

Criterion 2, relayed accepts: 13 of 500 relayed rounds at or below T (2.6 %), 95 % Wilson interval 1.5 to 4.4 %. Reported as found; the plan sets no pass threshold for it.

The largest replication honest RTT is 143.3 ms.

| Direct campaign | Rounds above T or unanswered | Max answered RTT (ms) |
|---|---|---|
| 20261119-003843_baseline-rep | 0 | 87.2 |
| 20261119-004105_baseline-rep | 0 | 143.3 |
| 20261119-004435_baseline-rep | 0 | 107.4 |
| 20261119-004651_baseline-rep | 0 | 107.6 |
| 20261119-004906_baseline-rep | 0 | 125.0 |
| 20261119-005121_baseline-rep | 0 | 64.8 |
| 20261119-005449_baseline-rep | 0 | 67.2 |
| 20261119-005704_baseline-rep | 0 | 100.1 |
| 20261119-005919_baseline-rep | 0 | 112.4 |
| 20261119-010134_baseline-rep | 0 | 108.4 |

| Relayed campaign | Answered rounds | Min (ms) | Median (ms) | Max (ms) | Rounds at or below T |
|---|---|---|---|---|---|
| 20261119-004004_relay-rep-ab200 | 50 | 270.9 | 370.0 | 417.3 | 0 |
| 20261119-004334_relay-rep-ab200 | 50 | 199.4 | 343.9 | 446.3 | 0 |
| 20261119-004549_relay-rep-ab200 | 50 | 164.2 | 362.1 | 417.9 | 0 |
| 20261119-004805_relay-rep-ab200 | 50 | 172.8 | 368.5 | 372.1 | 0 |
| 20261119-005020_relay-rep-ab200 | 50 | 107.4 | 401.9 | 406.1 | 8 |
| 20261119-005348_relay-rep-ab200 | 50 | 109.8 | 262.8 | 365.7 | 5 |
| 20261119-005603_relay-rep-ab200 | 50 | 275.1 | 374.0 | 430.5 | 0 |
| 20261119-005819_relay-rep-ab200 | 50 | 297.5 | 449.1 | 454.5 | 0 |
| 20261119-010034_relay-rep-ab200 | 50 | 208.8 | 304.3 | 409.3 | 0 |
| 20261119-010401_relay-rep-ab200 | 50 | 308.2 | 407.5 | 453.6 | 0 |

## 9b. Operating curve on the replication campaigns

Honest rounds: 500 from 10 direct campaigns. Relayed rounds: 500 from 10 relayed campaigns. A round is rejected by a bound T when it has no answer or its RTT is above T; a relayed round is accepted when it is answered and its RTT is at most T.

| Bound T (ms) | Honest rejected (FRR) | Relayed accepted (FAR) |
|---|---|---|
| 100 | 20.2 % | 0.0 % |
| 110 | 0.6 % | 1.8 % |
| 120 | 0.4 % | 2.0 % |
| 130 | 0.2 % | 2.0 % |
| 140 | 0.2 % | 2.0 % |
| 150 | 0.0 % | 2.0 % |
| 160 | 0.0 % | 2.6 % |
| 170 | 0.0 % | 3.0 % |
| 180 | 0.0 % | 4.2 % |
| 200 | 0.0 % | 5.0 % |
| 250 | 0.0 % | 9.6 % |
| 300 | 0.0 % | 24.4 % |

The largest honest RTT is 143.3 ms. A bound at that value rejects no honest round in sample, and accepts 2.0 % of relayed rounds.

## Notes

- Sections 3 and 4 pool every campaign: the 2026-11-18 campaigns, the M-19 in-sample campaigns and the held-out campaigns. The firmware before M-19 sent round 1 at START, so the phase of those campaigns followed from the connection timing. Sections 5 and 6 use the M-19 campaigns only, whose offsets are drawn at random.
- Within one campaign the sends stay on one phase, so every campaign's RTT is shifted by its own phase (section 1 shows R per campaign).
- The bound is set from honest data only. Relayed data never set it.
- The M-19 sections are at one distance (car-key 2.24 m, A-B 2 m). The 0.52 m and 0.99 m campaigns are in sections 1 to 4 only.
- Sections 7 and 9 are the tests of T on campaigns recorded after T was fixed (150 ms and 160 ms). Sections 3 to 6 and 8 are in-sample or pooled.
- A bound is valid for this stack and these distances. It is not a general defence (LIMITATIONS.md, limit 2).
