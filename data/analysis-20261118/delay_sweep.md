# Latency-injection sweep (relay B holds each challenge for D ms, 0.99 m)

The verifier authenticates a round when its response arrives within 2000 ms. Each row is one campaign of 50 rounds, unicast relay, default TX power, M-19 firmware.

| D (ms) | Rounds | OK | FAIL | TIMEOUT | Success (95 % Wilson) | Median RTT of OK rounds (ms) | Min / max of OK (ms) | Logged DELAY (ms) | Campaign |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 50 | 50 | 0 | 0 | 100.0 % (92.9 to 100.0) | 358.6 | 303.3 / 457.3 | 0 | 20261119-080236_relay-d0000 |
| 500 | 50 | 50 | 0 | 0 | 100.0 % (92.9 to 100.0) | 879.7 | 725.8 / 925.6 | 500 | 20261119-080729_relay-d0500 |
| 1000 | 50 | 50 | 0 | 0 | 100.0 % (92.9 to 100.0) | 1382.3 | 1280.2 / 1442.1 | 1000 | 20261119-080951_relay-d1000 |
| 1400 | 50 | 50 | 0 | 0 | 100.0 % (92.9 to 100.0) | 1784.1 | 1583.9 / 1845.9 | 1400 | 20261119-081133_relay-d1400 |
| 1500 | 50 | 50 | 0 | 0 | 100.0 % (92.9 to 100.0) | 1784.9 | 1680.5 / 1946.0 | 1500 | 20261119-081516_relay-d1500 |
| 1600 | 50 | 36 | 0 | 14 | 72.0 % (58.3 to 82.5) | 1984.5 | 1782.8 / 1998.0 | 1600 | 20261119-081819_relay-d1600 |
| 1625 | 50 | 45 | 0 | 5 | 90.0 % (78.6 to 95.7) | 1980.3 | 1871.9 / 1998.1 | 1625 | 20261119-083618_relay-d1625 |
| 1650 | 50 | 27 | 0 | 23 | 54.0 % (40.4 to 67.0) | 1942.9 | 1856.9 / 1985.0 | 1650 | 20261119-082032_relay-d1650 |
| 1675 | 50 | 18 | 0 | 32 | 36.0 % (24.1 to 49.9) | 1884.1 | 1830.8 / 1995.5 | 1675 | 20261119-083831_relay-d1675 |
| 1700 | 50 | 8 | 0 | 42 | 16.0 % (8.3 to 28.5) | 1984.3 | 1973.3 / 1992.3 | 1700 | 20261119-082356_relay-d1700 |
| 1750 | 50 | 8 | 0 | 42 | 16.0 % (8.3 to 28.5) | 1965.0 | 1933.0 / 1991.4 | 1750 | 20261119-082610_relay-d1750 |
| 1800 | 50 | 0 | 0 | 50 | 0.0 % (0.0 to 7.1) | nan | nan / nan | 1800 | 20261119-082824_relay-d1800 |
| 1900 | 50 | 0 | 0 | 50 | 0.0 % (0.0 to 7.1) | nan | nan / nan | 1900 | 20261119-083137_relay-d1900 |
| 2200 | 50 | 0 | 0 | 50 | 0.0 % (0.0 to 7.1) | nan | nan / nan | 2200 | 20261119-083350_relay-d2200 |

Honest relayed RTT at D = 0: median 358.6 ms. Expected threshold, timeout minus that median: 1641 ms of injected delay.

Measured threshold (success falls below 50 %): D50 = 1656 ms of injected delay, so the relay tolerates about 1656 ms of added latency at this timeout.

The relay's effectiveness limit is set by the verifier's timeout and by the honest relayed RTT, not by the distance: the timeout is a bench constant, and a real vehicle may use a different one.

