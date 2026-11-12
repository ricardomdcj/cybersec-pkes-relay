# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Academic research bench (UNIFEI, discipline EC343E) replicating a **PKES relay attack over BLE** for a paper (`marco3-pkes.pdf`). It is a study/proof-of-concept over Bluetooth Low Energy — not tooling against any real vehicle. The thesis: a relay transports the live challenge-response exchange, so it **forges distance, not identity**, which is why a nonce-based HMAC does not defeat it. The research goal is to measure the relay's RTT overhead against an honest baseline.

## The three roles

The system models a key, a car, and a man-in-the-middle, split across three subprojects:

- **`phoneaskey/`** — Android app (Kotlin, Jetpack Compose, Gradle) playing the **key fob**: a BLE peripheral / GATT server. Core logic is in `app/src/main/java/com/tripper/phone_as_key/ble/` (`BleContract.kt`, `KeyPeripheral.kt`).
- **`vehicle/`** — ESP-IDF (NimBLE) BLE **central** playing the car. Runs the legitimate challenge-response directly against the phone and measures the **honest RTT baseline** — the number every relay measurement is compared against. Single source file: `main/main.c`.
- **`relays/`** — the attack itself: **Relay A** (central, near the phone) and **Relay B** (peripheral, near the car), one ESP-IDF project, role chosen by `#define`. The two relays tunnel the BLE exchange to each other over **ESP-NOW** (`main/link.c`): challenge travels B→A, response travels A→B.

## Protocol contract — keep three files identical

The GATT protocol (service/characteristic UUIDs, nonce/response lengths, the pre-shared key, and the `HMAC-SHA256(PSK, nonce)` truncated to RESPONSE_LEN) is **duplicated by hand** across:

- `phoneaskey/app/src/main/java/com/tripper/phone_as_key/ble/BleContract.kt`
- `vehicle/main/ble_contract.h`
- `relays/main/ble_contract.h`

Any change to one **must** be mirrored in the other two or the bench stops interoperating. There is no shared source of truth; treat these three as one unit.

Flow: the car (central) enables notifications on the RESPONSE characteristic, writes a fresh nonce to CHALLENGE, and the key notifies back the HMAC. The nonce changes each round, so naive replay fails — the relay works only by carrying the live exchange.

## Building the ESP-IDF firmware (`vehicle/`, `relays/`)

ESP-IDF v5.4 is installed at `C:\Users\Ricardo\esp\v5.4`. From a subproject directory:

```
idf.py set-target <chip>      # see role/chip mapping below
idf.py build flash monitor
```

**Relay role & chip** are coupled. Edit `RELAY_ROLE` in `relays/main/relay_config.h` (or pass `-DRELAY_ROLE=1|2`) and match the target:

| Role | `RELAY_ROLE` | Chip | set-target |
|------|--------------|------|------------|
| Relay A (central, near phone) | `RELAY_ROLE_A` (1) | ESP32-S3 | `esp32s3` |
| Relay B (peripheral, near car) | `RELAY_ROLE_B` (2) | ESP32-C3 | `esp32c3` |

`vehicle/` currently targets `esp32s3` for testing; the final car endpoint will be a classic `esp32` (same NimBLE logic, just re-set-target).

**sdkconfig caveat:** a checked-in `sdkconfig` can predate the `sdkconfig.defaults` (e.g. generated without Bluetooth). If BT/NimBLE settings don't take effect, delete `sdkconfig` and reconfigure:

```
del sdkconfig
idf.py set-target <chip>
idf.py fullclean      # when switching target on an already-built tree
idf.py build
```

## Relay bring-up is staged via `RELAY_LINK_SELFTEST`

`relays/main/relay_config.h` has `RELAY_LINK_SELFTEST` (default `1`):

- **`1` (Milestone 1):** A and B exchange ESP-NOW frames with **no BLE** — B originates a challenge every ~1 s and measures the pure tunnel RTT. The BLE central/peripheral halves are `TODO(BLE)`. This RTT is already a data point for the paper.
- **`0` (Milestone 2):** fill in each relay's BLE half, then the full chain (phone → A → B → vehicle) runs the real challenge-response, compared against `vehicle/`'s baseline.

Note WiFi/ESP-NOW coexists with BLE on the same 2.4 GHz radio; that coexistence cost is intentional — it is part of what gets measured.

## Android app

Standard Gradle wrapper from `phoneaskey/`:

```
gradlew.bat assembleDebug        # build
gradlew.bat installDebug         # build + install on a connected device
gradlew.bat testDebugUnitTest    # unit tests
```

Package `com.tripper.phone_as_key`. The app only needs the `ble/` package to act as the key; the rest is scaffold UI/theme.

## Conventions

- READMEs and code comments are in **Portuguese**; follow suit when editing them.
- `relays/README.md` and `vehicle/README.md` carry the milestone plan and expected monitor output — read them before changing firmware behavior.
- Build output (`build/`, `.gradle/`, `.cxx/`), `local.properties`, and regenerated `sdkconfig` are gitignored; keep them out of commits.
