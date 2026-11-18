#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Status UI shared by every role on the bench (key, vehicle, relay A/B).
 *
 * A single view-only screen (no touch), in the functional-clean style of deskrab:
 *   - a colored header with the role name and an animated mascot in the corner;
 *   - a large banner with the current STATE (its color is mirrored on the RGB LED);
 *   - a grid of label/value cards (up to 6) with the role's metrics;
 *   - up to two hex lines (nonce / response) in the footer.
 *
 * The mascot represents the device (key, car or relay antenna) and has basic animations:
 * gentle bobbing, eye blinks, and a "hop" on every cyd_ui_pulse() call (made on each round
 * or event, so the screen feels alive).
 *
 * All functions are safe to call from any task: they take the LVGL lock.
 */

typedef enum {
    CYD_MASCOT_KEY,     // digital key
    CYD_MASCOT_CAR,     // vehicle
    CYD_MASCOT_RELAY,   // relay (antenna)
} cyd_mascot_t;

typedef struct {
    const char  *title;      // e.g. "VEHICLE"
    uint32_t     accent;     // header/mascot color, 0xRRGGBB
    cyd_mascot_t mascot;
    const char  *fields[6];  // card labels; use NULL to end before 6
    const char  *hex0;       // label of the 1st hex line (NULL = no hex line)
    const char  *hex1;       // label of the 2nd hex line (NULL = only one)
} cyd_ui_cfg_t;

// Brings up the screen. Call after cyd_display_init() / cyd_led_init().
void cyd_ui_init(const cyd_ui_cfg_t *cfg);

// State banner + color; the color drives the LED (breathing) and tints the banner.
void cyd_ui_set_state(const char *text, uint32_t color);

// Value of card idx (0..n-1), printf-style.
void cyd_ui_set_field(int idx, const char *fmt, ...);

// Shows 'n' bytes as hex on line idx (0 or 1). data NULL or n<=0 clears the line.
void cyd_ui_set_hex(int idx, const uint8_t *data, int n);

// "Hop" the mascot (reaction to an event, e.g. a round finished).
void cyd_ui_pulse(void);

// Formats microseconds for reading: "91.2 ms", "1.20 s", or "-" (us < 0).
// ASCII only (the screen font has no accented characters).
void cyd_fmt_us(char *buf, size_t cap, int64_t us);

// Blinks the LED in the given color for 'ms' milliseconds, then returns to the state color (V-10).
void cyd_ui_flash(uint32_t color, int ms);

// Thin progress bar just below the banner: cur of total. total <= 0 hides it.
void cyd_ui_set_progress(int cur, int total);

// Campaign mode in the header (e.g. "BASELINE", "RELAY"), up to 8 ASCII characters.
// NULL or "" hides it (V-06).
void cyd_ui_set_tag(const char *text);

// Link indicators in the header (V-06): BLE or ESP-NOW.
typedef enum { CYD_LINK_BLE = 0, CYD_LINK_ESPNOW = 1 } cyd_link_t;
// state: -1 hides the indicator; 0 = down (red); 1 = up (green).
void cyd_ui_set_link(cyd_link_t which, int state);

// Stats line below the cards (V-08), e.g. "min 83.0  med 91.2  max 97.5 ms".
// NULL clears the line.
void cyd_ui_set_stats(const char *text);

// Temporary mascot expression (V-09a): OK = closed eyes and a smile, FAIL = red eyes and a
// flat mouth, TIMEOUT = surprised. Returns to normal after 'ms' milliseconds.
typedef enum { CYD_EXPR_NORMAL = 0, CYD_EXPR_OK, CYD_EXPR_FAIL, CYD_EXPR_TIMEOUT } cyd_expr_t;
void cyd_ui_expression(cyd_expr_t e, int ms);

// Short text balloon over the banner (V-09b), in the given color, for 'ms' milliseconds.
void cyd_ui_badge(const char *text, uint32_t color, int ms);

// Sparkline of the last 30 rounds (V-07a). cyd_ui_spark_reset clears the series and shows the
// chart (call at the start of a campaign). cyd_ui_spark_push: us = RTT in microseconds;
// us < 0 = TIMEOUT (red mark on the baseline, no line point). cyd_ui_spark_band: reference
// band for the baseline (V-07b); hi <= lo turns it off.
void cyd_ui_spark_reset(void);
void cyd_ui_spark_push(int64_t us);
void cyd_ui_spark_band(int64_t lo_us, int64_t hi_us);

// V-11: final campaign summary. Alternates every 5 s between the text (3 lines separated by \n)
// and the histogram (up to 10 bars). legend: text below the histogram. Hides the card grid.
void cyd_ui_summary_show(const char *summary, const uint16_t *bins, int nbins, const char *legend);
// Shows the card grid again (start of a new campaign).
void cyd_ui_summary_hide(void);

// V-12: link between the two relays, in place of the last card (right half of row 2).
// dir 0 = B->A (challenge), dir 1 = A->B (response). label: text under the link; NULL keeps it.
void cyd_ui_link_pulse(int dir, const char *label);

// V-13: stacked bar on the stats line: a = tunnel part, b = A<->key part.
// a < 0 or b < 0 hides it.
void cyd_ui_stack_bar(int64_t a_us, int64_t b_us);

// V-14: car and key scene, with the key at dist_x10 tenths of a meter, for 'ms' milliseconds.
void cyd_ui_scene(int dist_x10, int ms);

// V-15: boot splash: role title and subtitle (e.g. the commit), for 'ms' milliseconds.
void cyd_ui_splash(const char *subtitle, int ms);
