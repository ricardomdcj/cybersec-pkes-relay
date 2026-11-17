# tools/sim — screen simulator

Draws the bench's screens on the PC. It compiles the device's own `components/cyd/cyd_ui.c` with
LVGL 8.3 and writes 320×240 frames, so the layout and the reactions can be checked without a board.
Only the hardware is replaced (`sim_hal.c`: display, LED, timers); the UI code is the one that
runs on the CYD.

## What it covers and what it does not

- **Covers:** the screens for any sequence of `cyd_ui` calls, with a virtual clock. Animations, the
  LED calls, the UI timers (splash, badge, flash) and the 5-second summary switch all run.
- **Does not cover:** the firmware logic (BLE, ESP-NOW, the round timer). Those need the radio. The
  scenarios call the UI API directly with the values the firmware would send, so the numbers in the
  scenarios are samples, not measurements.
- **Memory:** the LVGL pool is doubled, because pointers are bigger on the PC. Memory use in the
  simulator says nothing about the device.

## Build

Build the vehicle once with `idf.py build` (the simulator takes the LVGL sources and the LVGL
options from it), then, from the `pkes-relay` folder:

```
python tools/sim/build_gcc.py                  # uses msys64 gcc (C:\msys64\ucrt64\bin\gcc.exe)
python tools/sim/build_gcc.py --clean          # after changing a header
```

The output is `tools/sim/build/pkes_sim.exe`. The first build compiles LVGL (about 10 s here).

## Run

```
python tools/sim/run_scenarios.py                 # every scenario
python tools/sim/run_scenarios.py relay_a         # one scenario
```

Each `shot` in a scenario writes a BMP to `tools/sim/out/`, and `run_scenarios.py` also writes a PNG
of each frame (`bmp2png.py` does the same for one file). The log of each run is next to the frames.

## Scenarios

| File | What it shows |
|---|---|
| `splash.txt` | the boot splash, then the card grid (V-15) |
| `vehicle_campaign.txt` | a whole baseline campaign: idle, measuring with a sparkline and a timeout mark, the OK / TIMEOUT / FAIL reactions, the summary and the histogram (V-02 to V-11) |
| `relay_b.txt` | the relay B chain: stacked bar and link animation (V-12, V-13) |
| `relay_a.txt` | the relay A screen |
| `key.txt` | the key advertising, then answering a challenge |
| `distance_scene.txt` | the car and key scene at 0 m and 10 m (V-14) |

## Script commands

One command per line; `#` starts a comment. The first UI command must be `init`.

```
init <key|vehicle|relay_a|relay_b>   bring up the UI of a role (once per run)
verbose <on|off>                     print LED calls with the virtual time
wait <ms>                            advance the virtual clock
shot <file.bmp>                      write the frame to the output folder
tag <text|->                         header mode
link <ble|esp> <-1|0|1>              header indicator: hidden, down, up
state <hex colour> <text>            state banner
field <idx> <text>                   value of card idx
hex <idx> <hex bytes|->              hex line 0 or 1
stats <text|->                       statistics line
progress <cur> <total>               progress bar
pulse                                mascot hop
flash <hex colour> <ms>              LED blink
expr <ok|fail|timeout|normal> <ms>   mascot expression
badge <hex colour> <ms> <text>       balloon over the banner
spark <reset|timeout|us>             sparkline
band <lo_us> <hi_us>                 baseline band
linkpulse <0|1> [text|-]             relay link animation (0 = B->A, 1 = A->B)
stack <a_us> <b_us>                  stacked bar (negative hides it)
scene <dist_x10> <ms>                car and key scene (tenths of a metre)
splash <ms> <subtitle>               boot splash
summary <bins>|<legend>|<l1>|<l2>|<l3>   final summary and histogram
grid                                 back to the card grid
```

## Adding a screen

Write a `.txt` in `scenarios/` with `init` and the calls the firmware makes, then `shot` where you
want a frame. The role configs at the top of `sim_main.c` are copies of the `cyd_ui_cfg_t` in each
firmware (`key/main/main.c`, `vehicle/main/main.c`, `relays/main/relay_a.c`, `relays/main/relay_b.c`):
update them when a card label changes.

## Files

- `sim_main.c` — the script runner and the role configs.
- `sim_hal.c` / `sim_hal.h` — the replaced hardware: LVGL display into a 320×240 framebuffer, LED
  logger, virtual clock and `esp_timer`, BMP output.
- `stubs/` — the few ESP-IDF headers that `cyd_ui.c` includes.
- `build_gcc.py` — the build. `run_scenarios.py` — runs the scenarios. `bmp2png.py` — BMP to PNG.
- `build/` and `out/` are generated and gitignored.
