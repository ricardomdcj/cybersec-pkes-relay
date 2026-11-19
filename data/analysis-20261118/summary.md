# Campaign summary, 2026-11-18

Each condition pools the campaigns of one setup: three campaigns of 50 rounds, eight for the M-19 setups, and ten for the held-out and replication setups (random start offset of the round grid). Distance is the car-key separation. For relayed conditions, CSVB rounds equal the OK rounds (every answered round went through the relays).

## Outcomes

| Condition | Tunnel | Car-key (m) | Campaigns | Rounds | OK | FAIL | TIMEOUT | CSVB rounds | Success (95 % CI) |
|---|---|---|---|---|---|---|---|---|---|
| baseline 0.52 | direct | 0.52 | 3 | 150 | 150 | 0 | 0 | 0 | 100.0 % (97.5-100.0) |
| broadcast 0.52 | broadcast | 0.52 | 3 | 150 | 139 | 0 | 11 | 139 | 92.7 % (87.3-95.9) |
| unicast 0.52 | unicast | 0.52 | 3 | 150 | 150 | 0 | 0 | 150 | 100.0 % (97.5-100.0) |
| unicast 2 dBm 0.52 | unicast, TXPOW 8 | 0.52 | 3 | 150 | 150 | 0 | 0 | 150 | 100.0 % (97.5-100.0) |
| baseline 0.99 | direct | 0.99 | 3 | 150 | 150 | 0 | 0 | 0 | 100.0 % (97.5-100.0) |
| unicast A-B 0.75 (car-key 0.99) | unicast, A-B 0.75 m | 0.99 | 3 | 150 | 150 | 0 | 0 | 150 | 100.0 % (97.5-100.0) |
| baseline 2.24 | direct | 2.24 | 3 | 150 | 150 | 0 | 0 | 0 | 100.0 % (97.5-100.0) |
| unicast A-B 2 (car-key 2.24) | unicast, A-B 2 m | 2.24 | 3 | 150 | 150 | 0 | 0 | 150 | 100.0 % (97.5-100.0) |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | unicast, A-B 2 m, TXPOW 8 | 2.24 | 3 | 150 | 150 | 0 | 0 | 150 | 100.0 % (97.5-100.0) |
| baseline 2.24 M-19 | direct | 2.24 | 8 | 400 | 400 | 0 | 0 | 0 | 100.0 % (99.0-100.0) |
| unicast A-B 2 M-19 (car-key 2.24) | unicast, A-B 2 m | 2.24 | 8 | 400 | 400 | 0 | 0 | 400 | 100.0 % (99.0-100.0) |
| baseline 2.24 held-out | direct | 2.24 | 10 | 500 | 500 | 0 | 0 | 0 | 100.0 % (99.2-100.0) |
| unicast A-B 2 held-out (car-key 2.24) | unicast, A-B 2 m | 2.24 | 10 | 500 | 500 | 0 | 0 | 500 | 100.0 % (99.2-100.0) |
| baseline 2.24 replication | direct | 2.24 | 10 | 500 | 500 | 0 | 0 | 0 | 100.0 % (99.2-100.0) |
| unicast A-B 2 replication (car-key 2.24) | unicast, A-B 2 m | 2.24 | 10 | 500 | 500 | 0 | 0 | 500 | 100.0 % (99.2-100.0) |

## Vehicle RTT, answered rounds (ms)

| Condition | n | min | Q1 | median (95 % CI) | Q3 | p90 | max | mean |
|---|---|---|---|---|---|---|---|---|
| baseline 0.52 | 150 | 57.7 | 67.9 | 76.9 (75.2-78.7) | 101.3 | 105.0 | 128.2 | 81.9 |
| broadcast 0.52 | 139 | 167.7 | 295.2 | 366.1 (358.0-384.4) | 393.4 | 416.6 | 458.8 | 348.9 |
| unicast 0.52 | 150 | 169.7 | 313.5 | 384.6 (339.9-387.5) | 390.6 | 441.2 | 443.9 | 354.0 |
| unicast 2 dBm 0.52 | 150 | 164.2 | 312.6 | 367.4 (365.9-411.6) | 415.6 | 433.5 | 468.4 | 355.9 |
| baseline 0.99 | 150 | 62.7 | 68.2 | 82.0 (80.2-83.6) | 93.1 | 96.4 | 142.4 | 82.5 |
| unicast A-B 0.75 (car-key 0.99) | 150 | 112.4 | 327.6 | 376.6 (366.1-408.5) | 411.2 | 430.0 | 528.6 | 362.9 |
| baseline 2.24 | 150 | 63.8 | 69.1 | 85.7 (84.4-86.6) | 88.4 | 90.6 | 139.9 | 82.3 |
| unicast A-B 2 (car-key 2.24) | 150 | 158.2 | 311.1 | 364.5 (362.8-365.7) | 412.2 | 453.9 | 456.7 | 357.8 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 150 | 113.4 | 261.0 | 316.2 (314.4-374.2) | 377.6 | 408.2 | 468.4 | 323.3 |
| baseline 2.24 M-19 | 400 | 57.6 | 64.9 | 74.7 (72.1-77.6) | 84.9 | 89.6 | 136.1 | 76.1 |
| unicast A-B 2 M-19 (car-key 2.24) | 400 | 115.3 | 299.7 | 397.5 (359.2-399.7) | 425.6 | 446.0 | 550.5 | 361.8 |
| baseline 2.24 held-out | 500 | 57.6 | 69.2 | 81.2 (79.7-83.5) | 95.4 | 100.1 | 150.6 | 82.6 |
| unicast A-B 2 held-out (car-key 2.24) | 500 | 121.0 | 275.6 | 356.7 (346.6-359.0) | 399.0 | 432.0 | 512.0 | 337.0 |
| baseline 2.24 replication | 500 | 57.5 | 61.7 | 80.2 (77.2-83.3) | 97.7 | 104.9 | 143.3 | 80.8 |
| unicast A-B 2 replication (car-key 2.24) | 500 | 107.4 | 302.1 | 354.7 (346.5-363.2) | 401.9 | 430.3 | 454.5 | 341.1 |

## Campaign spread: median RTT of OK rounds per campaign (ms)

| Condition | campaign 1 | campaign 2 | campaign 3 | campaign 4 | campaign 5 | campaign 6 | campaign 7 | campaign 8 | campaign 9 | campaign 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| baseline 0.52 | 103.5 | 77.3 | 66.6 | - | - | - | - | - | - | - |
| broadcast 0.52 | 369.8 | 391.5 | 357.9 | - | - | - | - | - | - | - |
| unicast 0.52 | 385.0 | 314.4 | 389.0 | - | - | - | - | - | - | - |
| unicast 2 dBm 0.52 | 367.2 | 312.6 | 431.9 | - | - | - | - | - | - | - |
| baseline 0.99 | 81.7 | 66.4 | 94.4 | - | - | - | - | - | - | - |
| unicast A-B 0.75 (car-key 0.99) | 365.6 | 408.2 | 376.6 | - | - | - | - | - | - | - |
| baseline 2.24 | 88.9 | 67.2 | 85.9 | - | - | - | - | - | - | - |
| unicast A-B 2 (car-key 2.24) | 452.5 | 362.0 | 366.2 | - | - | - | - | - | - | - |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 314.1 | 359.4 | 376.0 | - | - | - | - | - | - | - |
| baseline 2.24 M-19 | 61.4 | 64.5 | 66.1 | 81.4 | 75.0 | 64.5 | 85.1 | 88.0 | - | - |
| unicast A-B 2 M-19 (car-key 2.24) | 358.2 | 347.0 | 263.0 | 322.4 | 410.9 | 432.5 | 397.4 | 444.7 | - | - |
| baseline 2.24 held-out | 103.0 | 91.9 | 69.6 | 78.5 | 82.9 | 67.9 | 96.9 | 96.5 | 61.1 | 69.6 |
| unicast A-B 2 held-out (car-key 2.24) | 427.1 | 300.5 | 394.8 | 447.3 | 311.5 | 358.5 | 347.4 | 274.4 | 359.7 | 357.7 |
| baseline 2.24 replication | 83.8 | 95.4 | 61.0 | 82.3 | 77.2 | 61.4 | 63.4 | 96.6 | 61.4 | 103.4 |
| unicast A-B 2 replication (car-key 2.24) | 370.0 | 343.9 | 362.1 | 368.5 | 401.9 | 262.8 | 374.0 | 449.1 | 304.3 | 407.5 |

## Round legs, median (IQR) in ms

| Condition | vehicle<->B | tunnel | A<->key |
|---|---|---|---|
| baseline 0.52 | - | - | - |
| broadcast 0.52 | 151.6 (144.8-170.4) | 23.9 (22.8-25.6) | 177.6 (171.1-198.7) |
| unicast 0.52 | 157.1 (115.4-173.4) | 24.0 (23.2-26.0) | 188.7 (172.5-240.1) |
| unicast 2 dBm 0.52 | 169.8 (118.7-226.6) | 24.4 (22.9-26.6) | 178.8 (167.5-188.5) |
| baseline 0.99 | - | - | - |
| unicast A-B 0.75 (car-key 0.99) | 144.5 (121.3-172.5) | 24.3 (23.5-26.4) | 215.0 (195.5-232.3) |
| baseline 2.24 | - | - | - |
| unicast A-B 2 (car-key 2.24) | 131.2 (96.3-192.9) | 24.4 (23.3-26.6) | 209.0 (193.0-235.0) |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 128.4 (120.9-160.8) | 24.1 (23.3-26.1) | 187.9 (161.2-194.8) |
| baseline 2.24 M-19 | - | - | - |
| unicast A-B 2 M-19 (car-key 2.24) | 161.9 (126.3-189.7) | 24.5 (23.5-26.6) | 193.9 (164.8-218.1) |
| baseline 2.24 held-out | - | - | - |
| unicast A-B 2 held-out (car-key 2.24) | 131.2 (90.8-165.8) | 24.5 (23.5-26.2) | 202.8 (162.6-212.6) |
| baseline 2.24 replication | - | - | - |
| unicast A-B 2 replication (car-key 2.24) | 156.0 (99.0-167.8) | 24.7 (23.6-26.5) | 179.2 (160.5-222.7) |

## Tunnel counters

| Condition | tx_failures (relay A + B) | TIMEOUT |
|---|---|---|
| baseline 0.52 | 0 | 0 |
| broadcast 0.52 | 0 | 11 |
| unicast 0.52 | 0 | 0 |
| unicast 2 dBm 0.52 | 0 | 0 |
| baseline 0.99 | 0 | 0 |
| unicast A-B 0.75 (car-key 0.99) | 0 | 0 |
| baseline 2.24 | 0 | 0 |
| unicast A-B 2 (car-key 2.24) | 0 | 0 |
| unicast A-B 2 TX 2 dBm (car-key 2.24) | 0 | 0 |
| baseline 2.24 M-19 | 0 | 0 |
| unicast A-B 2 M-19 (car-key 2.24) | 0 | 0 |
| baseline 2.24 held-out | 0 | 0 |
| unicast A-B 2 held-out (car-key 2.24) | 0 | 0 |
| baseline 2.24 replication | 0 | 0 |
| unicast A-B 2 replication (car-key 2.24) | 0 | 0 |

## Comparisons (difference of medians, ms)

| Comparison | Reference median | Compared median | Difference (95 % CI) |
|---|---|---|---|
| relayed unicast minus direct, car-key 0.52 m | 76.9 | 384.6 | 307.7 (262.7 to 311.4) |
| relayed broadcast minus direct, car-key 0.52 m | 76.9 | 366.1 | 289.2 (280.4 to 313.1) |
| relayed unicast 2 dBm minus direct, car-key 0.52 m | 76.9 | 367.4 | 290.5 (287.9 to 334.9) |
| relayed unicast minus direct, car-key 0.99 m | 82.0 | 376.6 | 294.6 (283.8 to 327.3) |
| relayed unicast minus direct, car-key 2.24 m | 85.7 | 364.5 | 278.9 (277.0 to 280.4) |
| unicast minus broadcast, car-key 0.52 m | 366.1 | 384.6 | 18.5 (-40.2 to 28.6) |
| 2 dBm minus default power, car-key 0.52 m | 384.6 | 367.4 | -17.2 (-20.9 to 69.6) |
| relayed unicast 2 dBm minus direct, car-key 2.24 m | 85.7 | 316.2 | 230.5 (228.7 to 288.5) |
| 2 dBm minus default power, A-B 2 m | 364.5 | 316.2 | -48.4 (-50.3 to 9.6) |
| direct baseline, 0.99 m minus 0.52 m | 76.9 | 82.0 | 5.1 (2.8 to 7.4) |
| direct baseline, 2.24 m minus 0.99 m | 82.0 | 85.7 | 3.7 (1.6 to 5.6) |
| relayed unicast minus direct, car-key 2.24 m, M-19 firmware | 74.7 | 397.5 | 322.8 (283.6 to 326.2) |
| relayed unicast minus direct, car-key 2.24 m, held-out campaigns | 81.2 | 356.7 | 275.5 (264.7 to 278.3) |
| relayed unicast minus direct, car-key 2.24 m, replication campaigns | 80.2 | 354.7 | 274.6 (266.1 to 284.4) |

## Notes

- TIMEOUT rounds are excluded from the RTT statistics and counted in the success rate.
- Wilson 95 % CI for the success rate; bootstrap (2000 resamples, fixed seed) for medians and differences.
- Round legs follow analyze_runs.py: vehicle<->B = vehicle RTT - turnaround of B; tunnel = turnaround of B - turnaround of A; A<->key = turnaround of A.
- Broadcast sessions ran with RELAY_PEER_MAC unset; unicast sessions with it set (branch relay-unicast-peer-mac).
- 2 dBm sessions applied TXPOW 8 to both relays after the bring-up; the relay logs confirm it.
- Car-key separations of 2.24 m are estimated from the layout (12 cm + about 2 m + 12 cm).
- Rounds are not independent: the RTT moves in plateaus within a campaign. The bootstrap intervals assume independent rounds, so they are optimistic.
- The spread between campaigns (campaign spread table) is larger than most differences between settings. Three campaigns per condition do not resolve differences below about 50 ms; the M-19 setups have eight and the held-out and replication setups ten.
