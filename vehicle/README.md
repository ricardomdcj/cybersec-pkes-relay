# vehicle — BLE verifier (honest baseline)

A BLE central in ESP-IDF (NimBLE) that reproduces the legitimate pairing with the key (project
`key/`) and measures the **challenge-response RTT without a relay**. That number is the baseline the
cost of the relay is compared against in the later milestones.

It runs on a **CYD (ESP32-2432S028R)**. Besides the measurement, the display shows the state
(scanning / connected / measuring), the round, the RTT, OK/FAIL counts, the nonce and response in
hex, the live statistics, and a sparkline of the last 30 rounds, with a car mascot in the corner
(`components/cyd` layer).

## Protocol (mirrors the shared contract)

1. Scan for the service `8f2a0001-…` and connect to the key.
2. Discover the characteristics and enable *notify* on `8f2a0003` (response).
3. Wait for the `START` command (see below).
4. Write a 16-byte nonce to `8f2a0002` (challenge).
5. On the notify, check `HMAC-SHA256(PSK, nonce)` truncated to 16 bytes.
6. Measure the RTT (write → notify), and repeat `ROUNDS` times on a fixed grid.

UUIDs, PSK and format live in `components/pkes_contract/ble_contract.h`; they must stay identical
on the key and the relays.

## Bench commands (UART0)

The same serial port as the console and the OTA. Send them in the monitor or any terminal:

```
RUN label=<label> dist=<meters>     tags the campaign (CSV column and display); optional
START                               runs the rounds; only after "subscribed to notify"
```

`RUN` is ignored while a campaign is running. Without `RUN` the label is `run` and the distance
`-`, with a warning in the log. A disconnection stops the campaign: send `START` again after the
reconnection. `tools/capture_run.py` sends both commands for you.

## Build / flash / monitor

From `vehicle/`, with the ESP-IDF v6.1 environment active:

```
idf.py set-target esp32     # CYD = classic ESP32
idf.py build flash monitor
```

> CYD flashing: esptool's auto-reset does not enter download mode (error `0x13`).
> Hold **BOOT**, tap **RST**, release BOOT, then flash with `--before no-reset` (see CLAUDE.md).
> After the first flash, `tools/serial_ota.py` can reflash without the button.

## Expected monitor output

```
I RUN_META: role=vehicle fw=<commit> build=<date time>
I VEHICLE: scanning for the key service...
I VEHICLE: key found, connecting...
I VEHICLE: connected (handle 1); discovering service
I VEHICLE: subscribed to notify; ready. Send START to begin the campaign
I VEHICLE: campaign started: label=baseline dist=0.5 rounds=50 interval=1000 ms timeout=2000 ms
I VEHICLE: CSVHDR v2 label,dist_m,round,nonce,t_tx_us,rtt_us,result,rssi_dbm,itvl_1_25ms,mtu
I VEHICLE: round 1/50: OK  RTT=NNNN us  RSSI=-41 dBm  nonce=xxxxxxxx
I VEHICLE: CSV,baseline,0.5,1,xxxxxxxx,<t_tx>,NNNN,OK,-41,24,247
...
I VEHICLE: === baseline dist=0.5 n=50 ok=50 fail=0 timeout=0 disconnects=0 | RTT us (answered): min=.. avg=.. max=.. ===
```

`OK` confirms that the key proved possession of the secret and is alive (the nonce changes every
round). A round with no response in time is logged as `TIMEOUT` (`rtt_us = -1`); a response that
arrives after the deadline is discarded. Adjust `ROUNDS`, `INTERVAL_MS` and `ROUND_TIMEOUT_MS` at
the top of `main/main.c`. `BASELINE_BAND_MIN_US` and `BASELINE_BAND_MAX_US` set the reference band on
the sparkline (0 = no band).
