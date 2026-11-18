# components — shared bench code

ESP-IDF components reused by `key/`, `vehicle/` and `relays/`. Each project sees them because its
root `CMakeLists.txt` does:

```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")
```

## `pkes_contract/`

The single source of the bench's GATT protocol (it used to be duplicated by hand in three places):

- `ble_contract.h` — UUIDs of the service and characteristics, `NONCE_LEN` / `RESPONSE_LEN`, and the PSK.
- `pkes_hmac.c/.h` — `HMAC-SHA256(PSK, nonce)` truncated to `RESPONSE_LEN`. It uses **PSA Crypto**
  on mbedTLS 4 (IDF v6.1) and the legacy MD API on mbedTLS 3 (v5.4). Call `pkes_hmac_init()` once in
  `app_main`. Only `key/` (computes) and `vehicle/` (checks) use it; the relays do not.
- `ble_link_params.h` / `ble_link.c` — the explicit BLE connection parameters (the NimBLE defaults,
  made visible) and the helpers that log the `CONN` and `MTU` lines.
- `run_meta.h` / `run_meta.c` — the `RUN_META` line printed at boot: the git commit of the source,
  the `+mod` flag for uncommitted changes, and the build date. The commit is read by CMake at
  configure time, so run `idf.py reconfigure` after a commit.

`REQUIRES bt mbedtls log` (the header uses the NimBLE UUID types).

## `cyd/`

The presentation layer of the CYD (ESP32-2432S028R), ported from the **deskrab** bench:

- `board.h` — pins of the ST7789 display, the XPT2046 touch and the RGB LED.
- `cyd_display.c/.h` — ST7789 over SPI + LVGL 8.3 (DMA buffers, the LVGL task, lock/unlock).
- `cyd_led.c/.h` — RGB LED (off / solid / blink / breathe), mirrors the state.
- `cyd_ui.c/.h` — the common status screen: a colored header with the mode tag and the link
  indicators, the state banner, a progress bar, the label/value card grid, the sparkline and its
  baseline band, the hex lines, an animated **mascot** per role (bob, blink, hop, expressions),
  the result badge, the end-of-campaign summary and histogram, the relay link diagram, the
  distance scene, and the boot splash.
- `idf_component.yml` — pulls in `lvgl/lvgl ~8.3` (component manager).

The bench is **view-only** (no touch control): the deskrab touch code was not ported.
`REQUIRES lvgl__lvgl esp_lcd ...`.

## `serial_ota/`

A minimal firmware update over UART0, so the CYD can be reflashed without the BOOT button. The
running firmware receives `OTA:*` lines and writes the new image to the other OTA slot. The host
side is `tools/serial_ota.py`; the protocol is documented in `serial_ota.h`. The UART0 reader also
delivers the bench commands (`RUN`, `START`, `TXPOW`) through a callback.
