# Statistics for the results (campaign as the unit)

Unit: the campaign. Rounds inside a campaign share one phase of the connection grid, so the tests use campaign medians, and the bootstrap resamples campaigns within each condition (2000 resamples, seed 20261119). Cliff's delta equals 2 x AUC - 1; a positive value means the relayed rounds are longer. Round-level values are descriptive only.

## 1. Overhead and campaign-level comparison

| Group | Campaigns (direct, relayed) | Median direct (ms) | Median relayed (ms) | Overhead (ms), 95 % CI | Campaign-level MW p (test) | Cliff's delta, campaigns, 95 % CI | Round-level Cliff's delta (descriptive) |
|---|---|---|---|---|---|---|---|
| 2.24 m, M-19 in-sample | 8, 8 | 74.7 | 397.5 | 322.8 (273.9 to 343.8) | 0.0002 (exact) | 1.000 (1.000 to 1.000) | 1.000 |
| 2.24 m, held-out | 10, 10 | 81.2 | 356.7 | 275.5 (211.5 to 297.4) | 0.0000 (exact) | 1.000 (1.000 to 1.000) | 1.000 |
| 2.24 m, replication | 10, 10 | 80.2 | 354.7 | 274.6 (225.7 to 305.4) | 0.0000 (exact) | 1.000 (1.000 to 1.000) | 1.000 |
| 2.24 m, pre-M-19 firmware | 3, 3 | 85.7 | 364.5 | 278.9 (274.8 to 366.8) | 0.1000 (exact) | 1.000 (1.000 to 1.000) | 1.000 |
| 0.99 m, pre-M-19 firmware | 3, 3 | 82.0 | 376.6 | 294.6 (273.4 to 340.0) | 0.1000 (exact) | 1.000 (1.000 to 1.000) | 0.999 |
| 0.52 m, pre-M-19 firmware | 3, 3 | 76.9 | 384.6 | 307.7 (215.1 to 319.6) | 0.1000 (exact) | 1.000 (1.000 to 1.000) | 1.000 |
| 2.24 m, M-19 family pooled | 28, 28 | 79.1 | 358.5 | 279.5 (262.5 to 296.6) | 1.33e-10 (normal) | 1.000 (1.000 to 1.000) | 1.000 |

## 2. Holm correction across the groups (campaign-level p-values)

Family: the six groups in table 1 (the pooled row is not included, to avoid counting the same campaigns twice).

| Group | Raw p | Holm-adjusted p |
|---|---|---|
| 2.24 m, M-19 in-sample | 0.0002 | 0.0006 |
| 2.24 m, held-out | 0.0000 | 0.0001 |
| 2.24 m, replication | 0.0000 | 0.0001 |
| 2.24 m, pre-M-19 firmware | 0.1000 | 0.3000 |
| 0.99 m, pre-M-19 firmware | 0.1000 | 0.3000 |
| 0.52 m, pre-M-19 firmware | 0.1000 | 0.3000 |

With three campaigns per side, the smallest two-sided exact p-value is 2/20 = 0.10: the pre-M-19 sets cannot reach p < 0.05 with any outcome. The M-19 sets (8 or 10 campaigns per side) can.

## 3. Discrimination by RTT and RSSI (RQ3), 2.24 m, M-19 family

AUC is P(relayed > direct) with ties counted half; 0.5 means no discrimination, 1.0 perfect separation in the direction relayed > direct. Campaign-level AUC uses one value per campaign; the interval is the cluster bootstrap over campaigns.

| Signal | Level | AUC (95 % CI) |
|---|---|---|
| RTT (median per campaign) | campaign, 28 vs 28 | 1.000 (1.000 to 1.000) |
| RSSI (median per campaign) | campaign, 28 vs 28 | 1.000 (1.000 to 1.000) |
| RSSI (rounds) | round, descriptive | 1.000 (1.000 to 1.000) |
| RTT (rounds) | round, descriptive | 1.000 |

The paper's RQ3 criterion (AUC above 0.5 in every campaign) cannot be applied as written: each campaign is a single condition, so a per-campaign AUC has one class only. The campaign-level AUC above replaces it.

## 4. Timeouts per configuration (H2)

| Condition | Campaigns | Rounds | Timeouts | Timeouts per 100 rounds |
|---|---|---|---|---|
| baseline 0.52 | 3 | 150 | 0 | 0.0 |
| baseline 0.99 | 3 | 150 | 0 | 0.0 |
| baseline 2.24 | 3 | 150 | 0 | 0.0 |
| baseline 2.24 M-19 | 8 | 400 | 0 | 0.0 |
| baseline 2.24 held-out | 10 | 500 | 0 | 0.0 |
| baseline 2.24 replication | 10 | 500 | 0 | 0.0 |
| broadcast 0.52 | 3 | 150 | 11 | 7.3 |
| unicast 0.52 | 3 | 150 | 0 | 0.0 |
| unicast 2 dBm 0.52 | 3 | 150 | 0 | 0.0 |
| unicast A-B 0.75 (car-key 0.99) | 3 | 150 | 0 | 0.0 |
| unicast A-B 2 (car-key 2.24) | 3 | 150 | 0 | 0.0 |
| unicast A-B 2 M-19 (car-key 2.24) | 8 | 400 | 0 | 0.0 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 3 | 150 | 0 | 0.0 |
| unicast A-B 2 held-out (car-key 2.24) | 10 | 500 | 0 | 0.0 |
| unicast A-B 2 replication (car-key 2.24) | 10 | 500 | 0 | 0.0 |

Broadcast relay (no MAC ACK), 0.52 m: 11 timeouts in 150 rounds (7.3 %). The unicast relay (MAC ACK) had none at any distance. Broadcast was only run at 0.52 m, so the failure cannot be separated from the distance on this bench; it does point to the transport (lost frames without acknowledgement) rather than to the distance (H2).

## 5. Sample size for Delta = 25 ms (half the BLE connection interval)

n per group = 2 (z(0.975) + z(0.80))^2 sigma^2 / Delta^2, with sigma the campaign-to-campaign standard deviation of the campaign medians (alpha 0.05, two-sided, power 0.80).

| Comparison | sigma (ms) | n per group for Delta = 25 ms |
|---|---|---|
| Direct campaigns (M-19 family) | 14.2 | 5.0 |
| Relayed campaigns (M-19 family) | 54.0 | 73.3 |
| Two relayed configurations (relayed SD) | 54.0 | 73.3 |
| Direct vs relayed (pooled SD, overhead about 300 ms) | 39.5 | 39.2 (overhead is far larger than Delta) |

The paper's current wording ('30 repetitions per cell') underestimates the campaigns needed to resolve 25 ms between two relayed configurations, because the campaign-to-campaign spread of relayed medians is about ten times the spread of the direct ones (the phase of each campaign). The overhead itself is resolved with a handful of campaigns.

## Not covered here

- Logistic regression of the decision on distance and T_max (the paper's RQ2 model): needs the per-round decision for each T_max, which the bench only yields by applying cutoffs (cutoff.md, sections 3 to 9). Not fitted.
- Mixed model with campaign as a random effect: approximated here by the campaign-level tests and the cluster bootstrap. A fitted mixed model needs statsmodels, not installed on the bench.

