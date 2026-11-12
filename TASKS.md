# Tasks / Next Steps

Milestone 1 (ESP-NOW tunnel + RTT self-test) is done. The remaining work is: bring
BLE up on both relays, then run the measurement campaign the paper needs. Grouped by
milestone; order within a group is roughly the order to tackle it.

## Milestone 2 — BLE on both relay ends

Goal: replace the self-test echo with the real phone↔vehicle exchange carried over the
tunnel, then set `RELAY_LINK_SELFTEST 0` in `relays/main/relay_config.h`.

### Relay A — BLE central (near the phone/key) — `relays/main/relay_a.c`
- [ ] Port the central logic from `vehicle/main/main.c`: scan for `SERVICE_UUID`,
      connect to the key, discover services/characteristics.
- [ ] Cache the CHALLENGE and RESPONSE characteristic handles; enable notify (CCCD) on RESPONSE.
- [ ] On `LINK_MSG_CHALLENGE` from the tunnel, write the nonce to the CHALLENGE
      characteristic (`ble_gattc_write_flat`) — this replaces the `TODO(BLE)` at `relay_a.c:34`.
- [ ] On `BLE_GAP_EVENT_NOTIFY_RX` from the key, forward the HMAC back as
      `LINK_MSG_RESPONSE` via `forward_response()` (`relay_a.c:45`).
- [ ] Handle reconnect / scan-restart if the key drops.

### Relay B — BLE peripheral (near the vehicle) — `relays/main/relay_b.c`
- [ ] Mirror `phoneaskey/.../ble/KeyPeripheral.kt` in NimBLE: advertise a cloned GATT
      service with the same `SERVICE_UUID` / CHALLENGE / RESPONSE characteristics.
- [ ] On the vehicle's write to CHALLENGE, send the nonce as `LINK_MSG_CHALLENGE` over
      the tunnel (`relay_b.c:64`).
- [ ] On `LINK_MSG_RESPONSE` from Relay A, notify the RESPONSE characteristic to the
      vehicle — replaces the `TODO(BLE)` at `relay_b.c:54`.
- [ ] Keep the self-test path (`#if RELAY_LINK_SELFTEST`) compiling so the tunnel
      baseline can still be reproduced.

### Integration
- [ ] Set `RELAY_LINK_SELFTEST 0` and run the full chain: phone → Relay A → Relay B → vehicle.
- [ ] Confirm the vehicle logs `OK` rounds (the relayed HMAC verifies), proving the
      relay forwards a live exchange end to end.

## Milestone 3 — Measurement campaign (feeds the paper)

- [ ] Collect the honest baseline with `vehicle/` directly against the phone
      (`RTT` distribution over `ROUNDS`); record min/avg/max.
- [ ] Collect relayed `RTT` over the full chain under the same `ROUNDS`/`INTERVAL_MS`.
- [ ] Record the dataset the abstract promises: latency, distance, RSSI, and auth
      decision per round. Decide and document a storage format (CSV from the monitor logs).
- [ ] Vary distance / separation between the relays and repeat, to find the latency
      threshold where authentication starts to fail (the relay's effectiveness limit).
- [ ] Compute and tabulate relay overhead = relayed RTT − baseline RTT; put the numbers
      and plots into `marco3-pkes.pdf`.

## Milestone 4 — Mitigation analysis (paper discussion)

- [ ] Characterize empirically how a timing/latency bound would separate baseline from
      relayed rounds (i.e., what cutoff rejects the relay without rejecting the honest key).
- [ ] Write up distance-bounding / ranging mitigations against the measured data.

## Housekeeping / smaller items

- [ ] Verify the ESP-NOW peer MAC and fixed channel in `link.c` match the two actual
      boards before the first two-board run.
- [ ] When the final "vehicle" endpoint moves to a classic ESP32, `idf.py set-target esp32`
      and re-verify (NimBLE logic is unchanged).
- [ ] Keep the three `ble_contract` sources in lockstep (`BleContract.kt`,
      `vehicle/main/ble_contract.h`, `relays/main/ble_contract.h`) on any protocol change.
- [ ] Regenerate `relays/sdkconfig` from `sdkconfig.defaults` per board (BT was absent in
      the stale local copy) — see CLAUDE.md.
