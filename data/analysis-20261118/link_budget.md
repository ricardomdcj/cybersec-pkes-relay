# Link-budget estimate (model, not measured)

The bench reached about 2 m between relay A and relay B, so every range below is an
extrapolation. Path loss: PL(d) = PL0 + 10 n log10(d), with PL0 = FSPL(1 m) = 40.1 dB at 2.412 GHz. The range is the distance where the received power
equals the sensitivity plus the fade margin.

## Measured RSSI of the direct baseline (BLE, mean of OK rounds)

| Distance (m) | Mean RSSI (dBm) |
|---|---|
| 0.52 | -33.1 |
| 0.99 | -53.2 |
| 2.24 | -59.6 |

Local exponents between neighbouring points: 0.52 to 0.99 m: n = 7.2; 0.99 to 2.24 m: n = 1.8.
They differ by a factor of about four, so one exponent does not describe this setup (antenna
orientation and near-field effects). The bench cannot calibrate n, so the ranges use n = 2, 3 and 4.

## Range estimates

| Link | TX (dBm) | Sensitivity (dBm) | n = 2 (open, line of sight) | n = 3 (indoor, obstructed) | n = 4 (dense indoor) |
|---|---|---|---|---|---|
| ESP-NOW tunnel, TX 19.5 dBm (default, typical) | 19.5 | -98 | 2.3 km | 177 m | 48 m |
| ESP-NOW tunnel, TX 2 dBm (lowest used) | 2.0 | -98 | 313 m | 46 m | 18 m |
| BLE legs (car to relay B, relay A to key) | 0.0 | -89 | 88 m | 20 m | 9 m |

## Assumptions

- Sensitivity: -98 dBm (ESP-NOW, 802.11b 1 Mbps) and -89 dBm (BLE 1M PHY). Typical values from the ESP32 Series Datasheet v5.3 (Tables 5-6 and 5-7); the bench boards were not characterised.
- Fade margin 10 dB; antenna gain 0 dBi at each end (the CYD PCB antennas were not characterised).
- BLE TX 0 dBm, a typical setting; not measured on this bench.
- TX 19.5 dBm is the typical ESP-NOW power (Table 5-6, 11b mode); TX 2 dBm is the lowest value the bench used (TXPOW 8).
- The car-key distances 0.99 and 2.24 m are estimates from the layout.
