# Screen map — `cyd_ui` (CYD, 320x240, landscape)

Every value is a screen pixel, taken from `cyd_ui.c` (the source wins if this file and the code
disagree). The numbers after "sim" are measured in the simulator frames (`tools/sim`), which render
the same code, so they show the real pixels. Layout changes: see the checkpoint notes in
`IMPROVEMENTS.md`.

## Vertical bands

| y | What |
|---|---|
| 0..39 | Header bar, 40 px, accent colour: title, campaign tag, link indicators. The mascot overlaps its right end (y 2..59). **Keep clear.** |
| 44..73 | State banner (x 6..245): state text and colour. |
| 76..79 | Progress bar (x 6..313), hidden when there is no progress. |
| 80..173 | Middle band: the card grid, or the scene, or the summary panel (x 6..313). The link diagram sits in the right half of row 2 (relay only). |
| 178..195 | Vehicle: statistics line (x 6..205, y 180) and sparkline (x 212..313, y 178..195). |
| 182..193 | Relay B only: stacked bar (x 6..313). It shares the row with the statistics line, but no screen shows both. |
| 200..236 | Hex lines, bottom-aligned (see below). |

## Widgets

### Header (y 0..39)

| Widget | Position | Size / font | Notes |
|---|---|---|---|
| Header container | (0, 0) | 320x40, accent colour | no border, no radius |
| Title | LEFT_MID, x 4 | font 20, white | the role name from `cyd_ui_cfg_t.title` |
| Campaign tag | LEFT_MID, x 108 | font 12, white | hidden unless set; the vehicle shows the run label, relay B `CHAIN` or `SELFTEST` |
| BLE dot | LEFT_MID, x 176 | 8x8 | green 0x1AA260 when up, red 0xD64545 when down; hidden until set |
| BLE label | LEFT_MID, x 187 | font 12, white | |
| ESP dot | LEFT_MID, x 212 | 8x8 | same colours; hidden until set |
| ESP label | LEFT_MID, x 223 | font 12, white | |

Sim: the dots and labels land about 13 px to the right of these values (LVGL's default header
inset; the code sets no padding). In the relay B frame the `ESP` text ends at x 258, the mascot
starts at x 265, and the `BLE` text ends at x 222 with the `ESP` dot at x 225. In the vehicle frame
`BASELINE` ends at x 181 and the BLE dot starts at x 189.

### Mascot (x 256..313, y 2..59)

| Widget | Position | Size | Notes |
|---|---|---|---|
| Mascot body | TOP_RIGHT, (-6, 2) | 58x58 | bobs 0..5 px upward (about 1.6 s period); one extra hop after `cyd_ui_pulse()` |
| Face | BOTTOM_MID | 44x38 | face colour 0xF4F4F4 |
| Eyes | CENTER, (-9, -3) and (+9, -3) | | blink; `expression` changes their shape |
| Mouth | CENTER, (0, +10) | 14x4 | |

Role details: key = ring 14x14 and stem 3x10 at the top; car = two wheels at the bottom (BOTTOM_MID,
x -12 and +12); relay = mast 3x16 and antenna tip at the top.

### State banner and progress

| Widget | Position | Size / font | Notes |
|---|---|---|---|
| State box | TOP_LEFT, (6, 44) | 240x30 | fill = the state colour (`cyd_ui_set_state`) |
| State label | LEFT_MID, x 8 inside the box | font 16, white | |
| Badge | TOP_LEFT, (136, 46) | font 14, white text on the colour passed to `cyd_ui_badge` | shown for the given time (1.2 s per round), over the right part of the banner |
| Progress bar | TOP_LEFT, (6, 76) | 308x4 | track 0x33333A, fill = accent |

### Card grid (y 80..173)

| Widget | Position | Size / font | Notes |
|---|---|---|---|
| Grid | TOP_LEFT, (4, 80) | 312x110, flex, 2 per row, gap 6 | wraps: row 1 at y 80, row 2 at y 130 |
| Card | column x 4 or x 160; row y 80 or y 130 | 150x44, bg 0x1E1E26 | |
| Card label | (0, 0) in the card | font 12, grey 0x8A8A9A | |
| Card value | BOTTOM_LEFT, (0, +2) in the card | font 20, white 0xF2F2F6 | |

Cards per role: vehicle 4 (row 1: Round, RTT; row 2: OK, Fail / TO); key 2 (row 1: Challenges,
Calc); relay A 3 (row 1: From tunnel, From key; row 2: Key); relay B 3 (row 1: From car, From
tunnel; row 2: Turnaround, or tunnel RTT in the self-test).

### Middle band alternatives (x 6..313, y 80..173)

| Widget | Position | Size / font | Notes |
|---|---|---|---|
| Link diagram | (160, 130) | 150x44 | relay only, hidden until the first pulse; uses the right half of row 2 |
| Link line | (12, 11) inside the diagram | 126x2, grey | |
| Node labels A, B | centred on the nodes | font 12 | |
| Link label | (0, 28) inside the diagram | width 150, font 12 | e.g. `challenge 22 ms` |
| Scene box | (6, 80) | 308x94 | `cyd_ui_scene`, shown for the given time |
| Scene label | (6, 94) | font 14, white | distance as `x.y m` |
| Car | left end of the scene box | | a static drawing; the key moves, the car does not |
| Key head | x = 110 + 1.5 × distance in m (clamped at 280) | | 0 m = x 110, 10 m = x 260 |
| Key stem | key head + 18 | | |
| Summary text | (6, 84) | font 14, 3 lines | `cyd_ui_summary_show`, shown for 5 s, then the histogram for 5 s |
| Histogram | (6, 80) | 308x60 | 10 bars, shares the box with the text |
| Histogram legend | (6, 146) | font 12 | |

### Vehicle only (y 178..195)

| Widget | Position | Size / font | Notes |
|---|---|---|---|
| Statistics line | TOP_LEFT, (6, 180) | width 200, font 12, clipped | `min 83.0  med 91.2  max 97.5 ms`; the sim shows it fits (ends before x 205) |
| Sparkline | (212, 178) | 102x18 | 30 rounds, line in tenths of a ms |
| Baseline band | (212, y) | 102 wide, translucent white | `BASELINE_BAND_MIN/MAX_US`; 0 = no band |
| Timeout marks | (212 + i × 102/30, 194) | 2x2, red | one per round slot of the last 30 |

### Relay B only (y 182..193)

| Widget | Position | Size / font | Notes |
|---|---|---|---|
| Stacked bar, tunnel part | (6, 182) | width proportional to the tunnel leg, 12 px high | tunnel = B turnaround − A turnaround |
| Stacked bar, A↔key part | (6 + tunnel, 182) | the rest of 308 px | the A↔key leg |

### Hex lines (footer)

| Widget | Position | Font | Notes |
|---|---|---|---|
| Hex line 0 | BOTTOM_LEFT, (6, bottom − 22) | font 12, grey | `nonce ...` or the `hex0` label |
| Hex line 1 | BOTTOM_LEFT, (6, bottom − 4) | font 12, grey | `resp ...` or the `hex1` label |

A full 16-byte value (38 characters) ends at about x 258–270 (measured). Font 14 would reach the
panel edge, which is why these lines use font 12.

### Boot splash (full screen, on the top layer)

| Widget | Position | Font | Notes |
|---|---|---|---|
| Background | (0, 0), 320x240 | | colour 0x101014 |
| Title | CENTER, (0, −18) | font 28 | role title |
| Subtitle | CENTER, (0, +22) | font 14 | `fw <commit>`; the `fw ` prefix is added by `cyd_ui_splash` |

## Fonts

Montserrat 12, 14, 16, 20 and 28 are enabled and used (`sdkconfig`). Text is ASCII only: the font has
no accented characters.

## Colours (`cyd_ui.c`)

| Name | Value | Use |
|---|---|---|
| `COL_BG` | 0x101014 | screen background |
| `COL_CARD` | 0x1E1E26 | cards |
| `COL_LABEL` | 0x8A8A9A | card labels, hex lines, link line |
| `COL_VALUE` | 0xF2F2F6 | card values, stats line |
| `COL_FACE` | 0xF4F4F4 | mascot face |
| `COL_INK` | 0x202028 | eyes, mouth |
| `COL_LINK_OK` | 0x1AA260 | link indicator up, OK badge |
| `COL_LINK_DOWN` | 0xD64545 | link indicator down |

The header and mascot take the role's accent colour from `cyd_ui_cfg_t.accent`.
