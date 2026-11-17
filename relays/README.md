# relays — Relay A and Relay B

Two BLE relays linked by ESP-NOW. One ESP-IDF project; the role is selected by a `#define`.

```
Key ─BLE─► Relay A (central) ─ESP-NOW─► Relay B (peripheral) ─BLE─► Vehicle
       HMAC ◄──────────────────── nonce ◄──────────────── nonce ◄──────────── nonce
```

- **Relay A** (`relay_a.c`): central, near the key. Receives the challenge over ESP-NOW, writes it
  to the key, and returns the HMAC.
- **Relay B** (`relay_b.c`): peripheral, near the vehicle. Clones the key's service; forwards the
  vehicle's challenge and notifies the response.
- **Link** (`link.c`, `link.h`): ESP-NOW on a fixed channel, with the tunnel frame format
  (`[seq][nonce]` and `[seq][turnaround A][HMAC]`). **Working.**
- **Contract** (`components/pkes_contract/ble_contract.h`): shared with the key and the vehicle.

## Status

- [x] Project layout and role selection (`relay_config.h`).
- [x] ESP-NOW link A↔B, plus the self-test that measures the tunnel RTT.
- [x] Relay A BLE central (`relay_a.c`): scan, connect, discover and notify, ported from
      `vehicle/main/main.c`, without generating a nonce or checking the HMAC (a dumb pipe).
- [x] Relay B BLE peripheral (`relay_b.c`): NimBLE GATT server mirroring the key (cloned service,
      CHALLENGE write, RESPONSE notify).
- [x] Round validity (M-01 to M-05): sequence numbers, late-response discard, turnaround in the
      response frame.
- [ ] Full-chain run on hardware (key → A → B → vehicle). See Milestone 2 in `TASKS.md`.

## Milestone 1: validate the tunnel (no BLE)

With `RELAY_LINK_SELFTEST 1` (the default in `relay_config.h`), Relay B fires one challenge per
second and measures the round trip over ESP-NOW.

If an old `sdkconfig` exists (generated without BT), delete it to pick up the new
`sdkconfig.defaults`. From the `relays/` folder (role and mode can go on the command line with `-D`,
without editing the headers):

```
del sdkconfig

REM --- Relay A (CYD, central role) ---
idf.py set-target esp32
idf.py -DRELAY_ROLE=1 build flash monitor

REM --- Relay B (CYD, peripheral role) ---
idf.py set-target esp32
idf.py fullclean
idf.py -DRELAY_ROLE=2 build flash monitor
```

With A and B powered, the monitor of B shows:

```
I RELAY_B: Relay B started (tunnel self-test; originating challenges every 1 s)
I RELAY_B: [selftest] response received (16 B); tunnel RTT=NNNN us
```

That `tunnel RTT` is the ESP-NOW link overhead, one of the data points for the paper.

Each relay also prints its MAC at boot (`this board MAC: ...`). Copy the MAC of the other relay
into `RELAY_PEER_MAC` in `relay_config.h` to switch the tunnel to unicast (MAC-level ACKs, real
loss counts). While it stays zeroed, the tunnel uses broadcast.

## Milestone 2: BLE on both ends

The BLE halves are implemented. With `RELAY_LINK_SELFTEST 0`, the full chain
(key → A → B → vehicle) runs the real challenge-response, and its RTT is compared with the
vehicle's honest baseline.

- **Relay A** (`relay_a.c`): central. Scans for `SVC_UUID`, connects to the key, discovers
  CHALLENGE/RESPONSE, enables notify. A tunnel challenge is written to CHALLENGE (with a response,
  as the vehicle does); a notify from the key is sent back through the tunnel. It neither
  generates nonces nor checks the HMAC.
- **Relay B** (`relay_b.c`): peripheral. A NimBLE GATT server advertising the cloned `SVC_UUID`.
  The vehicle's write to CHALLENGE is sent through the tunnel; the tunnel response is notified on
  RESPONSE. Responses that do not match the pending round (`seq`) are dropped and counted as late.

On both relays, a frame arriving from the tunnel (ESP-NOW callback, running in the WiFi task) is
queued, and the GATT call runs in a worker task. The cost of that hop is part of the measured
overhead.

### How to run (one binary per role)

```
del sdkconfig

REM --- Relay A (CYD, central role) ---
idf.py set-target esp32
idf.py -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=0 build flash monitor

REM --- Relay B (CYD, peripheral role) ---
idf.py set-target esp32
idf.py fullclean
idf.py -DRELAY_ROLE=2 -DRELAY_LINK_SELFTEST=0 build flash monitor
```

With the chain running, the vehicle's monitor should again log `OK` rounds (the relayed HMAC
verifies) and a summary line whose RTT now includes the relay hop. That number is what gets
compared with the honest baseline. The relays log:

```
I RELAY_B: vehicle challenge (16 B) -> tunnel
I RELAY_A: HMAC from key (16 B) -> tunnel
I RELAY_B: response (16 B) notified to the vehicle
```

At startup there is a harmless race: if the vehicle writes a challenge before Relay A has
connected to the key, Relay A drops that challenge (`... the key is not ready; dropping`) and the
vehicle loses that round. The 50 rounds leave room until the chain is up.

## Toolchain

The project was written for ESP-IDF v5.4 but **also builds on v6.1** (the current development
machine). The ESP-NOW send-callback signature changed in v5.4, so `link.c` selects the right form
by `ESP_IDF_VERSION`. On v6.1, `main/CMakeLists.txt` needs
`REQUIRES bt esp_wifi esp_netif esp_event nvs_flash esp_timer`.

> Flash space: in BLE mode the binary exceeds 1 MB (BT + WiFi + NimBLE together) and overflows the
> default app partition. That is why `sdkconfig.defaults` already uses
> `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y` (about 1.5 MB of app, fits in 2 MB of flash), which
> leaves about 8 % free in the current BLE build. If you change the defaults, delete `sdkconfig`
> to re-apply them.
