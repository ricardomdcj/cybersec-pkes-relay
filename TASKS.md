# Tasks and next steps

Milestone 1 (ESP-NOW tunnel + RTT self-test) is done. The remaining work is: bring BLE up on
both relays, then run the measurement campaign the paper needs. Grouped by milestone; the
order within a group is roughly the order to tackle it. The detailed, per-item plan (with
dependencies and checkpoints) is in `IMPROVEMENTS.md`.

## CYD port (done)

All roles moved to CYD boards (ESP32-2432S028R, classic ESP32) with an on-screen status UI
(shared `components/cyd`, a mascot per role). The Android key became the `key/` firmware.
The contract and the HMAC are a single shared `components/pkes_contract`. Buttonless reflashing
via serial OTA (`components/serial_ota` + `tools/serial_ota.py`).

- [x] `components/cyd` (display + LED + status UI/mascot), `components/pkes_contract`, `components/serial_ota`.
- [x] `key/` firmware (replaces `phoneaskey/`), `vehicle/` + `relays/` on `esp32` with the UI.
- [x] All four firmwares build; `key` and `vehicle` booted on hardware; serial OTA proven (buttonless).
- [ ] Boot `relays` (both roles) on hardware and run the full chain on the CYDs.
- [ ] Optional: raise the console/OTA baud above 115200 to speed up serial OTA (~100 s per push).

## Milestone 2 — BLE on both relay ends

Goal: replace the self-test echo with the real key↔vehicle exchange carried over the tunnel,
then set `RELAY_LINK_SELFTEST 0` in `relays/main/relay_config.h`.

### Relay A — BLE central (near the key) — `relays/main/relay_a.c`
- [x] Port the central logic from `vehicle/main/main.c`: scan for `SVC_UUID`, connect to the key,
      discover services and characteristics.
- [x] Cache the CHALLENGE and RESPONSE handles; enable notify (CCCD) on RESPONSE.
- [x] On a challenge from the tunnel, write the nonce to the key's CHALLENGE characteristic
      (`ble_gattc_write_flat`, with response, as the vehicle does). The GATT write runs on a
      worker task, not in the ESP-NOW (WiFi) callback.
- [x] On `BLE_GAP_EVENT_NOTIFY_RX` from the key, forward the HMAC back over the tunnel.
- [x] Handle reconnect and scan restart if the key drops.

### Relay B — BLE peripheral (near the vehicle) — `relays/main/relay_b.c`
- [x] Advertise a cloned GATT service with the same `SERVICE_UUID` and CHALLENGE/RESPONSE
      characteristics (NimBLE adds the CCCD for the notify characteristic).
- [x] On the vehicle's write to CHALLENGE, send the nonce over the tunnel.
- [x] On the response from Relay A, notify the RESPONSE characteristic to the vehicle (on a
      worker task, not in the ESP-NOW callback).
- [x] Keep the self-test path (`#if RELAY_LINK_SELFTEST`) compiling, so the tunnel baseline can
      still be reproduced.

### Integration — needs the four boards (not yet run on hardware)
- [ ] Build and flash each relay role with `-DRELAY_LINK_SELFTEST=0`, then run the full chain:
      key → Relay A → Relay B → vehicle.
- [ ] Confirm the vehicle logs `OK` rounds (the relayed HMAC verifies), proving that the relay
      forwards a live exchange end to end.

> The code compiles cleanly for `esp32` (vehicle, key, both relays in BLE and self-test mode).
> Only the on-hardware run remains.

## Milestone 3 — Measurement campaign (feeds the paper)

- [ ] Collect the honest baseline with `vehicle/` directly against the key (`RTT` distribution
      over `ROUNDS`); record min, median, and max.
- [ ] Collect relayed `RTT` over the full chain under the same `ROUNDS` and `INTERVAL_MS`.
- [x] Storage format decided and emitted by the vehicle: one `CSV` row per round (CSVHDR v2),
      with a run label and distance from the `RUN` command, the negotiated connection interval
      and MTU. `tools/capture_run.py` captures it; `tools/analyze_runs.py` joins and analyzes it.
- [ ] Run the campaign and fill the dataset: baseline and relayed rows, with the distance and
      the overhead.
- [ ] Vary the distance and the separation between the relays, and repeat, to find the latency
      threshold where authentication starts to fail (the relay's effectiveness limit).
- [ ] Compute and tabulate the relay overhead (relayed RTT − baseline RTT); put the numbers and
      plots into `marco3-pkes.pdf`.

## Milestone 4 — Mitigation analysis (paper discussion)

- [ ] Characterize empirically how a timing or latency bound would separate baseline from
      relayed rounds (i.e., what cutoff rejects the relay without rejecting the honest key).
- [ ] Write up the distance-bounding and ranging mitigations against the measured data.

## Housekeeping

- [ ] ESP-NOW addressing: the tunnel currently uses broadcast unless `RELAY_PEER_MAC` is set.
      Set both peer MACs (read from the boot logs) to get MAC-level ACKs and real loss counts.
- [ ] Keep the protocol definitions in lockstep: `components/pkes_contract/ble_contract.h` is the
      source of truth; `relays/main/link.h` defines the tunnel frames; `phoneaskey/` must match
      the contract only if it is revived.
- [x] Flash headroom: the BLE relay build fits the single-app large partition (~8 % free).
- [x] Toolchain: the bench builds with ESP-IDF v6.1 (the version installed on the work machine). The
      code was first written for v5.4, and the v5.4 paths stay behind three guards:
      - `relays/main/link.c`: the ESP-NOW send callback changed signature in v5.4, so `on_sent` is
        chosen by `#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)`.
      - `components/pkes_contract/pkes_hmac.c`: on mbedTLS 4 (IDF v6.1) `mbedtls_md_hmac` is a private
        identifier, so the HMAC goes through PSA Crypto (`psa_import_key` + `psa_mac_compute`, after
        `pkes_hmac_init()`); mbedTLS 3 keeps the MD path. Chosen by `MBEDTLS_VERSION_NUMBER >= 0x04000000`.
      - `relays/main/CMakeLists.txt`: the explicit `REQUIRES` list (`bt esp_wifi esp_netif esp_event
        nvs_flash esp_timer pkes_contract cyd serial_ota`); IDF 6 split the drivers into separate
        components, so `main` must list them.
      v5.4 was not rebuilt in checkpoint 09: its paths are kept but not checked since the v6.1 move.
