# Campaign summary

| Condition | Dist (m) | Rounds | OK | FAIL | TIMEOUT | Success (95 % CI) |
|---|---|---|---|---|---|---|
| baseline_20261116-220429 | - | 20 | 20 | 0 | 0 | 100.0 % (83.9-100.0) |
| relayed_20261116-220429 | - | 20 | 20 | 0 | 0 | 100.0 % (83.9-100.0) |

## Vehicle RTT (answered rounds, ms)

| Condition | n | min | Q1 | median (95 % CI) | Q3 | p90 | max | mean | sd |
|---|---|---|---|---|---|---|---|---|---|
| baseline_20261116-220429 | 20 | 83.0 | 86.6 | 91.0 (87.9-91.1) | 91.2 | 91.3 | 97.5 | 89.5 | 3.3 |
| relayed_20261116-220429 | 20 | 263.0 | 291.0 | 334.7 (291.0-386.4) | 390.2 | 390.8 | 441.1 | 339.3 | 53.1 |

## Round legs (median, ms)

| Condition | vehicle<->B | tunnel | A<->key |
|---|---|---|---|
| baseline_20261116-220429 | - | - | - |
| relayed_20261116-220429 | 152.4 (IQR 125.0-174.8) | 21.6 (IQR 21.2-22.6) | 193.2 (IQR 94.2-243.4) |

## Overhead relative to 'baseline_20261116-220429' (difference of medians, ms)

| Condition | difference of medians (95 % CI) |
|---|---|
| relayed_20261116-220429 | 243.7 (200.0 to 295.4) |

Notes: TIMEOUT rounds are excluded from the RTT statistics and included in the success rate. CI of the median and of the difference by bootstrap (2000 resamples, seed 20261117). CI of the rate by Wilson 95 %.
