// PKES simulator HAL. It replaces only the parts of the CYD stack that need hardware, so the
// real components/cyd/cyd_ui.c runs on the PC with LVGL 8.3 and a virtual clock:
//   - cyd_display_*: an LVGL display whose flush copies the pixels into a 320x240 framebuffer;
//   - cyd_led_*:     records the LED calls (printed with verbose mode, with the virtual time);
//   - esp_timer_*:   a short timer list, fired by sim_advance_ms().
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "cyd_display.h"
#include "cyd_led.h"
#include "esp_timer.h"
#include "sim_hal.h"

#define SIM_W       320
#define SIM_H       240
#define DRAW_LINES  40   // same as cyd_display.c
#define TICK_MS     5

int64_t g_sim_us;
int g_sim_verbose;

static uint8_t s_fb[SIM_W * SIM_H * 3];   // RGB888, top-left origin

// ---- esp_timer -------------------------------------------------------------------------------

struct esp_timer {
    esp_timer_cb_t cb;
    void *arg;
    int64_t due;      // < 0 = stopped
    int64_t period;   // 0 = one shot
};

#define MAX_TIMERS 32
static struct esp_timer s_timers[MAX_TIMERS];
static int s_ntimers;

int64_t esp_timer_get_time(void) { return g_sim_us; }

esp_err_t esp_timer_create(const esp_timer_create_args_t *a, esp_timer_handle_t *out) {
    if (s_ntimers >= MAX_TIMERS) return ESP_FAIL;
    struct esp_timer *t = &s_timers[s_ntimers++];
    t->cb = a->callback;
    t->arg = a->arg;
    t->due = -1;
    t->period = 0;
    *out = t;
    return ESP_OK;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t us) {
    t->due = g_sim_us + (int64_t)us;
    t->period = 0;
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t us) {
    t->due = g_sim_us + (int64_t)us;
    t->period = (int64_t)us;
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t t) { t->due = -1; return ESP_OK; }

esp_err_t esp_timer_delete(esp_timer_handle_t t) { t->due = -1; t->cb = NULL; return ESP_OK; }

static void fire_timers(void) {
    for (int i = 0; i < s_ntimers; i++) {
        struct esp_timer *t = &s_timers[i];
        if (t->cb && t->due >= 0 && t->due <= g_sim_us) {
            t->due = t->period ? t->due + t->period : -1;
            t->cb(t->arg);
        }
    }
}

// ---- display ---------------------------------------------------------------------------------

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t      s_disp_drv;
static lv_color_t         s_buf1[SIM_W * DRAW_LINES];
static lv_color_t         s_buf2[SIM_W * DRAW_LINES];

// Copies one area (rows are contiguous in the color buffer) into the framebuffer.
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            lv_color32_t c;
            c.full = lv_color_to32(*px++);
            uint8_t *p = &s_fb[(y * SIM_W + x) * 3];
            p[0] = c.ch.red;
            p[1] = c.ch.green;
            p[2] = c.ch.blue;
        }
    }
    lv_disp_flush_ready(drv);
}

void cyd_display_init(void) {
    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, SIM_W * DRAW_LINES);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = SIM_W;
    s_disp_drv.ver_res  = SIM_H;
    s_disp_drv.flush_cb = flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);
}

void cyd_display_lock(void) {}
void cyd_display_unlock(void) {}
void cyd_display_set_backlight(uint8_t percent) { (void)percent; }

// ---- LED -------------------------------------------------------------------------------------

void cyd_led_init(void) {}

void cyd_led_set(cyd_led_mode_t mode, uint32_t rgb) {
    static const char *names[] = { "off", "breathe", "blink", "solid" };
    if (g_sim_verbose)
        printf("  [%7.3f s] led %s %06x\n", (double)g_sim_us / 1e6, names[mode], rgb);
}

// ---- virtual clock and output ----------------------------------------------------------------

void sim_advance_ms(int ms) {
    for (int t = 0; t < ms; t += TICK_MS) {
        g_sim_us += TICK_MS * 1000;
        lv_tick_inc(TICK_MS);
        fire_timers();
        lv_timer_handler();
    }
}

static void put16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }

int sim_write_bmp(const char *path) {
    lv_refr_now(NULL);   // make sure the framebuffer holds the current frame

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    const uint32_t row = SIM_W * 3;              // already a multiple of 4
    const uint32_t img = row * SIM_H;
    uint8_t hdr[54];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B';
    hdr[1] = 'M';
    put32(hdr + 2, 54 + img);
    put32(hdr + 10, 54);
    put32(hdr + 14, 40);
    put32(hdr + 18, SIM_W);
    put32(hdr + 22, SIM_H);                      // positive height: rows bottom-up
    put16(hdr + 26, 1);
    put16(hdr + 28, 24);
    put32(hdr + 34, img);
    put32(hdr + 38, 2835);
    put32(hdr + 42, 2835);
    fwrite(hdr, 1, sizeof(hdr), f);

    uint8_t *line = malloc(row);
    if (!line) { fclose(f); return -1; }
    for (int y = SIM_H - 1; y >= 0; y--) {
        for (int x = 0; x < SIM_W; x++) {
            const uint8_t *p = &s_fb[(y * SIM_W + x) * 3];
            line[x * 3 + 0] = p[2];              // BMP stores B, G, R
            line[x * 3 + 1] = p[1];
            line[x * 3 + 2] = p[0];
        }
        fwrite(line, 1, row, f);
    }
    free(line);
    fclose(f);
    return 0;
}
