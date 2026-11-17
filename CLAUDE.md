# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Academic research bench (UNIFEI, discipline EC343E) replicating a **PKES relay attack over BLE** for a paper (`marco3-pkes.pdf`). It is a study/proof-of-concept over Bluetooth Low Energy — not tooling against any real vehicle. The thesis: a relay transports the live challenge-response exchange, so it **forges distance, not identity**, which is why a nonce-based HMAC does not defeat it. The research goal is to measure the relay's RTT overhead against an honest baseline. See `SCOPE.md` for the project's authorization, scope, and why the bench is self-contained.

The project is written in **English** (code, comments, logs, documentation) for publication. The original working notes were in Portuguese; see the Conventions section.

## The roles — all CYD (ESP32-2432S028R)

The system models a key, a car, and a man-in-the-middle. **Every role runs on a CYD
(ESP32-2432S028R, classic ESP32 with a 320×240 ST7789 display + XPT2046 touch + RGB LED).**
Each board shows its own live state on the display (view-only, no touch control), using the
shared `components/cyd/` layer ported from the `deskrab` bench (`display`, `led`, a status UI
with a small animated mascot per role). The firmwares:

- **`key/`** — ESP-IDF (NimBLE) BLE **peripheral / GATT server**, the **key fob**. Receives the
  nonce on CHALLENGE, computes `HMAC-SHA256(PSK, nonce)` (shared `pkes_contract` component) and
  notifies it on RESPONSE. This replaces the old Android app as the real key. `main/main.c`,
  mascot = key (amber).
- **`vehicle/`** — ESP-IDF (NimBLE) BLE **central** playing the car. Runs the legitimate
  challenge-response against the key and measures the **honest RTT baseline** — the number every
  relay measurement is compared against. `main/main.c`, mascot = car (blue).
- **`relays/`** — the attack itself: **Relay A** (central, near the key) and **Relay B**
  (peripheral, near the car), one ESP-IDF project, role chosen by `#define`. The two relays
  tunnel the BLE exchange over **ESP-NOW** (`main/link.c`): challenge travels B→A, response
  travels A→B. Mascot = relay/antenna (A purple, B cyan).
- **`phoneaskey/`** — the **legacy** Android app (Kotlin) that first played the key, kept as a
  historical artifact (it is referenced in the paper). Superseded by `key/`; if touched, it must
  still match the protocol contract below.

Shared code lives in repo-level `components/` (pulled in by each project's root `CMakeLists.txt`
via `set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")`):
- **`components/pkes_contract/`** — the single source of truth for the GATT contract
  (`ble_contract.h`), the HMAC helper (`pkes_hmac.c`, PSA on mbedTLS 4 / MD on 3), the BLE
  connection helpers (`ble_link*`) and the run header (`run_meta*`, the `RUN_META` line).
- **`components/cyd/`** — `board.h`, `cyd_display`, `cyd_led`, `cyd_ui` (+ LVGL 8.3 via its
  `idf_component.yml`).
- **`components/serial_ota/`** — the serial OTA receiver shared by the firmwares.

## Protocol contract — a single shared header

The GATT protocol (service/characteristic UUIDs, nonce/response lengths, the pre-shared key, and
the `HMAC-SHA256(PSK, nonce)` truncated to RESPONSE_LEN) lives in **one** place that all three C
firmwares include:

- **`components/pkes_contract/ble_contract.h`** — used by `key/`, `vehicle/` and `relays/`.

The old hand-duplicated copies (`vehicle/main/ble_contract.h`, `relays/main/ble_contract.h`) were
removed. The **only** remaining mirror is the legacy Kotlin `BleContract.kt` in `phoneaskey/`; if
you ever revive that app, keep it matching this header, otherwise there is now a single source of
truth and nothing to hand-sync.

Flow: the car (central) enables notifications on the RESPONSE characteristic, writes a fresh nonce to CHALLENGE, and the key notifies back the HMAC. The nonce changes each round, so naive replay fails — the relay works only by carrying the live exchange.

The **tunnel frames** between the relays (`relays/main/link.h`) are a separate contract:
CHALLENGE `[seq:4][nonce:16]` and RESPONSE `[seq:4][turnaround of A:4][hmac:16]`. The `seq` is the
round number assigned by Relay B. Both relays must run the same build.

## Building the ESP-IDF firmware (`key/`, `vehicle/`, `relays/`)

**All three firmwares target the classic `esp32` (CYD).** ESP-IDF v5.4 is at
`C:\Users\Ricardo\esp\v5.4`; **the `Ricardo.Junior` workspace machine has only v6.1**
(`C:\esp\v6.1`, activated by `. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1`). The
CYD stack is proven on v6.1 (same as `deskrab`). The code builds on both, with these v6/mbedTLS-4 shims:
- `relays/main/link.c` — the ESP-NOW send-callback signature changed in v5.4, selected by
  `ESP_IDF_VERSION`; `relays/main/CMakeLists.txt` carries the v6 `REQUIRES`.
- `components/pkes_contract/pkes_hmac.c` — mbedTLS 4.x (IDF v6.1) makes the legacy one-shot
  `mbedtls_md_hmac` private, so for `MBEDTLS_VERSION_NUMBER >= 0x04000000` the HMAC goes through
  **PSA Crypto** (`pkes_hmac_init()` → `psa_crypto_init`, then `psa_mac_compute`); v5.4 keeps the
  old MD path. Only `key/` (computes) and `vehicle/` (verifies) use it; the relays are dumb pipes.

First build of any project downloads LVGL into `managed_components/` (component manager, from
`components/cyd/idf_component.yml`). From a subproject directory:

```
idf.py set-target esp32
idf.py build flash monitor
```

**Stale `sdkconfig` trap:** the checked-in `sdkconfig.defaults` say `esp32`, but a local
`sdkconfig` generated earlier can still say `esp32s3` or `esp32c3` (this happened to `vehicle/`
and `relays/`). Check `CONFIG_IDF_TARGET` in `sdkconfig` before flashing. If it is wrong, run
`idf.py set-target esp32` in that folder.

**Relay role** is chosen at build time (both roles are the **same** `esp32` CYD). Edit `RELAY_ROLE` in `relays/main/relay_config.h` or pass it:

| Role | `RELAY_ROLE` | Chip | set-target |
|------|--------------|------|------------|
| Relay A (central, near key) | `RELAY_ROLE_A` (1) | ESP32 (CYD) | `esp32` |
| Relay B (peripheral, near car) | `RELAY_ROLE_B` (2) | ESP32 (CYD) | `esp32` |

e.g. `idf.py -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=0 build`. Since both roles share a chip, the two
boards are distinguished only by which role you flash (and `relay_config.h`'s default is Role A).

**Flashing caveat (CYD auto-reset):** in this automated environment, esptool's DTR/RTS
auto-reset does **not** put these CYD boards into download mode — every attempt (both esptool
4.9 and 5.6, clean port, bridge stopped) fails with `Wrong boot mode detected (0x13)`. The reset
(EN) fires but IO0 is never pulled low. (A normal interactive `idf.py flash` reportedly works on
the same board, so this is specific to the automated context.) So esptool flashing here needs
download mode set by hand: hold **BOOT**, tap **RST**, release BOOT (chip latches in download
mode), then flash with `--before no-reset`. If a `deskrab` status line is active it respawns
`deskrab_bridge.py`, which holds the COM port; kill it in the same command that flashes.

**Buttonless reflashing — serial OTA (`components/serial_ota` + `tools/serial_ota.py`):** to
avoid the BOOT button on every change, all three firmwares embed a tiny UART0 OTA receiver
(shared `serial_ota` component). The running firmware listens for `OTA:*` lines on the console
UART and writes the new image to the other OTA app slot, then reboots into it — no reset, no
download mode. Host side:

```
python tools/serial_ota.py COM6 vehicle/build/vehicle.bin
```

This needs the two-OTA partition layout (`CONFIG_PARTITION_TABLE_TWO_OTA_LARGE`, already in each
`sdkconfig.defaults`). The **first** flash of each board still goes through esptool+BOOT once
(bootstrap, to get an OTA-capable firmware on it); every flash after that is serial OTA. Stop any
process holding the COM port (the `deskrab` bridge) before running the script.

**sdkconfig caveat:** a checked-in `sdkconfig` can predate `sdkconfig.defaults` (e.g. generated
without Bluetooth/LVGL). If settings don't take effect, delete `sdkconfig` and reconfigure:

```
del sdkconfig
idf.py set-target esp32
idf.py fullclean      # when switching target on an already-built tree
idf.py build
```

**Bench commands and the campaign (UART0):** the vehicle takes `RUN label=<label> dist=<meters>`
(tags the CSV and the display) and `START` (runs the rounds; only after the key is subscribed).
A disconnection stops the campaign, so send `START` again after reconnecting. The relays take
`TXPOW <n>`. The CSV format and the procedure are in `TESTING.md`.

## Relay bring-up is staged via `RELAY_LINK_SELFTEST`

`relays/main/relay_config.h` has `RELAY_LINK_SELFTEST` (default `1`):

- **`1` (Milestone 1):** A and B exchange ESP-NOW frames with **no BLE** — B originates a challenge every ~1 s and measures the pure tunnel RTT. This RTT is already a data point for the paper.
- **`0` (Milestone 2):** both relays' BLE halves are implemented; the full chain (key → A → B → vehicle) runs the real challenge-response, compared against `vehicle/`'s baseline.

Both modes drive the display (state banner, counters, tunnel RTT in self-test).

Note WiFi/ESP-NOW coexists with BLE on the same 2.4 GHz radio; that coexistence cost is intentional — it is part of what gets measured.

## Android app (legacy)

`phoneaskey/` is the original key-fob app, **superseded by `key/`** and kept only as a historical
artifact. Standard Gradle wrapper from `phoneaskey/`:

```
gradlew.bat assembleDebug        # build
gradlew.bat installDebug         # build + install on a connected device
gradlew.bat testDebugUnitTest    # unit tests
```

Package `com.tripper.phone_as_key`. The app only needs the `ble/` package to act as the key; the rest is scaffold UI/theme.

## Screen simulator (tools/sim)

Renders the device's own `components/cyd/cyd_ui.c` (LVGL 8.3) on the PC, so screens can be checked
without a board. Only the display, LED and timers are replaced (`sim_hal.c`); the UI code is the one
on the CYD. Full details and the script commands are in `tools/sim/README.md`.

- Build once `vehicle/` has been built with `idf.py build` (it provides LVGL and the LVGL options),
  then `python tools/sim/build_gcc.py` (msys64 gcc: this work machine has no MSVC). Run every
  scenario with `python tools/sim/run_scenarios.py`; frames go to `tools/sim/out/` as BMP and PNG.
- It drives the UI API with the values from a script; it does **not** run the BLE, ESP-NOW or round
  logic, and the numbers in the scenarios are samples. Its LVGL pool is doubled, so memory use there
  says nothing about the device.
- When a card or a label changes in a firmware, update the matching `CFG_*` in `tools/sim/sim_main.c`.

## Conventions

- **Everything is in English**: code, comments, log messages, on-screen text, READMEs, `TESTING.md`,
  `TASKS.md`, `SCOPE.md`, commit messages and this file. The project is meant for publication.
  The files are kept in ASCII where practical: the on-screen font has no accented characters.
- **Protocol tokens are fixed** and must not be translated: `OTA:*` lines, `CSV`, `CSVA`,
  `CSVB`, `CSVHDR`, `RUN`, `START`, `TXPOW`, `RUN_META`, the `=== ... ===` summary, and the
  `RELAY_A:`/`RELAY_B:`/`VEHICLE:`/`KEY` log tags. The tools parse them. `tools/analyze_runs.py`
  and `tools/capture_run.py` also accept the older Portuguese tags (`VEICULO`, `FALHA`,
  `papel`) so that the early captures can still be read.
- On-screen labels stay ASCII (the built-in Montserrat fonts have no accented letters).
- Commit messages are in English and end with the `Co-Authored-By` line the workflow asks for.
- Build output (`build/`, `.cxx/`, `managed_components/`), `local.properties`, and regenerated `sdkconfig` are gitignored; keep them out of commits.
- Progress is kept in `IMPROVEMENTS.md` (tasks and checkpoints) and `TASKS.md` (milestones).
