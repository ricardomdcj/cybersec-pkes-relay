# TESTING.md — Bench test & measurement procedure

How to bring the PKES relay bench up from nothing, validate each piece, run the relay
attack end to end, and collect the numbers the paper needs. Read `SCOPE.md` first — this is
a self-contained bench of devices the authors own, not tooling against any real vehicle.

The firmware logs in Portuguese; the expected log lines below are quoted verbatim.

---

## 0. The three stages (and why)

You build confidence bottom-up. Each stage is a prerequisite for the next:

| Stage | What runs | Proves | Milestone |
|-------|-----------|--------|-----------|
| **A. Tunnel self-test** | Relay A + Relay B only (no BLE) | the two boards talk over ESP-NOW; measures pure tunnel RTT | M1 |
| **B. Honest baseline** | Vehicle + phone (no relays) | the real challenge-response works; gives the baseline RTT | M3 control |
| **C. Relay chain** | phone → Relay A → Relay B → vehicle | the relay carries a live exchange; the car authenticates a key that is out of range | M2 |

The headline result is **C's RTT minus B's RTT = the overhead the relay adds**. The attack
"works" because the vehicle still logs `OK` in stage C even with the phone arbitrarily far
away — the relay forges distance, not identity.

---

## 1. Inventory

**Hardware**
- [ ] Android phone (Android 12+), running the `phoneaskey` app — plays the **key**.
- [ ] **ESP32-S3** board — **Relay A** (near the phone).
- [ ] **ESP32-C3** board — **Relay B** (near the vehicle).
- [ ] One more BLE-capable ESP32 board — the **vehicle** (verifier/central).
- [ ] USB cables for each board; the phone charged with Bluetooth on.

> **Board-collision warning:** `vehicle/` and Relay A both default to **esp32s3**. If you only
> own one S3, flash the vehicle to a *different* chip — the vehicle is a plain NimBLE central
> and runs on any BLE ESP32 (esp32, esp32c3, esp32s3, esp32c6…). Just `idf.py set-target <chip>`
> on `vehicle/`. The final vehicle endpoint is meant to be a classic esp32.

**Software**
- [ ] ESP-IDF **v6.1** activated in the shell:
      `. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1`
- [ ] Android SDK / Gradle wrapper (for the phone app), a USB-debugging-enabled phone.

---

## 2. Find your COM ports

Plug in one board at a time and note which `COMx` appears (Device Manager, or run
`idf.py -p COMx monitor` and see which responds). Write them down:

```
Vehicle   -> COM__
Relay A   -> COM__
Relay B   -> COM__
```

COM4/COM5 on this machine are Bluetooth serial ports — ignore them.

> You cannot flash or monitor two boards on the same port. Each board needs its own USB.
> Only one program can hold a port at a time; see Troubleshooting if flashing says "port is busy".

---

## 3. Build & flash the phone app (the key)

From `phoneaskey/`:

```
gradlew.bat installDebug      # build + install on the connected phone
```

Open the app, grant the Bluetooth permissions it asks for (Android 12+ needs
`BLUETOOTH_ADVERTISE` and `BLUETOOTH_CONNECT`), and **start the key**. The on-screen log
should show it advertising:

```
Servidor GATT criado, adicionando servico...
Servico adicionado (status 0), iniciando advertising
Anunciando servico 8f2a0001-1b2c-4e6a-9f3d-2a7c1e5b0a10
```

Leave the key advertising for stages B and C.

---

## Stage A — ESP-NOW tunnel self-test (M1)

No BLE. Confirms the two relay boards reach each other and measures the tunnel overhead.
Both relays build with `RELAY_LINK_SELFTEST 1` (the default).

From `relays/`:

```
REM --- Relay B (ESP32-C3): the self-test originator ---
idf.py set-target esp32c3
idf.py -DRELAY_ROLE=2 flash monitor -p COM__      # RELAY_LINK_SELFTEST defaults to 1

REM --- Relay A (ESP32-S3): the self-test echo ---  (new shell / second board)
idf.py fullclean
idf.py set-target esp32s3
idf.py -DRELAY_ROLE=1 flash monitor -p COM__
```

With both powered, **Relay B's** monitor prints one line per second:

```
I RELAY_B: [selfteste] originando desafios pelo tunel a cada 1 s
I RELAY_B: [selfteste] resposta recebida (16 B); RTT tunel=NNNN us
```

That `RTT tunel` is the pure ESP-NOW round trip — already a data point for the paper. Record
a handful of values. If you never see "resposta recebida", the boards are not reaching each
other (see Troubleshooting: ESP-NOW).

> `fullclean` + `set-target` is needed whenever you switch the target chip between the two
> relays on the same build tree. Working on two checkouts/folders (one per board) avoids it.

---

## Stage B — Honest baseline (vehicle + phone, no relays)

This is the control the whole paper compares against. Only the phone (key) and the vehicle.

From `vehicle/`:

```
idf.py set-target esp32s3        # or your chosen vehicle chip
idf.py -DRUN_LABEL=baseline flash monitor -p COM__
```

With the phone's key advertising (Stage 0), the vehicle scans, connects directly to the
phone, runs 20 rounds (`ROUNDS`, one per `INTERVAL_MS`=1 s) and prints:

```
I VEICULO: escaneando pelo servico da chave...
I VEICULO: chave encontrada, conectando...
I VEICULO: conectado (handle 1); descobrindo servico
I VEICULO: inscrito em notify; iniciando 20 rodadas (rotulo=baseline)
I VEICULO: CSVHDR label,rodada,rtt_us,resultado,rssi_dbm
I VEICULO: rodada 1/20: OK  RTT=NNNN us  RSSI=-41 dBm
I VEICULO: CSV,baseline,1,NNNN,OK,-41
...
I VEICULO: === baseline n=20 ok=20 falha=0 | RTT us: min=A avg=B max=C ===
```

**Record `min/avg/max`.** All 20 rounds should be `OK` (the HMAC verifies). The nonce changes
every round, so this is a live exchange, not a replay. This is your **baseline RTT**.

---

## Stage C — Relay chain (M2, the attack)

Now insert the relays between the phone and the vehicle, and rebuild the relays in BLE mode
(`RELAY_LINK_SELFTEST 0`). Physical layout:

```
[ phone/key ] --BLE--> [ Relay A (S3) ] --ESP-NOW--> [ Relay B (C3) ] --BLE--> [ vehicle ]
     far from the car        near the phone              near the car         thinks key is near
```

Flash each relay with the real BLE halves:

```
REM --- Relay A (ESP32-S3), near the phone ---
idf.py set-target esp32s3
idf.py -DRELAY_ROLE=1 -DRELAY_LINK_SELFTEST=0 flash monitor -p COM__

REM --- Relay B (ESP32-C3), near the vehicle ---  (second board / folder)
idf.py fullclean
idf.py set-target esp32c3
idf.py -DRELAY_ROLE=2 -DRELAY_LINK_SELFTEST=0 flash monitor -p COM__
```

Keep the **vehicle** firmware running (same logic — it is the instrument). For a cleanly
labelled dataset, reflash it with `idf.py -DRUN_LABEL=relay flash monitor` so its CSV rows and
summary read `relay` instead of `baseline`. Keep the phone's key advertising.

**Bring-up order** (avoids the startup race): power **Relay A** and make sure it connects to
the phone *first*, then start/reset the vehicle so it begins writing challenges once the chain
is ready.

Expected logs:

- **Relay A** (near phone):
  ```
  I RELAY_A: escaneando pelo servico da chave...
  I RELAY_A: conectado a chave (handle 1); descobrindo servico
  I RELAY_A: inscrito em notify na chave; pronto para encaminhar desafios do tunel
  I RELAY_A: HMAC da chave (16 B) -> tunel
  ```
- **Relay B** (near vehicle):
  ```
  I RELAY_B: anunciando o clone da chave; aguardando o veiculo
  I RELAY_B: veiculo conectado (handle 1)
  I RELAY_B: desafio do veiculo (16 B) -> tunel
  I RELAY_B: resposta (16 B) notificada ao veiculo
  ```
- **Vehicle**: same `rodada N/20: OK` and `=== BASELINE ... ===` lines as Stage B — but the
  RTT now includes the full relay path. **This is your relayed RTT.**

Success = the vehicle logs `OK` rounds through the relay. The relayed HMAC verifies because
the response was computed by the *real key* (phone), carried live across the tunnel.

> At startup you may briefly see `RELAY_A: ... a chave nao esta pronta; descarto` or a lost
> round on the vehicle — that is the chain still coming up; the 20 rounds give it slack.

---

## 4. Real-life proceedings — the demonstration

How to actually stage the attack for the paper / a live demo:

1. **Set the scene.** The vehicle (car) sits locked and waiting. The phone (owner's key) is
   somewhere the car cannot hear it over BLE — another room, down the hall, outside. Prove
   this first: with *no relays*, move the phone away until the vehicle can no longer find it
   (it keeps logging `escaneando pelo servico da chave...` and never connects). That is the
   honest security premise: out of range ⇒ no auth.
2. **Place the relays.** Put **Relay B** next to the car, **Relay A** next to the owner's
   phone. The two relays are linked by ESP-NOW (the "long wire" an attacker would stretch
   between the car and the key — on the bench, just two boards that can sit meters apart).
3. **Run it.** The car scans, finds **Relay B** — which advertises the *exact* cloned service
   UUID of the key — connects, and runs its normal challenge-response. Every challenge is
   carried phone-ward over the tunnel, answered by the real phone, and carried back. The car
   logs `OK` and would "unlock": **it authenticated a key that is nowhere near it.**
4. **Make the point.** Keep increasing the phone↔car distance (well beyond BLE range) while
   each relay stays near its endpoint. Authentication still succeeds. Distance was forged, not
   identity — the nonce/HMAC crypto is intact and unbroken; it just never proved proximity.
5. **Record** the per-round RTT and whether each round was `OK`, for both the honest baseline
   and the relayed run, at several separations (next section).

What to capture for the write-up: the vehicle monitor of a failed honest attempt at distance
(step 1), the three relay/vehicle monitors during a successful relayed run (step 3), and the
RTT tables (step 5). Photos of the physical layout help the paper.

---

## 5. Measurement campaign (feeds the paper — M3)

For each condition, run the vehicle's 20-round sweep and capture the `=== BASELINE ... ===`
line plus the per-round `RTT=` values.

1. **Baseline** (Stage B): vehicle ↔ phone directly. Repeat a few times for a distribution.
2. **Relayed** (Stage C): full chain, same `ROUNDS`. Repeat.
3. **Overhead** = relayed RTT − baseline RTT (min/avg/max, and per round).
4. **Vary distance** and repeat:
   - phone ↔ Relay A separation,
   - Relay A ↔ Relay B separation (the ESP-NOW hop — the most interesting one),
   - Relay B ↔ vehicle separation.
   Push until rounds start failing (timeouts / dropped notifies); that failure point is the
   relay's effective range and the basis for a timing/distance-bounding mitigation discussion.
5. **Collect the dataset.** The vehicle emits a parseable row per round on its own monitor:

   ```
   I (NNNN) VEICULO: CSVHDR label,rodada,rtt_us,resultado,rssi_dbm
   I (NNNN) VEICULO: CSV,baseline,1,3456,OK,-41
   ```

   Build the vehicle with a **run label** so baseline and relayed runs self-identify:
   `idf.py -DRUN_LABEL=baseline build` for Stage B, and reflash with `-DRUN_LABEL=relay` for
   Stage C (the firmware logic is identical — only the label changes). The summary line echoes
   it too: `=== baseline n=20 ... ===`.

   Extract the dataset from a capture with `grep "CSV," capture.log` and split on commas:
   `label` (baseline|relay), `rodada`, `rtt_us`, `resultado` (OK|FALHA), `rssi_dbm` (the
   vehicle's connection RSSI, 0 if unavailable). Add the physical separation you set
   (phone↔A, A↔B, B↔vehicle, in metres) as an extra column by hand — the firmware can't know it.

---

## 6. Capturing logs to files

`idf.py monitor` is interactive (exit with **Ctrl+]**). For hands-off capture into a file so
you can parse RTTs later, open the port non-interactively with pyserial (keeps the port free
for a fixed number of seconds, and the RTS pulse resets the board so the boot log is included):

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

Point it at the vehicle port for RTTs; one capture per run. Then `grep "CSV," capture.log`
for the per-round dataset rows (and the `===` summary line for min/avg/max).

---

## 7. Troubleshooting

- **"port is busy" on flash.** A monitor still holds the port — often `idf.py monitor` in
  another terminal or the VS Code ESP-IDF extension. Close it (don't kill the board).
- **Relay A never connects to the key** (`escaneando...` forever). Phone Bluetooth off, the
  app's key not started/advertising, or Android permissions denied. Re-check Stage 0; the
  app log must show `Anunciando servico 8f2a0001-...`.
- **Vehicle never finds Relay B.** Relay B not advertising — its monitor must show
  `anunciando o clone da chave`. If it is stuck, it was likely flashed in self-test mode;
  rebuild with `-DRELAY_LINK_SELFTEST=0`.
- **Everything is in self-test / no BLE.** `-DRELAY_LINK_SELFTEST=0` must be on the build
  command for Stage C, and `1` (default) for Stage A. The `-D` triggers a reconfigure; no
  need to edit headers.
- **ESP-NOW silent (Stage A).** Both relays must be on the same channel (fixed at
  `LINK_CHANNEL 1` in `link.c`) and within radio range. The link is broadcast, so no MAC
  pairing is needed.
- **`a chave nao esta pronta` at startup.** Expected briefly — the vehicle wrote a challenge
  before Relay A finished connecting to the key. Bring Relay A + phone up first, or ignore the
  first lost round.
- **Changed `sdkconfig.defaults` and nothing happened.** Defaults only apply to keys missing
  from `sdkconfig`. Delete `sdkconfig` and rebuild (`del sdkconfig`, then build).
- **App partition overflow on flash.** The BLE relay binary is >1 MB; `sdkconfig.defaults`
  already selects `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE`. If you see an overflow, your
  `sdkconfig` is stale — delete and rebuild.

---

## 8. Success criteria

- **Stage A:** Relay B logs `RTT tunel=` values every second. ✔ tunnel works.
- **Stage B:** vehicle logs `ok=20 falha=0` and a `BASELINE` RTT. ✔ honest baseline captured.
- **Stage C:** vehicle logs `OK` rounds *through the relays*, with the phone out of its own
  BLE range. ✔ the relay forwards a live exchange end to end.
- **For the paper:** baseline vs. relayed RTT tables, the overhead, and the distance at which
  the relay starts to fail.
