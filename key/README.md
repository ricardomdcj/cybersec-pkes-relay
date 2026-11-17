# key — the digital key (BLE peripheral on a CYD)

A port of the old Android app `phoneaskey/` to a **CYD (ESP32-2432S028R)**. A BLE peripheral and
GATT server (NimBLE) that advertises the key service, receives the nonce on CHALLENGE, computes
`HMAC-SHA256(PSK, nonce)` truncated to `RESPONSE_LEN`, and notifies on RESPONSE.

The contract (UUIDs, sizes, PSK) and the HMAC come from the shared component
`components/pkes_contract` — the same source used by `vehicle/`.

## Display

It shows the state (advertising / connected), how many challenges were answered, the compute +
notify time, and the last nonce and response in hex, with a **key mascot** (amber) in the corner.
It uses the `components/cyd` layer (ST7789 + LVGL + RGB LED). View-only, no touch.

## Build / flash / monitor

```
idf.py set-target esp32     # CYD = classic ESP32
idf.py build flash monitor
```

> CYD flashing: esptool's auto-reset does not enter download mode (error `0x13`).
> Hold **BOOT**, tap **RST**, release BOOT, then flash with `--before no-reset` (see CLAUDE.md).

## Expected monitor output

```
I RUN_META: role=key fw=<commit> build=<date time>
I cyd_display: ST7789 + LVGL ready
I KEY: key started (BLE peripheral + CYD display)
I KEY: advertising the key; waiting for the vehicle
I KEY: vehicle connected (handle 0)
I KEY: HOP K resp nonce=xxxxxxxx calc_notify=NNNN us (challenge 1, 16 B) t=... us
```

When the vehicle or a relay drops, the key logs the reason and advertises again:
`vehicle disconnected (reason=..., disconnects=N); re-advertising`.
