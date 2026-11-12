# Tasks / Next Steps

Milestone 1 (ESP-NOW tunnel + RTT self-test) is done. The remaining work is: bring
BLE up on both relays, then run the measurement campaign the paper needs. Grouped by
milestone; order within a group is roughly the order to tackle it.

## Milestone 2 — BLE on both relay ends

Goal: replace the self-test echo with the real phone↔vehicle exchange carried over the
tunnel, then set `RELAY_LINK_SELFTEST 0` in `relays/main/relay_config.h`.

### Relay A — BLE central (near the phone/key) — `relays/main/relay_a.c`
- [x] Port the central logic from `vehicle/main/main.c`: scan for `SERVICE_UUID`,
      connect to the key, discover services/characteristics.
- [x] Cache the CHALLENGE and RESPONSE characteristic handles; enable notify (CCCD) on RESPONSE.
- [x] On `LINK_MSG_CHALLENGE` from the tunnel, write the nonce to the CHALLENGE
      characteristic (`ble_gattc_write_flat`, with response, matching the vehicle). The GATT
      write runs on a worker task, not in the ESP-NOW (WiFi) callback.
- [x] On `BLE_GAP_EVENT_NOTIFY_RX` from the key, forward the HMAC back as
      `LINK_MSG_RESPONSE` via `forward_response()`.
- [x] Handle reconnect / scan-restart if the key drops.

### Relay B — BLE peripheral (near the vehicle) — `relays/main/relay_b.c`
- [x] Mirror `phoneaskey/.../ble/KeyPeripheral.kt` in NimBLE: advertise a cloned GATT
      service with the same `SERVICE_UUID` / CHALLENGE / RESPONSE characteristics
      (NimBLE auto-adds the CCCD for the notify characteristic).
- [x] On the vehicle's write to CHALLENGE, send the nonce as `LINK_MSG_CHALLENGE` over
      the tunnel.
- [x] On `LINK_MSG_RESPONSE` from Relay A, notify the RESPONSE characteristic to the
      vehicle (on a worker task, not in the ESP-NOW callback).
- [x] Keep the self-test path (`#if RELAY_LINK_SELFTEST`) compiling so the tunnel
      baseline can still be reproduced (BLE compiled only when `RELAY_LINK_SELFTEST 0`).

### Integration — needs the two boards + phone + vehicle (not yet run on hardware)
- [ ] Build/flash each role (`-DRELAY_ROLE=1` S3, `-DRELAY_ROLE=2` C3) with
      `-DRELAY_LINK_SELFTEST=0`, then run the full chain: phone → Relay A → Relay B → vehicle.
- [ ] Confirm the vehicle logs `OK` rounds (the relayed HMAC verifies), proving the
      relay forwards a live exchange end to end.

> Code compiles clean on ESP-IDF v6.1 (Role A / esp32s3 build verified; that build compiles
> both relay files). Only the on-hardware run remains.

## Milestone 3 — Measurement campaign (feeds the paper)

- [ ] Collect the honest baseline with `vehicle/` directly against the phone
      (`RTT` distribution over `ROUNDS`); record min/avg/max.
- [ ] Collect relayed `RTT` over the full chain under the same `ROUNDS`/`INTERVAL_MS`.
- [x] Storage format decided and emitted by the vehicle: one `CSV,label,rodada,rtt_us,resultado,rssi_dbm`
      row per round (plus a `CSVHDR` header), with a build-time run label (`-DRUN_LABEL=baseline|relay`)
      and the connection RSSI. Grep `CSV,` from a monitor capture. (Distance is added by hand — the
      firmware can't know the physical separation.)
- [ ] Run the campaign and fill the dataset: capture baseline and relayed rows, add the distance
      column, compute overhead.
- [ ] Vary distance / separation between the relays and repeat, to find the latency
      threshold where authentication starts to fail (the relay's effectiveness limit).
- [ ] Compute and tabulate relay overhead = relayed RTT − baseline RTT; put the numbers
      and plots into `marco3-pkes.pdf`.

## Milestone 4 — Mitigation analysis (paper discussion)

- [ ] Characterize empirically how a timing/latency bound would separate baseline from
      relayed rounds (i.e., what cutoff rejects the relay without rejecting the honest key).
- [ ] Write up distance-bounding / ranging mitigations against the measured data.

## Housekeeping / smaller items

- [ ] ESP-NOW addressing: `link.c` currently uses **broadcast** (`ff:ff:...`) on a fixed
      channel (`LINK_CHANNEL 1`), so there is no per-board peer MAC to set — just make sure
      both boards are on the same channel. Optional: switch to unicast peer MACs for cleaner
      two-board isolation (keeps stray ESP-NOW traffic out of the measurement).
- [ ] When the final "vehicle" endpoint moves to a classic ESP32, `idf.py set-target esp32`
      and re-verify (NimBLE logic is unchanged).
- [ ] Keep the three `ble_contract` sources in lockstep (`BleContract.kt`,
      `vehicle/main/ble_contract.h`, `relays/main/ble_contract.h`) on any protocol change.
- [x] Regenerate `relays/sdkconfig` from `sdkconfig.defaults` per board (`sdkconfig.defaults`
      now also enables `CONFIG_ESP_COEX_SW_COEXIST_ENABLE`; `idf.py set-target` regenerates).
- [x] Flash headroom: the BLE build is >1 MB and overflowed the default 1 MB app partition
      (the ESP32-C3 / Relay B by a few KB). `sdkconfig.defaults` now sets
      `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y` (~1.5 MB app, fits 2 MB flash; ~31% free).
- [ ] Toolchain: code was written for ESP-IDF v5.4 but also builds on v6.1 (`link.c` guards the
      ESP-NOW send-cb signature by `ESP_IDF_VERSION`; `main/CMakeLists.txt` lists the v6 `REQUIRES`).
