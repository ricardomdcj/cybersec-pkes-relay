# TESTING.md — Bench test and measurement procedure

How to bring the PKES relay bench up from scratch, validate each part, run the relay attack
end to end, and collect the numbers the paper needs. Read `SCOPE.md` first — this is a
self-contained bench of devices the authors own, not tooling against any real vehicle.

The firmware logs in English; the expected log lines below are quoted verbatim.

---

## Before you start on a new machine

1. Install ESP-IDF v6.1 with the Espressif Installation Manager. In a PowerShell window, load the environment:
   `. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1`
2. Get the code. The newest checkpoint is on `cyd-port`:
   `git clone https://github.com/ricardomdcj/cybersec-pkes-relay.git pkes-relay`, then `git checkout cyd-port`.
3. Generate the local configuration in each firmware folder. `sdkconfig` is gitignored and must not be copied
   from another machine. In `key/`, `vehicle/` and `relays/`, run `idf.py set-target esp32`. Check that
   `CONFIG_IDF_TARGET="esp32"` is in each `sdkconfig`. The first build downloads LVGL into `managed_components/`,
   so it needs internet.
4. Build each project once with `idf.py build`. The default relay build is role A with the self-test on.
5. Find the COM ports in Device Manager. On the work machine COM4 and COM5 are Bluetooth serial ports; the
   numbers on another PC will differ.
6. Give each relay role its own build folder, so switching roles does not rebuild everything:
   - key: `idf.py flash -p COMx` in `key/`
   - vehicle: `idf.py flash -p COMx` in `vehicle/`
   - Relay A: `idf.py -B %LOCALAPPDATA%\pkes_build\relay_a -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=0 -p COMx flash` in `relays/`
   - Relay B: the same, with `-DRELAY_ROLE=2` and `%LOCALAPPDATA%\pkes_build\relay_b`
   The relay peer MAC can stay zero. The tunnel then uses broadcast, and the MAC-level ACK counts stay at zero.
   Section 3 covers the peer MAC step if you want those counts.
7. The first flash of each board uses the BOOT + RST sequence in `CLAUDE.md`. Later flashes can use
   `python tools/serial_ota.py COMx <firmware>.bin`.
8. Campaign captures go to `data/runs/`. Put them on their own branch (for example `data/campaign-YYYYMMDD`), not on a
   checkpoint branch, and add them explicitly.

## 0. The three stages (and why)

You build confidence bottom-up. Each stage is a prerequisite for the next:

| Stage | What runs | Proves | Milestone |
|-------|-----------|--------|-----------|
| **A. Tunnel self-test** | Relay A + Relay B only (no BLE) | the two boards talk over ESP-NOW; measures pure tunnel RTT | M1 |
| **B. Honest baseline** | Vehicle + key (no relays) | the real challenge-response works; gives the baseline RTT | M3 control |
| **C. Relay chain** | key → Relay A → Relay B → vehicle | the relay carries a live exchange; the car authenticates a key that is out of range | M2 |

The headline result is **C's RTT minus B's RTT = the overhead the relay adds**. The attack
"works" because the vehicle still logs `OK` in stage C even with the key arbitrarily far
away — the relay forges distance, not identity.

---

## 1. Inventory

**Hardware** (four CYD boards, ESP32-2432S028R, each with its own USB cable)
- [ ] **CYD #1 — key** (`key/` firmware). Plays the key fob.
- [ ] **CYD #2 — Relay A** (near the key). `relays/`, `RELAY_ROLE=1`.
- [ ] **CYD #3 — Relay B** (near the vehicle). `relays/`, `RELAY_ROLE=2`.
- [ ] **CYD #4 — vehicle** (`vehicle/` firmware, BLE central, the verifier).

> All four boards are classic ESP32 CYDs. Before flashing, check that `CONFIG_IDF_TARGET` in
> each project's `sdkconfig` is `esp32` (see CLAUDE.md, "Stale sdkconfig trap").

**Software**
- [ ] ESP-IDF **v6.1** activated in the shell:
      `. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1`
- [ ] Python from the ESP-IDF venv (pyserial): `C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe`

---

## 2. Find your COM ports

Plug in one board at a time and note which `COMx` appears (Device Manager, or run
`idf.py -p COMx monitor` and see which responds). Write them down:

```
Key       -> COM__
Relay A   -> COM__
Relay B   -> COM__
Vehicle   -> COM__
```

COM4/COM5 on this machine are Bluetooth serial ports — ignore them.

> You cannot flash or monitor two boards on the same port. Each board needs its own USB.
> Only one program can hold a port at a time; see Troubleshooting if flashing says "port is busy".

---

## 3. Flash the key

From `key/` (after `idf.py set-target esp32` once):

```
idf.py -p COMx flash monitor
```

The key screen shows `advertising` and the log shows:

```
I RUN_META: role=key fw=<commit> build=<date time>
I KEY: advertising the key; waiting for the vehicle
```

Leave the key advertising for stages B and C.

---

## Stage A — ESP-NOW tunnel self-test (M1)

No BLE. Confirms the two relay boards reach each other and measures the tunnel overhead.
Both relays build with `RELAY_LINK_SELFTEST 1` (the default).

From `relays/`, one board at a time:

```
idf.py -DRELAY_ROLE=2 -DRELAY_LINK_SELFTEST=1 flash monitor -p COM__     # Relay B: the originator
idf.py -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=1 flash monitor -p COM__     # Relay A: the echo
```

With both powered, **Relay B's** monitor prints one line per second:

```
I RELAY_B: Relay B started (tunnel self-test; originating challenges every 1 s)
I RELAY_B: [selftest] response received (16 B); tunnel RTT=NNNN us
```

That `tunnel RTT` is the pure ESP-NOW round trip — already a data point for the paper. Record
a handful of values. If you never see "response received", the boards are not reaching each
other (see Troubleshooting: ESP-NOW).

Each relay prints its MAC address at boot (`this board MAC: xx:xx:xx:xx:xx:xx`). Note both: you
need them for the unicast setup below.

---

## Stage B — Honest baseline (vehicle + key, no relays)

This is the control the whole paper compares against. Only the key and the vehicle.

From `vehicle/`:

```
idf.py -p COM__ flash monitor
```

Then, in the monitor (or any serial terminal on the vehicle port), send the bench commands. The
label and the distance go into every CSV row:

```
RUN label=baseline dist=0.5
START
```

Wait for `subscribed to notify` before sending `START`; `START` is ignored until the key is
connected. The vehicle runs 50 rounds (`ROUNDS`, one every `INTERVAL_MS` = 1 s) and prints:

```
I VEHICLE: RUN label=baseline dist=0.5
I VEHICLE: scanning for the key service...
I VEHICLE: key found, connecting...
I VEHICLE: connected (handle 0); discovering service
I VEHICLE: subscribed to notify; ready. Send START to begin the campaign
I VEHICLE: campaign started: label=baseline dist=0.5 rounds=50 interval=1000 ms timeout=2000 ms
I VEHICLE: CSVHDR v2 label,dist_m,round,nonce,t_tx_us,rtt_us,result,rssi_dbm,itvl_1_25ms,mtu
I VEHICLE: round 1/50: OK  RTT=NNNN us  RSSI=-41 dBm  nonce=xxxxxxxx
I VEHICLE: CSV,baseline,0.5,1,xxxxxxxx,t_tx,NNNN,OK,-41,24,247
...
I VEHICLE: === baseline dist=0.5 n=50 ok=50 fail=0 timeout=0 disconnects=0 | RTT us (answered): min=A avg=B max=C ===
```

**Record `min/avg/max`.** All 50 rounds should be `OK` (the HMAC verifies). The nonce changes
every round, so this is a live exchange, not a replay. This is your **baseline RTT**.

---

## Stage C — Relay chain (M2, the attack)

Now insert the relays between the key and the vehicle, and set the relays to BLE mode
(`RELAY_LINK_SELFTEST 0`). Physical layout:

```
[ key ] --BLE--> [ Relay A ] --ESP-NOW--> [ Relay B ] --BLE--> [ vehicle ]
  far from the car    near the key           near the car       thinks the key is near
```

**Set the unicast peers (optional but recommended).** Put the MAC of the *other* relay into
`relays/main/relay_config.h` (`RELAY_PEER_MAC`): Relay A gets B's MAC and Relay B gets A's MAC.
While the macro is zeroed, the tunnel uses broadcast, which has no MAC-layer ACK, so tunnel
losses do not show up in the counters. Rebuild the two relays after the change.

Flash each relay with the real BLE halves:

```
idf.py -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=0 flash monitor -p COM__     # Relay A, near the key
idf.py -DRELAY_ROLE=2 -DRELAY_LINK_SELFTEST=0 flash monitor -p COM__     # Relay B, near the vehicle
```

**Both relays must come from the same build**, because the tunnel frame format changed. Keep the
**vehicle** and the **key** on the same firmware line as well. Give the vehicle the labels for
this run (`RUN label=relay dist=...`) and send `START` once it reports `subscribed to notify`.

**Bring-up order** (avoids the startup race): power **Relay A** and make sure it connects to
the key *first*, then start the vehicle so it begins the campaign once the chain is ready.

Expected logs:

- **Relay A** (near key):
  ```
  I RELAY_A: scanning for the key service...
  I RELAY_A: connected to key (handle 1); discovering service
  I RELAY_A: subscribed to key notify; ready to forward tunnel challenges
  I RELAY_A: HMAC from key (16 B) -> tunnel
  ```
- **Relay B** (near vehicle):
  ```
  I RELAY_B: advertising the key clone; waiting for the vehicle
  I RELAY_B: vehicle connected (handle 0)
  I RELAY_B: vehicle challenge (16 B) -> tunnel
  I RELAY_B: response (16 B) notified to the vehicle
  ```
- **Vehicle**: the same `round N/50: OK` and `=== ... ===` lines as Stage B — but the RTT now
  includes the full relay path. **This is your relayed RTT.**

Success = the vehicle logs `OK` rounds through the relay. The relayed HMAC verifies because
the response was computed by the *real key*, carried live across the tunnel.

> At startup you may briefly see `RELAY_A: ... the key is not ready; dropping` or a lost
> round on the vehicle — that is the chain still coming up. The 50 rounds give it slack.

**Tunnel-side evidence (relay logs).** Relay A logs `CSVA,seq,nonce,turnaround_us,rssi,tx_failures`
and Relay B logs `CSVB,seq,nonce,turnaround_us,rssi,turnaround_A_us,tx_failures`. The round is
correlated by the nonce across all three boards.

---

## 4. Real-life proceedings — the demonstration

How to actually stage the attack for the paper / a live demo:

1. **Set the scene.** The vehicle (car) sits locked and waiting. The key (owner's fob) is
   somewhere the car cannot hear it over BLE — another room, down the hall, outside. Prove
   this first: with *no relays*, move the key away until the vehicle can no longer find it
   (it keeps logging `scanning for the key service...` and never connects). That is the
   honest security premise: out of range ⇒ no auth.
2. **Place the relays.** Put **Relay B** next to the car, **Relay A** next to the owner's
   key. The two relays are linked by ESP-NOW (the "long wire" an attacker would stretch
   between the car and the key — on the bench, just two boards that can sit meters apart).
3. **Run it.** The car scans, finds **Relay B** — which advertises the *exact* cloned service
   UUID of the key — connects, and runs its normal challenge-response. Every challenge is
   carried key-ward over the tunnel, answered by the real key, and carried back. The car
   logs `OK` and would "unlock": **it authenticated a key that is nowhere near it.**
4. **Make the point.** Keep increasing the key↔car distance (well beyond BLE range) while
   each relay stays near its endpoint. Authentication still succeeds. Distance was forged, not
   identity — the nonce/HMAC crypto is intact and unbroken; it just never proved proximity.
5. **Record** the per-round RTT and whether each round was `OK`, for both the honest baseline
   and the relayed run, at several separations (next section).

What to capture for the write-up: the vehicle monitor of a failed honest attempt at distance
(step 1), the three relay/vehicle monitors during a successful relayed run (step 3), and the
RTT tables (step 5). Photos of the physical layout help the paper.

---

## 5. Measurement campaign (feeds the paper)

For each condition, run the vehicle's 50-round campaign and keep the captures.

Before the first campaign, read the vehicle's `CONN` line at connect and note `itvl`. If it divides 1000 ms (40 or
50 ms, for example), the 1 s round grid keeps the rounds of a campaign on one connection phase. Since M-19
(`b79a463`), each START draws a start offset in [0, 1 s), so the campaigns sample different phases. Note the
offset (`grid start offset` line) with the campaign. Do not change firmware during a campaign.

1. **Baseline** (Stage B): vehicle ↔ key directly. Repeat several times for a distribution.
2. **Relayed** (Stage C): full chain, same `ROUNDS`. Repeat.
3. **Overhead** = relayed RTT − baseline RTT (median, IQR, and per round).
4. **Vary distance** and repeat:
   - key ↔ Relay A separation,
   - Relay A ↔ Relay B separation (the ESP-NOW hop — the most interesting one),
   - Relay B ↔ vehicle separation.
   Push until rounds start failing (`TIMEOUT` rows or `FAIL` rows); that failure point is the
   relay's effective range and the basis for a timing/distance-bounding mitigation discussion.
   Use `TXPOW <n>` on the relays to sweep the radio power without moving the boards.
   The first `TIMEOUT` on a board also tests the timeout path (M-20). The round must be logged as `TIMEOUT`, and the
   next round must run. If the vehicle stops or resets, keep the log and stop the campaign.
5. **Collect the dataset.** Use the capture script — it sends `RUN` and `START`, saves the log in UTF-8 and
   writes a manifest with the label, distance, firmware commit and result counts:

   ```
   C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe -I tools/capture_run.py ^
       --port COM6 --label relay --dist 3.0 --start
   ```

   The output goes to `data/runs/<timestamp>_<label>/` (`capture_COM6.log` and `manifest.json`).
   Use `--no-reset` to keep a running board untouched, and `--max-s` to limit the time.

   Each vehicle round is one `CSV` line (CSVHDR v2):

   ```
   label,dist_m,round,nonce,t_tx_us,rtt_us,result,rssi_dbm,itvl_1_25ms,mtu
   ```

   `result` is `OK`, `FAIL` (HMAC mismatch) or `TIMEOUT` (no response within `ROUND_TIMEOUT_MS`,
   with `rtt_us = -1`). `itvl_1_25ms` is the negotiated connection interval (1.25 ms units) and
   `mtu` is the ATT MTU. The relay lines (`CSVA`, `CSVB`) carry the turnaround of each relay, and
   `CSVB` also carries A's turnaround and the tunnel failure counter.

6. **Analyze.** Compare any number of campaigns in one go; the first input is the reference
   condition for the overhead:

   ```
   python -I tools/analyze_runs.py data/runs/<baseline> data/runs/<relay> --out data/analysis
   ```

   It writes `rounds.csv` (one row per round, with the legs: vehicle↔B, tunnel, A↔key) and
   `summary.md` (counts, Wilson CI on the success rate, median and IQR, bootstrap CI on the
   median and on the overhead). It also reads the older captures in `data/`.

   Then draw the charts (SVG, standard library only):

   ```
   python -I tools/plot_runs.py data/analysis/rounds.csv
   ```

   `rtt_timeseries.svg` has one panel per condition, coloured by result, with a cross for each TIMEOUT.
   `rtt_histogram.svg` and `rtt_boxplot.svg` compare the conditions.

---

### Latency sweep (`DELAY`, relay B)

Relay B holds every vehicle challenge for `DELAY <ms>` before it goes on the tunnel (`relays/main/relay_b.c`,
off at 0). The verifier authenticates a round only if its response arrives within `ROUND_TIMEOUT_MS`
(2000 ms), so the success rate falls where the injected delay plus the honest relayed round trip reaches the
timeout. The vehicle must run M-22 or later: a response that arrives after its round has timed out is
discarded instead of closing the round in flight.

- The sweep runs from the repo root, one relayed campaign per delay, labelled `relay-dNNNN`:
  `powershell -NoProfile -ExecutionPolicy Bypass -File tools\campaign\run_sweep.ps1 -Delays 500,1000 -Dist 0.99`.
  Without `-Delays` it runs the default list in the script, which starts at 0.
- `run_relay.ps1 -DelayMs <ms>` sends `COM7 DELAY <ms>` and waits for the relay's `DELAY set=<ms> ms` line.
  It is sent on every relayed campaign (default 0), so no campaign inherits the delay of the one before.
- The deskrab status-line bridge respawns while a session is active and takes COM5, COM6 and COM8 during
  bring-up. `run_relay.ps1` keeps it stopped until the passive logger has the three ports; a campaign that
  still fails at that step is retried by the sweep.
- The expected threshold is the timeout minus the honest relayed round trip at D = 0; `tools/analyze_delay.py`
  computes it, with the measured D50 and the table, into `data/analysis-20261118/delay_sweep.md`.

---

## 6. Capturing logs to files

`idf.py monitor` is interactive (exit with **Ctrl+]**). For hands-off capture, use
`tools/capture_run.py` (section 5, step 5). If you need a raw capture of any port, the pyserial
one-liner below works too (the RTS pulse resets the board, so the boot log is included):

```
C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe - <<'PY'
import serial, time
port, secs = "COM__", 30
s = serial.Serial(port, 115200, timeout=1)
s.setRTS(False); time.sleep(0.1); s.setRTS(True)   # pulse reset
end = time.time() + secs
with open("capture.log", "wb") as f:
    while time.time() < end:
        f.write(s.read(4096))
s.close()
PY
```

To watch all four boards at once, `tools/monitor4.ps1` multiplexes the four ports into one log
tagged by role (`KEY`, `RELAY_A`, `RELAY_B`, `VEHICLE`). Edit its port map first.

---

## 7. Troubleshooting

- **"port is busy" on flash.** A monitor still holds the port — often `idf.py monitor` in
  another terminal or the VS Code ESP-IDF extension. Close it (don't kill the board).
- **Relay A never connects to the key** (`scanning...` forever). Key not started or not
  advertising, or the key board is on an older firmware. Check the key's monitor for
  `advertising the key; waiting for the vehicle`.
- **Vehicle never finds Relay B.** Relay B is not advertising — its monitor must show
  `advertising the key clone`. If it is stuck, it was likely flashed in self-test mode;
  rebuild with `-DRELAY_LINK_SELFTEST=0`.
- **Everything is in self-test / no BLE.** `-DRELAY_LINK_SELFTEST=0` must be on the build
  command for Stage C, and `1` (default) for Stage A. The `-D` triggers a reconfigure; no
  need to edit headers.
- **ESP-NOW silent (Stage A).** Both relays must be on the same channel (fixed at
  `LINK_CHANNEL 1` in `link.c`) and within radio range. With broadcast (`RELAY_PEER_MAC`
  zeroed) no MAC pairing is needed; with unicast, the MACs must be the right ones.
- **`the key is not ready; dropping` at startup.** Expected briefly — the vehicle wrote a
  challenge before Relay A finished connecting to the key. Bring Relay A + key up first, or
  ignore the first lost round.
- **`START ignored: no key subscribed`.** The vehicle has not finished subscribing to the key's
  notifications yet. Wait for `subscribed to notify` and send `START` again.
- **Campaign stopped after a disconnection.** Expected: `campaign interrupted (disconnection)`.
  Send `START` again after the boards reconnect. The interrupted round is not counted.
- **A relay reports `TX failures` going up.** With unicast this is a real MAC-level failure
  (the peer is out of range or down). With broadcast the counter stays at zero, because there is
  no ACK to report.
- **Changed `sdkconfig.defaults` and nothing happened.** Defaults only apply to keys missing
  from `sdkconfig`. Delete `sdkconfig` and rebuild (`del sdkconfig`, then build). Also check
  `CONFIG_IDF_TARGET` (see CLAUDE.md).
- **App partition overflow on flash.** The BLE relay binary is ~1.4 MB; `sdkconfig.defaults`
  already selects `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE`. If you see an overflow, your
  `sdkconfig` is stale — delete and rebuild.

---

## 8. Success criteria

- **Stage A:** Relay B logs `tunnel RTT=` values every second. ✔ tunnel works.
- **Stage B:** the vehicle logs `ok=50 fail=0 timeout=0` and a baseline RTT. ✔ honest baseline captured.
- **Stage C:** the vehicle logs `OK` rounds *through the relays*, with the key out of its own
  BLE range. ✔ the relay forwards a live exchange end to end.
- **For the paper:** baseline vs. relayed RTT tables, the overhead, and the distance at which
  the relay starts to fail.
