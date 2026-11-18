# PKES bench improvements: measurement and visuals

Task list derived from two sources: the analysis of the 2026-11-16 logs (`data/`) and of the
code in `vehicle/`, `relays/` and `key/`; and the screen ideas inspired by `deskrab`.

Each item is an independent task. When a task was too large, it was split into two (`a` and
`b`). None of this has been flashed to a board yet. Each item is validated by `idf.py build`, by
reading the serial monitor, or by the screen simulator (V-17).

Conventions: each item lists **Files**, **Acceptance** and **Depends on**. The project is in
English end to end (checkpoint 06), so on-screen text is ASCII English (see V-16).

---

## Part 1: Measurement

### A. Round validity (do this first)

> **Status (2026-11-17):** implemented and compiled, not yet validated on hardware.
> Build: the first build used the wrong target (stale local sdkconfig: vehicle esp32s3, relays
> esp32c3). Rebuilt for `esp32` on 2026-11-17: all pass (vehicle, relays BLE A and B, self-test A
> and B).
> (an older warning: `s_from_vehicle` unused in the self-test, already there before.)
> **Both relays must be reflashed together:** the tunnel format changed (see `relays/main/link.h`),
> and an old relay cannot talk to a new one.
>
> Decisions made during implementation:
> - **M-02:** on `TIMEOUT` the `rtt_us` is sent as `-1`. On the display the failure card became
>   `Fail / tmo`, with the value `failures / timeouts`.
> - **M-03:** `seq` is a counter that Relay B assigns to each challenge (not the 4 bytes of the
>   nonce). B->A frame: `[seq:4][nonce:16]`. A->B frame: `[seq:4][turnaround A:4][hmac:16]`.
> - **M-04:** a discarded response appears in the log with the counter `late`. It has no display
>   field yet (that comes with V-04/V-06).
> - **M-05:** it came in the same frame as M-03. The `CSVB` line gained a last column with A's
>   turnaround.
> - **To verify on hardware:** on timeout, the vehicle calls `ble_gap_conn_rssi` and
>   `ble_gattc_write_flat` from the `esp_timer` task. It is the same pattern the round timer already
>   used, but it needs confirming with NimBLE running.

- [x] **M-01. Per-round timeout on the vehicle.** Today the next challenge only goes out inside the
  notify handler. If the response is lost, the campaign stalls forever. Add a wait timer (constant
  `ROUND_TIMEOUT_MS`, suggested 2 s) that marks the round as lost and moves on.
  **Files:** `vehicle/main/main.c`. **Acceptance:** with the relay switched off in the middle of a
  campaign, the vehicle records lost rounds and carries on without restarting. **Depends on:** nothing.

- [x] **M-02. TIMEOUT result, separate from FAIL.** Today `FAIL` only means a wrong HMAC. Add
  `TIMEOUT` (no response in time) as its own result, with its own counter in the CSV and in the `===`
  line. **Files:** `vehicle/main/main.c`. **Acceptance:** a lost round shows up as `TIMEOUT` in the CSV
  and is counted separately in the summary. **Depends on:** M-01.

- [x] **M-03. Round ID in the tunnel response frame.** `LINK_MSG_RESPONSE` carries only the 16-byte
  HMAC. Without an ID, the response is matched to its challenge by arrival order. Add 4 bytes of the
  nonce (or a counter) to the frame. **Files:** `relays/main/link.h`, `relays/main/relay_a.c`,
  `relays/main/relay_b.c`. **Acceptance:** every response frame carries the ID of the challenge that
  produced it. **Depends on:** nothing.

- [x] **M-04. Relay B discards late responses.** With M-03, B compares the received ID with the pending
  challenge. If they differ, it discards the response and counts it under "late". Without this, a
  response that arrives after the timeout lands in the next round and becomes a false `FAIL`.
  **Files:** `relays/main/relay_b.c`. **Acceptance:** with a forced delay on the tunnel, the "late"
  counter goes up and the vehicle does not record a `FAIL`. **Depends on:** M-03.

- [x] **M-05. Relay A returns its own turnaround.** Along with the response (M-03), A sends the
  `turnaround` it already measures (the `CSVA` line). That way B can break the time down without
  depending on logs. **Files:** `relays/main/relay_a.c`, `relays/main/link.h`. **Acceptance:** B
  receives and records A's turnaround in the same round. **Depends on:** M-03.

### B. ESP-NOW tunnel

> **Status (2026-11-17):** implemented and compiled for `esp32`; to be validated on hardware.
> M-06a: each relay's MAC is printed in the boot log (`this board MAC`). M-06b: while
> `RELAY_PEER_MAC` (in `relay_config.h`) stays zeroed, the link uses broadcast, which has no ACK and
> shows no losses. M-07: the last column of `CSVA` and `CSVB` is the cumulative count of send
> failures. M-08: the log shows `power save WiFi = NONE`.

- [x] **M-06a. Read and show each board's MAC.** Print the MAC at boot (`esp_wifi_get_mac`) and show it
  on the display. **Files:** `relays/main/link.c`, `relays/main/main.c`. **Acceptance:** A's and B's
  MACs appear in the boot log. **Depends on:** nothing.

- [x] **M-06b. Unicast peers.** Replace `BCAST` with a fixed peer MAC, set in `relay_config.h`. The
  send then gets MAC-layer ACKs. **Files:** `relays/main/link.c`, `relays/main/relay_config.h`.
  **Acceptance:** frames go only to the peer, with no third-party traffic on the channel.
  **Depends on:** M-06a.

- [x] **M-07. Count tunnel send failures.** With broadcast, the send callback does not usefully report
  loss. With unicast (M-06b), the status becomes real information. Count the failures and expose them
  in the CSV and in the summary. **Files:** `relays/main/link.c`, `relays/main/relay_a.c`,
  `relays/main/relay_b.c`. **Acceptance:** a forced send failure (peer switched off) raises the counter.
  **Depends on:** M-06b.

- [x] **M-08. Turn off Wi-Fi power save.** Set `esp_wifi_set_ps(WIFI_PS_NONE)` in `link_init` and log the
  active mode. The default can add latency. Measure the tunnel RTT before and after. **Files:**
  `relays/main/link.c`. **Acceptance:** the log shows PS=NONE. **Depends on:** nothing.

### C. BLE connection

> **Status (2026-11-17):** implemented and compiled for `esp32`; to be validated on hardware.
> The parameters in `components/pkes_contract/ble_link_params.h` are the **NimBLE defaults**
> (30-50 ms, latency 0, supervision 2.56 s). This step does not change behaviour: it only makes the
> values explicit and logged. Changing these values affects the baseline and the relays at the same
> time. `CONN` and `MTU` lines in the log; the interval goes into CSV v2. M-12 counts disconnections
> (each one causes a reconnection) and shows up in the `===` summary.

- [x] **M-09a. Header with the connection parameters.** Create `components/pkes_contract/ble_link_params.h`
  with the minimum and maximum interval, latency and supervision timeout. **Files:** the new header.
  **Depends on:** nothing.

- [x] **M-09b. Apply the parameters at connect.** Pass the parameters to `ble_gap_connect` (today it is
  `NULL`, so the NimBLE default applies, which we do not know). **Files:** `vehicle/main/main.c`,
  `relays/main/relay_a.c`. **Depends on:** M-09a.

- [x] **M-10. Log the negotiated parameters.** At connect and on every `BLE_GAP_EVENT_CONN_UPDATE`, print
  the effective interval, latency and timeout on a `CONN,...` line. **Files:** `vehicle/main/main.c`,
  `relays/main/relay_a.c`. **Acceptance:** a `CONN` line appears on every connection. **Depends on:** M-09b.

- [x] **M-11. Log MTU and PHY.** Negotiate the MTU and record the MTU and PHY on the `CONN` line.
  **Files:** `vehicle/main/main.c`, `relays/main/relay_a.c`, `relays/main/relay_b.c`. **Depends on:** M-10.

- [x] **M-12. Disconnect reason and reconnection counter.** The log already shows `reason=520` at the
  start (probably a connection timeout, HCI 0x08). Record the reason and count the reconnections in the
  CSV or in the summary. **Files:** `vehicle/main/main.c`, `relays/main/relay_a.c`,
  `relays/main/relay_b.c`, `key/main/main.c`. **Depends on:** nothing.

- [ ] **M-19. Read the connection interval before the first campaign.** The vehicle logs the negotiated
  interval in its `CONN` line (`components/pkes_contract/ble_link.c`). If `itvl` divides 1000 ms (40 or 50 ms,
  for example), the fixed 1 s round grid (M-16) can keep rounds at one connection phase, and the samples are
  correlated. Then add a random start offset to the grid, as a checkpoint, before the campaign. Do not change
  firmware during a campaign. **Files:** `vehicle/main/main.c`. **Depends on:** M-10, M-16. **Acceptance:** the
  `itvl` value is in the first campaign's notes, and the offset checkpoint exists if it divides 1000 ms.

- [ ] **M-20. Run the timeout path on a board.** A round with no response ends in `timeout_cb`
  (`vehicle/main/main.c`), which calls `finish_round`, and so `ble_gap_conn_rssi`, from the esp_timer task.
  Normal rounds already send their challenge from that task and read RSSI in the host task. The RSSI read during
  a timeout has not run on a board. **Depends on:** M-01, M-16. **Acceptance:** at least one `TIMEOUT` row in a
  captured campaign, the next round runs, and the log shows no NimBLE error.

- [ ] **M-21. Key: reject short CHALLENGE writes (optional).** `key/main/main.c` HMACs 16 bytes even when the
  write was shorter (`answer_challenge`, `gatt_access`), so the unwritten bytes are stack data. Reject writes
  whose length is not `NONCE_LEN`. The vehicle always sends 16 bytes, so the bench is not affected. Do this only
  when `key/` is rebuilt for another reason. **Files:** `key/main/main.c`. **Acceptance:** a 5-byte write gets an
  error status and no notification.

### D. Run header and labelling

> **Status (2026-11-17):** implemented and compiled for `esp32`; to be validated on hardware.
> Behaviour changes that affect the bench:
> - `RUN label=<label> dist=<meters>` and `START` are commands on UART0 (the same port as OTA).
> - The campaign **no longer starts on its own**: send `START` after `subscribed to notify`. A
>   disconnection stops the campaign; `START` is needed again after reconnecting.
> - Without `RUN`, the label is `run` and the distance is `-`, with a warning in the log.
> - The round grid is fixed: one round every 1 s, counted from the previous send.
> - CSV v2 (`CSVHDR v2`): `label,dist_m,round,nonce,t_tx_us,rtt_us,result,rssi_dbm,itvl_1_25ms,mtu`.
> - `RUN_META` is printed at boot with the commit at the time of configure. After committing, run
>   `idf.py reconfigure` before the build, otherwise the line shows the previous commit.
> - Tools: `tools/capture_run.py` (capture and manifest) and `tools/analyze_runs.py` (joins by nonce,
>   splits the legs and computes statistics). The analyzer was tested with the old captures and with a
>   synthetic v2 log. A real capture has not been tested yet.

- [x] **M-13a. Build metadata.** Generate the git commit and the build date in CMake
  (`-DRUN_GIT_COMMIT=...`). Print the `RUN_META` line at boot, together with the SHA256 of the ELF.
  **Files:** `vehicle/CMakeLists.txt`, `vehicle/main/main.c`. **Depends on:** nothing.

- [x] **M-13b. Replicate `RUN_META` on the relays and the key.** The same line in `relays/` and `key/`.
  **Files:** `relays/main/main.c`, `key/main/main.c`. **Depends on:** M-13a.

- [x] **M-14a. Run command over serial.** Parse a line `RUN label=relay dist=3.0` arriving on UART0. The
  `serial_ota` component already reads `OTA:*` lines on that channel. **Files:**
  `components/serial_ota/serial_ota.c` or a new parser. **Depends on:** nothing.

- [x] **M-14b. Use the run's label and distance.** Replace the build-time `RUN_LABEL` with the command's
  value in the CSV, the summary and the display. Without a command, use the default and log a warning.
  **Files:** `vehicle/main/main.c`. **Depends on:** M-14a.

- [x] **M-15. Start on command.** The campaign only starts with `START` (or right after `RUN`). This way
  there are no unmarked warm-up rounds. **Files:** `vehicle/main/main.c`. **Depends on:** M-14a.

- [x] **M-16. Fixed round grid.** Schedule the challenge every `INTERVAL_MS` from the start, instead of
  1 s after each notify. The period then does not drift with the RTT. **Files:** `vehicle/main/main.c`.
  **Depends on:** M-01.

- [x] **M-17. Absolute timestamp in the CSV.** Include `t_tx` (µs since boot) on each line, to match the
  logs and build a timeline. **Files:** `vehicle/main/main.c`. **Depends on:** nothing.

- [x] **M-18. CSV v2 schema.** Fix the columns (label, distance, round, nonce, `t_tx_us`, `rtt_us`,
  result, RSSI, and whatever else is needed) in `CSVHDR` and document them. **Files:**
  `vehicle/main/main.c`. **Depends on:** M-02, M-14b, M-17.

### E. Capture and analysis (PC tools)

- [x] **D-01. Standardise the log encoding.** The files in `data/` are UTF-16 (baseline) and UTF-8 with
  BOM (relayed), which breaks `grep`. Convert them to UTF-8 without BOM and record this in a README in
  the folder. **Files:** `data/`. **Depends on:** nothing. *Done in checkpoint 08: UTF-8 copies in `data/utf8/` (`tools/recode_logs.py`); the originals are untouched. See `data/README.md`.*

- [x] **D-02. Capture script `tools/capture_run.py`.** Non-interactive capture with pyserial (the same
  procedure as `TESTING.md` section 6), writing UTF-8 to `data/runs/<date>_<label>/capture_<port>.log`.
  Run it with the ESP-IDF venv Python and `-I`. **Depends on:** nothing.

- [x] **D-03. Run manifest.** The same script writes `manifest.json` with the label, distance, date, commit
  and SHA, read from the `RUN_META` and `RUN` lines. **Files:** `tools/capture_run.py`. **Depends on:**
  D-02, M-13a, M-14b.

- [x] **D-04a. Join by nonce.** `tools/analyze_runs.py` joins the vehicle, Relay A and Relay B lines by
  nonce and produces a table per leg (vehicle<->B, tunnel, A<->key). **Depends on:** D-01, M-05.

- [x] **D-04b. Statistics.** Median, IQR, p90, bootstrap CI for the overhead, and success rate by distance
  with a Wilson CI. Output in CSV and Markdown. **Depends on:** D-04a.

- [x] **D-05a. Time-series chart.** RTT per round, coloured by result (OK, FAIL, TIMEOUT), one panel per
  condition (the panel boundary marks the change of condition). SVG from `tools/plot_runs.py`, no
  matplotlib. **Depends on:** D-04a.

- [x] **D-05b. Histogram and boxplot, baseline vs relay.** Histogram as % of each condition's rounds, overlaid per condition; box plot with whiskers at 1.5 IQR and outliers. SVG from `tools/plot_runs.py`. **Depends on:** D-04b.

- [x] **D-06. Update `TESTING.md`.** Done in checkpoint 06: `TESTING.md` was rewritten for the current
  procedure (three stages, `RUN`/`START`, CSV v2, relay peer MAC, bench photos). It must still be
  checked against the first real campaign. **Depends on:** M-18, D-03.

- [ ] **D-07. Analyzer fails loudly.** `tools/analyze_runs.py` drops vehicle CSV lines with too few fields without
  a message (`parse_rows`, lines 72-85), and it ignores any line whose tag is not `VEHICLE` or `VEICULO`. Change it
  to count the CSV lines seen and the rows parsed, stop with the line number on any mismatch, check that the row
  count equals the `n=` of the campaign's `===` summary line, and require exact field counts (10 for v2, 6 for v1).
  **Files:** `tools/analyze_runs.py`. **Depends on:** D-04a. **Acceptance:** a log with one truncated line stops the
  analyzer, and the pilot logs still give the numbers in `data/analysis/summary.md`.

- [ ] **D-08. Replicate by session.** At least three sessions per condition (baseline and relayed), each in its own
  folder under `data/runs/`, with the per-session medians shown next to the pooled CI. The bootstrap treats rounds as
  independent, so the pooled CI describes the spread within one session only. **Depends on:** D-02, D-03, D-04b.
  **Acceptance:** three session folders per condition, and the summary shows the spread between sessions.

### F. Paper text (limitations and discussion)

- [ ] **D-09. Limitations and discussion in the paper.** Use `LIMITATIONS.md` as the source. Check the two citations
  (Hancke and Kuhn, 2005; IEEE 802.15.4z) before they go into the paper. **Depends on:** M-19, D-08.
  **Acceptance:** the Discussion states the four limits and the mitigations paragraph, with checked references.

---

## Part 2: Visual

### Inspiration from deskrab (annotated)

| Element in deskrab | Where it is | How to use it in PKES |
|---|---|---|
| Fixed y bands: top 0–27, scene 28–107, text 112–168, data 172–230 | `deskrab/.claude/skills/cyd-animation/references/layout.md` | Same split, documented (V-01). Today the positions are spread across `cyd_ui.c`. |
| Top bar with status on the right | `deskrab/main/ui.c`, `layout.md` | Mode and link indicators (V-06). |
| Mascot that reacts to events (`event = true`) | `deskrab/main/skits.c` (`s_defs`) | Reaction to OK, FAIL and TIMEOUT (V-09a, V-09b). |
| `say()` bubble and thought cloud | `skits.c`, `layout.md` | Bubble with the round's RTT (V-09b). |
| Background scene that tells the story | `deskrab/main/scene.c`, README "Adding a scenario" | Car and key at the real distance (V-14a, V-14b). |
| Continuous effect tied to the state | `deskrab/main/fx.c` | Packet moving along the link, in real time (V-12b). |
| LED that breathes in the state's colour | `deskrab/main/led.c` | Pulse per result (V-10). |
| Simulator that draws the panel to BMP | `deskrab/tools/sim/`, `CLAUDE.md` "Screen simulator" | Preview and record screens without a board (V-17). |
| Accented fonts generated from Montserrat | `deskrab/tools/gen_latin_fonts.py` | Not needed: the project is in English (V-16, dropped). |
| Video pipeline | `deskrab/tools/anuncio/` (`telas.py`, `compose.py`, `video.py`) | Video assembly (V-18, dropped). |

### Tasks

> **Status (2026-11-17, checkpoint 02):** implemented and compiled for `esp32`; not yet seen on screen.
> - V-06: header with the mode and two indicators, BLE and ESP, between the title and the mascot
>   (x 116 to 250). Vehicle: mode = the RUN label, BLE only. Key: BLE only. Relays: mode `CHAIN` (or
>   `SELFTEST`), BLE when connected and ESP when the link comes up (in self-test the BLE indicator is
>   hidden).
> - V-08: `min ... med ... max ms` line below the cards, with the median of answered rounds. Updates each
>   round; shows `-` before the first answer.
> - The line sits at y 178, just below the second row of cards (which ends at y 174).
>
> **Status (2026-11-17, checkpoint 03):** see the checkpoint 02 block above for V-06 and V-08 (they are
> in the same checkpoint range; kept here for the record).
>
> **Status (2026-11-17, checkpoint 04):** implemented and compiled for `esp32`; not yet seen on screen.
> - Layout: the statistics line moved to font 12 with 200 px width, at y 180. The sparkline occupies
>   x 212..314, y 178..196. The cards did not change.
> - V-07a: sparkline of the last 30 rounds (line, in tenths of ms). A timeout is not a point: it appears
>   as a red mark at the base of its column.
> - V-07b: baseline band as a translucent white area behind the line. It comes from `BASELINE_BAND_MIN_US`
>   and `BASELINE_BAND_MAX_US` on the vehicle (default 0 = no band). Fill it with the min and max of the
>   measured baseline.
> - V-09a: mascot expression for 1.5 s. OK: closed eyes and a smile. FAIL: red eyes and a flat mouth.
>   TIMEOUT: wide eyes and an "oh" mouth.
> - V-09b: bubble with `91.2 ms OK`, `... FAIL` or `TIMEOUT` over the banner for 1.2 s, to the left of the
>   mascot. It covers the banner's state for a moment.
> - Limit: the sparkline scale is recalculated each round. Check on the board that the band and the red
>   marks are readable at 102 px.
>
> **Status (2026-11-17, checkpoint 05):** implemented and compiled for `esp32`; not yet seen on screen.
> - V-11: at the end of the campaign, the card grid is replaced by a panel with 3 lines (OK and failures,
>   median and IQR, min and max) and a 10-bin histogram. It alternates every 5 s. `START` returns to the grid.
> - V-12: the A-B link takes the right half of the second row of cards (the 3rd card moves to the left).
>   Each challenge draws a dot travelling from B to A; each response, from A to B. The text under the link
>   shows the real time of that leg. **Deviation from the plan:** the animation lasts a fixed 350 ms, not
>   the real time, because 22 ms on the tunnel is invisible to the eye. The real time is in the text.
> - V-13: stacked bar on the Relay B statistics line: tunnel | A<->key, showing the share of the round.
>   Relay A does not show the bar, because it does not know the tunnel.
> - V-14: on receiving `RUN dist=...`, the screen shows the car and the key for 5 s. The key sits at 110 px
>   for 0 m and 260 px for 10 m (limit x 280). **Deviation:** the scene disappears after 5 s; it does not stay
>   during the campaign, because the grid needs the card rows.
> - V-15: 2 s splash at boot with the role title and the commit (`fw ...`). **Deviation:** no mascot on the
>   splash, to avoid drawing the mascot twice. It stays in the background.
> - Limit: the V-12 animation and the V-09b bubble may briefly cover the banner. Check on the board.
>
> **Status (2026-11-17, checkpoint 02 detail):**
> - V-02: `cyd_fmt_us()` shows `91.2 ms` and `1.20 s`. Used for the vehicle RTT, the Relay B turnaround and
>   the key's `Calc` (the `Calc us` label became `Calc`).
> - V-03: state banner 240 px wide (was 308), so it does not cover the mascot at x 256-314.
> - V-04: the Relay B card shows the turnaround in BLE mode (label `Turnaround`); in self-test it still shows
>   the tunnel RTT (label `Tunnel RTT`).
> - V-05: thin progress bar below the banner, and the OK card shows `12 (100%)`. The `Fail / tmo` card keeps
>   `failures / timeouts`.
> - V-10: the LED flashes by result (green OK, red FAIL, yellow TIMEOUT) for 1.2 s and then breathes again.
>   Limit: on the last round, the switch to "done" cancels the flash before it shows.

> **Status (2026-11-17, checkpoint 07):** the simulator is in `tools/sim` (V-17a, V-17b). It renders the
> device's own `cyd_ui.c`, so the layout checks below were made on that code, not on a picture.
> - Checked: V-03 (the state banner ends at x 246; the mascot starts at x 256, so they do not overlap).
>   V-08 (the statistics line `min 83.0  med 91.2  max 97.5 ms` fits its 200 px box). V-15 (splash reads
>   `fw 64bd0f5`). V-11 (the histogram shows 5 s after the summary). V-14 (the car and the key scene at 0 m and
>   10 m: the key moves right with the distance). V-06 (the header indicators were checked by their
>   coordinates, not by pixels: the `ESP` label should end near x 253, against the mascot at x 256, a gap of
>   about 3 px).
> - **Fix found by the simulator:** the two hex lines were drawn in `montserrat_14`, with no width limit.
>   The key's 16-byte nonce ran to x 319 of 320, so the last digit could be cut on the panel. They are now
>   `montserrat_12` and end at x 258–270. **This changes the look on the device too.**
> - Not checked: the animations on hardware and the real colours of the panel (the simulator writes the
>   logical RGB; the ST7789 orientation and colour settings are not simulated).

> **Status (2026-11-17, checkpoint 09):** docs and small cleanups. Built for esp32 (six configurations), not flashed.
> - V-01 done: `components/cyd/LAYOUT.md` lists the vertical bands and every widget with its coordinates,
>   in the format of deskrab's `layout.md`.
> - Header: the campaign tag moved from x 116 to x 108 and the link indicators from x 182 to x 176 (the
>   ESP spacing is unchanged). Measured in the simulator: the `ESP` text now ends at x 258, about 7 px from
>   the mascot (it touched it before). In `BASELINE` mode the tag ends at x 181 and the BLE dot starts at x 189.
>   The BLE text and the ESP dot are 3 px apart. Checked in the relay B and vehicle frames.
> - `relays/main/relay_b.c`: `s_from_vehicle` is not unused. It is incremented in the BLE write handler, so
>   it is now declared only in the BLE build. The warning is gone from the self-test build.
> - `TASKS.md`: the toolchain item is done, listing the three v6.1 guards.
> - Portuguese comments left in `relays/main/CMakeLists.txt` and in the `sdkconfig.defaults` files of
>   `key/`, `vehicle/` and `relays/` are translated (comments only, no config value changed).

- [x] **V-01. PKES layout map.** Write `components/cyd/LAYOUT.md` with the y bands and the position of each
  widget, in the format of deskrab's `layout.md`. **Acceptance:** every widget in `cyd_ui.c` is listed with
  its coordinates. **Depends on:** nothing.

- [x] **V-02. Values with units.** `cyd_fmt_us()` shows `91.2 ms`, and above 1 s, `1.20 s`. Today the vehicle
  shows the raw `97545` (µs) and the key shows `Calc us`. Use it on the vehicle, relays and key.
  **Files:** `components/cyd/cyd_ui.c`, `cyd_ui.h`. **Depends on:** nothing.

- [x] **V-03. Mascot over the banner.** The mascot (58×58, at x 256–314 and y 2–60) covers the end of the
  state banner (y 44–74). Shrink the mascot or shorten the banner. Check with V-17b or a photo of the board.
  **Files:** `components/cyd/cyd_ui.c`. **Depends on:** V-01.

- [x] **V-04. Relay B shows the RTT in BLE mode.** The "RTT us" field is only updated in the self-test
  (`relays/main/relay_b.c`, around line 333). In BLE mode it stays "-" forever. Show B's turnaround.
  **Files:** `relays/main/relay_b.c`. **Depends on:** nothing.

- [x] **V-05. Progress and success rate.** Progress bar (`12/50`) and counters with percentages
  (`OK 12 (100%)`, `FAIL 0`, `TIMEOUT 0`). **Depends on:** M-02, V-02.

- [x] **V-06. Top bar with status.** Mode (`BASELINE` or `RELAY`), a BLE indicator and an ESP-NOW indicator,
  on the right, in the style of deskrab's top bar. **Depends on:** V-01, M-14b.

- [x] **V-07a. Sparkline of the last 30 rounds.** Line chart (`lv_chart`) of the last 30 rounds. Measure the
  LVGL pool use afterwards (deskrab uses 72 KB, close to 40%). **Depends on:** V-02.

- [x] **V-07b. Reference band and loss marks.** Shade the baseline's min–max band on the chart, and mark
  TIMEOUT with a red tick. **Depends on:** V-07a, M-02.

- [x] **V-08. Live statistics.** A line such as `min 83  med 91  max 98 ms`, updated each round.
  **Depends on:** V-02.

- [x] **V-09a. Mascot expressions.** Arc-shaped eyes for OK, X eyes and a flat mouth for FAIL, and a "?"
  for TIMEOUT. Use the poses of `build_mascot` in `cyd_ui.c`. **Depends on:** V-01.

- [x] **V-09b. Bubble with the result.** Show `91 ms OK` beside the mascot for about 1 s, like deskrab's
  `say()`. **Depends on:** V-09a.

- [x] **V-10. LED per round.** Short green for OK, red for FAIL, yellow for TIMEOUT, then back to the
  breathing of the state colour (`cyd_led_set`). **Depends on:** M-02.

- [x] **V-11a. In-RAM histogram.** Keep the campaign's RTTs and draw a 10-bin histogram. **Depends on:** V-02.

- [x] **V-11b. Final summary screen.** At the end of the campaign, alternate between the summary (OK%, median,
  IQR) and the histogram every 5 s. **Depends on:** V-11a, V-08.

- [x] **V-12a. Link drawing on the relay.** Two nodes (A and B) joined by a dashed line, in the scene band
  (y 28–107). **Depends on:** V-01.

- [x] **V-12b. Animated packet with real time.** A dot travels along the line for the measured time of the leg
  (for example 22 ms on the tunnel). The speed is the real one, not a fixed animation. **Depends on:** V-12a, M-05.

- [x] **V-13. Stacked bar for the last round.** Relay B shows a bar split into tunnel and A<->key, with the
  total. **Depends on:** M-05, V-02.

- [x] **V-14a. Car and key scene.** Static drawing of a car and a key in the scene band, in the style of
  deskrab's scenes. **Depends on:** V-01.

- [x] **V-14b. Distance in the scene.** Position the key in proportion to the distance given in the `RUN`
  command, with scale and limit. **Depends on:** V-14a, M-14b.

- [x] **V-15. Identity splash.** At boot: the title, the mascot and the short commit from `RUN_META`, for 2 s.
  **Depends on:** M-13a.

- [-] **V-16a. Accented fonts (optional).** Dropped. Generating Latin-1 fonts with deskrab's
  `gen_latin_fonts.py` is no longer needed.

- [-] **V-16b. Accented texts (optional).** Dropped and superseded by the full English translation
  (checkpoint 06). The on-screen font has no accented letters, so on-screen text stays ASCII.

- [x] **V-17a. Simulator harness for PKES.** Port of the idea in `deskrab/tools/sim/` to PKES: the device's
  own `cyd_ui.c` and LVGL 8.3 built with msys64 gcc; display, LED and timers replaced (`tools/sim/sim_hal.c`).
  Built in checkpoint 07. **Depends on:** V-01. *V-01 (`LAYOUT.md`) is still open; the harness does not need it.*

- [x] **V-17b. Script and frame recording.** `tools/sim/scenarios/*.txt` drive the `cyd_ui` calls with a virtual
  clock; each `shot` writes a 320×240 BMP (and a PNG via `run_scenarios.py`). Six scenarios: splash, vehicle
  campaign, relay B, relay A, key, distance scene. **Depends on:** V-17a.

- [-] **V-18. Video script and assembly.** Dropped by you: the video is not part of this project. Kept only
  as a record of the original plan.

---

## Annotated mockups

Screens at 320×240, simplified. The text inside them is what would appear on the screen (ASCII). These are
proposals; the real labels in the firmware may differ slightly.

### Vehicle (proposal)

```
+------------------------------------------------+
| VEHICLE                [BASELINE]  BLE  ESPNOW |  [1]
+------------------------------------------------+
| measuring                           12 / 50    |  [2]
| [##############--------------------]           |
|                                                |
|  RTT  91.2 ms                       (mascot)   |  [3] [9]
|  .--.__.--'`--.__.--.__.--'`--.__.--.          |  [4]
|  baseline band 83-98 ms                        |  [5]
|                                                |
|  min 83   med 91   max 98 ms                   |  [6]
|  OK 12 (100%)   FAIL 0   TIMEOUT 0             |  [7]
|  nonce 7c1dbaa5   resp 7c1dbaa5...             |  [8]
+------------------------------------------------+
```

- **[1]** Top bar with mode and links (V-06).
- **[2]** State and progress (V-05).
- **[3]** Large value with units (V-02).
- **[4]** Sparkline of the last 30 rounds (V-07a).
- **[5]** Baseline band and loss marks (V-07b).
- **[6]** Live statistics (V-08).
- **[7]** Counters with percentages (V-05).
- **[8]** Hex lines, as today.
- **[9]** Mascot with the expression of the last result (V-09a, V-09b).

### Relay B (proposal)

```
+------------------------------------------------+
| RELAY B                 [CHAIN]  BLE  ESPNOW   |  [1]
+------------------------------------------------+
|  (car)  o - - - - - - - - - o  (B)             |  [2]
|                packet 22 ms                    |
|  [== tunnel 22 ms ==][======= A-key 192 ms ===]|  [3]
|  turnaround B 214 ms   late 0   failures 0     |  [4]
|  nonce c728bac7                                |
+------------------------------------------------+
```

- **[1]** Top bar (V-06).
- **[2]** Link drawn with an animated packet in real time (V-12a, V-12b).
- **[3]** Stacked bar for the last round (V-13). The values are the measured ones: tunnel 22 ms and A<->key
  192 ms in round 1 of the 2026-11-16 run.
- **[4]** B's turnaround (V-04) and the counters from M-04 and M-07.

### Key (proposal)

Same structure as the vehicle, with the key mascot. The `Calc us` field becomes `Calc ms` (V-02).
Mascot expressions follow V-09a.

---

## Scene script for the video

Each scene shows measured numbers. The animations use real times, so the screen serves as evidence, not
decoration.

1. **Opening.** Splash with identity and commit (V-15). Car and key on the same screen, at zero distance (V-14a).
2. **Baseline.** Vehicle with the key direct: RTT around 91 ms, stable chart, green LED (V-02, V-07a, V-10).
3. **With relay.** Relays A and B: packet moving along the link and the stacked bar (V-12, V-13). The vehicle
   keeps marking `OK`, with a higher RTT.
4. **Distance.** The scene shows the key far away, and the vehicle still marks `OK` (V-14b). This is the paper's
   argument: the relay forwards identity, not distance.
5. **Result.** Histogram baseline vs relay on the summary screen (V-11b).

---

## Suggested order

1. Round validity: M-01 to M-05. They are the basis of any new number.
2. Capture and analysis tools: D-01 to D-04b. They let us handle data without depending on a board.
3. Tunnel and BLE: M-06a to M-12. They change the firmware but do not change a running campaign.
4. Low-cost visuals: V-01 to V-05, V-08, V-10. They use data that already exists.
5. Visuals with new data: V-06, V-07, V-09, V-11 to V-14. They depend on the earlier items.
6. Simulator: V-17. V-16 and V-18 are dropped.

---

## Pause: what was left out on purpose

- **D-01 (encoding of old logs):** decided in checkpoint 08. UTF-8 copies in `data/utf8/`; the originals stay as
  they were.
- **D-05 (charts):** decided in checkpoint 08. The charts are SVG, written with the standard library
  (`tools/plot_runs.py`). No matplotlib was installed.
- **D-06 (`TESTING.md`):** the new procedure is written (checkpoint 06). It still needs checking against the
  first real campaign, because the hardware may change a step.
- **Visual block (V):** the simulator (V-17) is done in checkpoint 07. Its frames are proposals for the screen
  design, and they are not measurements. The firmware logic does not run in it (see `tools/sim/README.md`).
- **Hardware validation:** none of the items above has run on a board yet. Suggested order: flash vehicle, key
  and relays; read `this board MAC` from the relays and paste it into `RELAY_PEER_MAC` (A points to B and B to
  A), rebuild and reflash the relays; then one baseline campaign and one relay campaign with
  `tools/capture_run.py --port COMx --label <label> --dist <m> --start`.

- **External review (checkpoint 12).** The gpt-oss analysis was checked against the code and the pilot data, and
  then deleted. These ideas were dropped:
  - **R-01 (rate limit on the key):** DoS only. The paper does not measure it, and the check would sit on the timed path.
  - **R-02 (pairing, encryption, address whitelist):** none of them stops the relay, since the relay is a real
    central to the key. BLE addresses can be cloned.
  - **R-03 (counter or timestamp in the nonce):** the vehicle's random nonce already gives freshness. A counter does
    not stop a relay that forwards the live nonce.
  - **R-04 (PSK protection, secure boot, flash encryption):** the PSK is a bench constant in the source. Both features
    are eFuse-based and hard to undo on the ESP32.
  - **R-05 (HMAC truncation sweep):** the relay forwards the genuine MAC, so its length changes neither the success nor
    the timing. Forgery odds are analytic.
  - **R-06 (5 ms turnaround limit):** the smallest relay-A turnaround in the 2026-11-16 pilot is 92.6 ms, so the limit
    would reject every round. Dropping slow rounds removes the overhead being measured.
  - **R-07 (sequence wrap detection):** at one round per second the 32-bit seq wraps after about 136 years, and
    matching is by equality.
  - **R-08 (power side-channel on the key):** outside `SCOPE.md` (timing and RTT). It moves the work toward key extraction.
  - **R-09 (replay simulation on a board):** it follows from the HMAC and the fresh nonce. One sentence in the paper covers it.
  - **R-10 (BOOT-RST automation):** serial OTA already avoids BOOT for updates, and BOOT is needed once per board.
  - **R-11 (CSV checksum):** replaced by D-07. Failing loudly on bad lines fixes the silent drops.
  - **R-12 (Hancke-Buchmann fast challenge-response on BLE):** BLE cannot give the per-bit timing it needs. It stays a
    discussion point (D-09), not an implementation.

---

## Checkpoints (local branches)

Each checkpoint is a commit on a `ckpt/...` branch. Each branch starts from the previous one, so an older
branch is always a complete state of the code. A checkpoint is only created after the code builds for
`esp32`. Branches go to `origin` when the user asks for it. `ckpt/01` to `ckpt/12` are pushed.

| Branch | Content | Status |
|---|---|---|
| `ckpt/01-rodada-ble-ferramentas` | M-01 to M-18, D-02, D-03, D-04a, D-04b | built for esp32, not flashed |
| `ckpt/02-visual-basico` | V-02, V-03, V-04, V-05, V-10 | built for esp32, not flashed, not seen on screen |
| `ckpt/03-topo-estatistica` | V-06, V-08 | built for esp32, not flashed, not seen on screen |
| `ckpt/04-sparkline-mascote` | V-07a, V-07b, V-09a, V-09b | built for esp32, not flashed, not seen on screen |
| `ckpt/05-visual-restante` | V-11a, V-11b, V-12a, V-12b, V-13, V-14a, V-14b, V-15 | built for esp32, not flashed, not seen on screen |
| `ckpt/06-english` | Full English translation (code, logs, docs, Android app text), `TESTING.md` rewritten, D-06 done, V-16 and V-18 dropped | built for esp32, not flashed |
| `ckpt/07-simulator` | V-17a, V-17b (`tools/sim`); hex lines font 14 to 12 in `cyd_ui.c` | built for esp32 (6 configs), not flashed; frames in `tools/sim/out` |
| `ckpt/08-analysis-plots` | D-01 (`data/utf8`, `data/README.md`), D-05a, D-05b (`tools/plot_runs.py`, SVG), `TESTING.md` step 6 | analysis tools only, no firmware change, so no esp32 build |
| `ckpt/09-docs-cleanup` | V-01 (`components/cyd/LAYOUT.md`), header shift in `cyd_ui.c`, `relay_b.c` warning, toolchain doc, translated build-file comments | built for esp32 (6 configs), not flashed; header checked in the simulator |
| `ckpt/10-home-setup` | `TESTING.md` checklist for a new machine ("Before you start on a new machine") | docs only, no firmware change |
| `ckpt/11-memory-notes` | Copies of the project memory notes in `memory/`; pilot CSV description corrected in `data/README.md` | docs only, no firmware change |
| `ckpt/12-review-notes` | `LIMITATIONS.md`; open tasks M-19 to M-21 and D-07 to D-09; dropped ideas R-01 to R-12; `TESTING.md` checks before the first campaign; `CLAUDE.md` pointer; repo copy of the memory note corrected | docs only, no firmware change |

Branch names from checkpoints 01 to 05 keep their original spelling, since they already exist.

Rule for going back: when flashing, test from the newest branch to the oldest. If the newest fails, go back one
branch, flash and test again. M-01 to M-05 are in the same commit as the rest (`vehicle/main/main.c` changed in
one go), so there is no way to revert only them.
