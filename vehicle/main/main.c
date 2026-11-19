/*
 * Vehicle (verifier) of the PKES relay bench.
 *
 * BLE central that reproduces the legitimate pairing with the key (project key/):
 *   scan -> connect -> discover service/characteristics -> enable notify ->
 *   [START command] -> write nonce -> check HMAC-SHA256(PSK, nonce) -> measure the RTT.
 *
 * The RTT measured here (challenge write -> arrival of the notify) is the HONEST BASELINE,
 * without a relay. It is the reference against which the cost of the relay is compared later.
 *
 * Commands over UART0 (the same port as the console and the OTA):
 *   RUN label=<label> dist=<meters>    tags the campaign (shown in the CSV and on the display)
 *   START                              runs the ROUNDS rounds (only after subscription); the first
 *                                      round goes out at a random offset within one period (M-19)
 * Without RUN the label is "run" and the distance "-", with a warning in the log.
 *
 * Result of each round (the result column of the CSV):
 *   OK       the response arrived within ROUND_TIMEOUT_MS and the HMAC matches
 *   FAIL     the response arrived within the deadline, but the HMAC does not match
 *   TIMEOUT  no response within ROUND_TIMEOUT_MS (rtt_us = -1)
 * A response arriving after the deadline is discarded: the round was already counted as TIMEOUT.
 *
 * CSV format (CSVHDR v2): label,dist_m,round,nonce,t_tx_us,rtt_us,result,rssi_dbm,itvl_1_25ms,mtu.
 * t_tx_us is the instant (us since boot) at which the challenge was sent.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

#include "ble_contract.h"
#include "ble_link_params.h"  /* explicit connection parameters (M-09) */
#include "run_meta.h"         /* RUN_META line at boot (M-13) */
#include "pkes_hmac.h"        /* HMAC shared with the key (key/) */
#include "cyd_display.h"
#include "cyd_led.h"
#include "cyd_ui.h"
#include "serial_ota.h"

#define ROUNDS       50     /* how many challenge-response rounds per campaign */
#define INTERVAL_MS  1000   /* period of the round grid (one round every INTERVAL_MS) */

/* Per-round deadline (M-01): without a response in this time, the round becomes TIMEOUT.
 * It must be well above the expected RTT (baseline ~90 ms, relay ~300-450 ms). */
#ifndef ROUND_TIMEOUT_MS
#define ROUND_TIMEOUT_MS  2000
#endif

/* Baseline band on the sparkline (V-07b), in microseconds. Fill in the min and max of the
 * measured baseline (the "min ... max" line of the summary). 0 = no band. */
#ifndef BASELINE_BAND_MIN_US
#define BASELINE_BAND_MIN_US  0
#endif
#ifndef BASELINE_BAND_MAX_US
#define BASELINE_BAND_MAX_US  0
#endif

/* Default campaign label, used when there is no RUN command. */
#ifndef RUN_LABEL
#define RUN_LABEL "run"
#endif

static const char *TAG = "VEHICLE";

static uint8_t  own_addr_type;
static uint16_t g_conn = BLE_HS_CONN_HANDLE_NONE;

/* Handles discovered on the current connection. */
static uint16_t svc_start, svc_end;
static uint16_t h_challenge, h_response, h_cccd;

/* State of the connection and of the campaign. subscribed = ready for START; running = campaign
 * in progress. Written by the host task, the esp_timer and the serial command. */
static volatile bool subscribed;
static volatile bool running;
static uint16_t conn_itvl;                /* negotiated interval, 1.25 ms (M-10) */
static uint16_t conn_mtu = 23;            /* negotiated ATT MTU (M-11) */
static uint32_t disconnects;              /* drops since boot (M-12) */

/* Campaign tag coming from the RUN command (M-14). */
static char run_label[24] = RUN_LABEL;
static char run_dist[16]  = "-";
static bool run_received;

/* State of the round in flight. round_open goes from true (challenge sent) to false when someone
 * closes the round: the notify (NimBLE host task) or the timeout (esp_timer task).
 * The mutex guarantees that only one of them closes the round. */
typedef enum { RES_OK, RES_FAIL, RES_TIMEOUT } round_res_t;
static SemaphoreHandle_t round_mux;
static bool     round_open;
static uint8_t  last_nonce[NONCE_LEN];
static int64_t  t_tx_us;
static esp_timer_handle_t round_timer, timeout_timer;

/* Counters and statistics (RTT in microseconds, answered rounds only). */
static int      round_i, ok_count, fail_count, timeout_count, answered_count;
static int64_t  rtt_min, rtt_max, rtt_sum;
static int64_t  s_ans[ROUNDS];    /* answered RTTs of the campaign, for the median (V-08) */
static int      s_n_ans;

/* Prototypes. */
static void start_scan(void);
static int  gap_event(struct ble_gap_event *event, void *arg);
static void send_challenge(void);
static void round_abort(const char *reason);

/* Accent color of the vehicle (blue) and the card indices in the UI. */
#define ACCENT 0x2962FF
enum { F_ROUND = 0, F_RTT = 1, F_OK = 2, F_FAIL = 3 };

static const char *res_str(round_res_t r)
{
    return r == RES_OK ? "OK" : r == RES_FAIL ? "FAIL" : "TIMEOUT";
}

/* Mode in the header (V-06): the RUN label in upper case, up to 8 ASCII characters. */
static void set_tag_label(void)
{
    char t[9];
    int n = 0;
    for (; n < 8 && run_label[n]; n++) {
        char c = run_label[n];
        t[n] = (c >= 'a' && c <= 'z') ? (char) (c - 32) : c;
    }
    t[n] = '\0';
    cyd_ui_set_tag(t);
}

/* Live statistics (V-08): min, median and max of the answered rounds, in ms with 1 decimal. */
static void fmt_ms1(char *buf, size_t cap, int64_t us)
{
    snprintf(buf, cap, "%lld.%01lld", (long long) (us / 1000), (long long) ((us % 1000) / 100));
}

static void update_stats_line(void)
{
    if (s_n_ans == 0) {
        cyd_ui_set_stats("min -  med -  max - ms");
        return;
    }
    int64_t s[ROUNDS];
    memcpy(s, s_ans, (size_t) s_n_ans * sizeof s[0]);
    for (int i = 1; i < s_n_ans; i++) {          /* insertion sort: 50 values, cost is irrelevant */
        int64_t v = s[i];
        int j = i - 1;
        while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
        s[j + 1] = v;
    }
    int64_t med = (s_n_ans % 2) ? s[s_n_ans / 2]
                                : (s[s_n_ans / 2 - 1] + s[s_n_ans / 2]) / 2;
    /* generous sizes: GCC checks the worst case of int64 and complains if it does not fit */
    char a[24], m[24], x[24], line[96];
    fmt_ms1(a, sizeof a, s[0]);
    fmt_ms1(m, sizeof m, med);
    fmt_ms1(x, sizeof x, s[s_n_ans - 1]);
    snprintf(line, sizeof line, "min %s  med %s  max %s ms", a, m, x);
    cyd_ui_set_stats(line);
}

/* ---- bench commands (RUN, START) ---------------------------------------- */

/* Copies the sorted answered RTTs into 's' (insertion sort; up to ROUNDS values). */
static void sorted_answered(int64_t *s)
{
    memcpy(s, s_ans, (size_t) s_n_ans * sizeof s[0]);
    for (int i = 1; i < s_n_ans; i++) {
        int64_t v = s[i];
        int j = i - 1;
        while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
        s[j + 1] = v;
    }
}

/* Final campaign summary (V-11): 3 lines of text and a 10-bin histogram between min and max. */
static void send_summary(void)
{
    char l1[96], l2[96], l3[64], txt[320], leg[96];
    snprintf(l1, sizeof l1, "OK %d/%d (%d%%)  fail %d  TO %d",
             ok_count, ROUNDS, ok_count * 100 / ROUNDS, fail_count, timeout_count);
    if (s_n_ans == 0) {
        snprintf(txt, sizeof txt, "%s\nno answers", l1);
        cyd_ui_summary_show(txt, NULL, 0, "");
        return;
    }
    int n = s_n_ans;
    int64_t s[ROUNDS];
    sorted_answered(s);
    int64_t med = (n % 2) ? s[n / 2] : (s[n / 2 - 1] + s[n / 2]) / 2;
    int64_t q1 = s[(n - 1) / 4];
    int64_t q3 = s[(n - 1) * 3 / 4];
    char mn[24], mx[24], md[24], a1[24], a3[24];
    fmt_ms1(mn, sizeof mn, s[0]);
    fmt_ms1(mx, sizeof mx, s[n - 1]);
    fmt_ms1(md, sizeof md, med);
    fmt_ms1(a1, sizeof a1, q1);
    fmt_ms1(a3, sizeof a3, q3);
    snprintf(l2, sizeof l2, "med %s  IQR %s to %s ms", md, a1, a3);
    snprintf(l3, sizeof l3, "min %s  max %s ms", mn, mx);
    snprintf(txt, sizeof txt, "%s\n%s\n%s", l1, l2, l3);

    uint16_t bins[10] = {0};
    int64_t w = (s[n - 1] - s[0]) / 10 + 1;
    for (int i = 0; i < n; i++) {
        int k = (int) ((s[i] - s[0]) / w);
        if (k > 9) k = 9;
        bins[k]++;
    }
    snprintf(leg, sizeof leg, "%s ms ... %s ms", mn, mx);
    cyd_ui_summary_show(txt, bins, 10, leg);
}

/* "3.0" -> 30, "3" -> 30, "0.5" -> 5 (only the first decimal digit). Anything else -> 0. */
static int dist_to_x10(const char *s)
{
    int integer = 0, tenth = 0;
    const char *p = s;
    while (*p >= '0' && *p <= '9') { integer = integer * 10 + (*p - '0'); p++; }
    if (*p == '.' || *p == ',') {
        p++;
        if (*p >= '0' && *p <= '9') tenth = *p - '0';
    }
    return integer * 10 + tenth;
}

/* Copies the value of "key=value" from a line, up to the next space. */
static bool kv_get(const char *line, const char *key, char *out, size_t cap)
{
    const char *p = strstr(line, key);
    if (!p) return false;
    p += strlen(key);
    size_t n = 0;
    while (p[n] && p[n] != ' ' && p[n] != '\r' && p[n] != '\n' && n + 1 < cap) {
        out[n] = p[n];
        n++;
    }
    out[n] = '\0';
    return n > 0;
}

static void start_campaign(void)
{
    if (running) { ESP_LOGW(TAG, "START ignored: campaign already running"); return; }
    if (!subscribed || h_challenge == 0) {
        ESP_LOGW(TAG, "START ignored: no key subscribed (wait for 'subscribed to notify')");
        return;
    }
    if (!run_received)
        ESP_LOGW(TAG, "RUN not sent: campaign uses the default label '%s' and dist=- (note it in the dataset)",
                 run_label);

    running = true;
    round_i = 0; ok_count = 0; fail_count = 0; timeout_count = 0; answered_count = 0;
    rtt_sum = 0; rtt_min = INT64_MAX; rtt_max = 0;
    s_n_ans = 0;
    update_stats_line();
    cyd_ui_summary_hide();                                                     /* V-11 */
    cyd_ui_spark_reset();                                                      /* V-07a */
    cyd_ui_spark_band(BASELINE_BAND_MIN_US, BASELINE_BAND_MAX_US);             /* V-07b */
    ESP_LOGI(TAG, "campaign started: label=%s dist=%s rounds=%d interval=%d ms timeout=%d ms",
             run_label, run_dist, ROUNDS, INTERVAL_MS, ROUND_TIMEOUT_MS);
    ESP_LOGI(TAG, "CSVHDR v2 label,dist_m,round,nonce,t_tx_us,rtt_us,result,rssi_dbm,itvl_1_25ms,mtu");
    cyd_ui_set_state("measuring", ACCENT);
    cyd_ui_set_progress(0, ROUNDS);

    /* Random start offset (M-19). The round grid is INTERVAL_MS, a multiple of the 50 ms connection
     * interval, so every round of a campaign keeps the same phase on the connection grid: one campaign
     * samples one phase, and the campaigns are not independent. Starting at a random point within one
     * period gives each campaign another phase. Drawn once per campaign; the rounds that follow keep
     * the INTERVAL_MS grid from this first send (finish_round). */
    uint32_t offset_us = esp_random() % ((uint32_t) INTERVAL_MS * 1000u);
    ESP_LOGI(TAG, "grid start offset %lu us (connection interval %u x 1.25 ms)",
             (unsigned long) offset_us, (unsigned) conn_itvl);
    esp_timer_stop(round_timer);                      /* safety: it must not be armed */
    if (esp_timer_start_once(round_timer, (uint64_t) offset_us) != ESP_OK) {
        ESP_LOGE(TAG, "round timer busy; sending round 1 now");
        send_challenge();
    }
}

static void bench_cmd(const char *line)
{
    if (strncmp(line, "RUN", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
        if (running) { ESP_LOGW(TAG, "RUN ignored: campaign in progress"); return; }
        char lbl[sizeof run_label], dst[sizeof run_dist];
        if (kv_get(line, "label=", lbl, sizeof lbl)) snprintf(run_label, sizeof run_label, "%s", lbl);
        if (kv_get(line, "dist=", dst, sizeof dst))  snprintf(run_dist, sizeof run_dist, "%s", dst);
        run_received = true;
        set_tag_label();
        if (strcmp(run_dist, "-") != 0) cyd_ui_scene(dist_to_x10(run_dist), 5000);   /* V-14 */
        ESP_LOGW(TAG, "RUN label=%s dist=%s", run_label, run_dist);
    } else if (strcmp(line, "START") == 0) {
        start_campaign();
    }
}

/* ---- closing a round (log, CSV, display, next round) -------------------- */

static void finish_round(round_res_t res, int64_t rtt, const uint8_t *nonce,
                         const uint8_t *got, uint16_t got_len)
{
    int8_t rssi = 0;                                  /* 0 = unavailable */
    if (g_conn != BLE_HS_CONN_HANDLE_NONE && ble_gap_conn_rssi(g_conn, &rssi) != 0) rssi = 0;

    switch (res) {
    case RES_OK:      ok_count++; break;
    case RES_FAIL:    fail_count++; break;
    case RES_TIMEOUT: timeout_count++; break;
    }
    if (res != RES_TIMEOUT) {
        answered_count++;
        if (rtt < rtt_min) rtt_min = rtt;
        if (rtt > rtt_max) rtt_max = rtt;
        rtt_sum += rtt;
        if (s_n_ans < ROUNDS) s_ans[s_n_ans++] = rtt;
    }

    if (res == RES_TIMEOUT) {
        ESP_LOGW(TAG, "round %d/%d: TIMEOUT  no response within %d ms  RSSI=%d dBm  nonce=%02x%02x%02x%02x",
                 round_i + 1, ROUNDS, ROUND_TIMEOUT_MS, rssi,
                 nonce[0], nonce[1], nonce[2], nonce[3]);
    } else {
        ESP_LOGI(TAG, "round %d/%d: %s  RTT=%lld us  RSSI=%d dBm  nonce=%02x%02x%02x%02x",
                 round_i + 1, ROUNDS, res_str(res), (long long) rtt, rssi,
                 nonce[0], nonce[1], nonce[2], nonce[3]);
    }
    /* One parseable line per round (columns in CSVHDR v2). rtt_us = -1 on TIMEOUT. */
    ESP_LOGI(TAG, "CSV,%s,%s,%d,%02x%02x%02x%02x,%lld,%lld,%s,%d,%u,%u",
             run_label, run_dist, round_i + 1,
             nonce[0], nonce[1], nonce[2], nonce[3],
             (long long) t_tx_us,
             (long long) (res == RES_TIMEOUT ? -1 : rtt),
             res_str(res), rssi, (unsigned) conn_itvl, (unsigned) conn_mtu);

    /* Mirror the round on the display. */
    int done = round_i + 1;
    char rtt_txt[24];
    cyd_ui_set_field(F_ROUND, "%d/%d", done, ROUNDS);
    if (res == RES_TIMEOUT) {
        cyd_ui_set_field(F_RTT, "timeout");
    } else {
        cyd_fmt_us(rtt_txt, sizeof rtt_txt, rtt);          /* V-02: "91.2 ms" */
        cyd_ui_set_field(F_RTT, "%s", rtt_txt);
    }
    cyd_ui_set_field(F_OK, "%d (%d%%)", ok_count, ok_count * 100 / done);   /* V-05 */
    cyd_ui_set_field(F_FAIL, "%d / %d", fail_count, timeout_count);
    cyd_ui_set_progress(done, ROUNDS);                                         /* V-05 */
    update_stats_line();                                                       /* V-08 */
    cyd_ui_set_hex(0, nonce, NONCE_LEN);
    cyd_ui_set_hex(1, got, got_len);
    cyd_ui_pulse();
    /* V-09: mascot expression and result badge; V-07a: one point on the sparkline. */
    cyd_ui_expression(res == RES_OK ? CYD_EXPR_OK : res == RES_FAIL ? CYD_EXPR_FAIL
                                                                    : CYD_EXPR_TIMEOUT, 1500);
    if (res == RES_TIMEOUT) {
        cyd_ui_badge("TIMEOUT", 0xB8860B, 1200);
    } else {
        char badge[32];
        snprintf(badge, sizeof badge, "%s %s", rtt_txt, res == RES_OK ? "OK" : "FAIL");
        cyd_ui_badge(badge, res == RES_OK ? 0x1AA260 : 0xD64545, 1200);
    }
    cyd_ui_spark_push(res == RES_TIMEOUT ? -1 : rtt);

    /* LED by result (V-10): green for OK, red for FAIL, amber for TIMEOUT. */
    cyd_ui_flash(res == RES_OK ? 0x1AA260 : res == RES_FAIL ? 0xD64545 : 0xF4B400, 1200);

    round_i++;
    if (round_i < ROUNDS) {
        /* Fixed grid (M-16): the next challenge goes out INTERVAL_MS after the previous one, not
         * RTT + 1 s. If the round took longer than the interval, it goes out right away. */
        int64_t delay = (t_tx_us + (int64_t) INTERVAL_MS * 1000) - esp_timer_get_time();
        if (delay < 1000) delay = 1000;
        esp_timer_start_once(round_timer, (uint64_t) delay);
    } else {
        running = false;
        if (answered_count > 0) {
            ESP_LOGI(TAG,
                     "=== %s dist=%s n=%d ok=%d fail=%d timeout=%d disconnects=%lu | RTT us (answered): min=%lld avg=%lld max=%lld ===",
                     run_label, run_dist, ROUNDS, ok_count, fail_count, timeout_count,
                     (unsigned long) disconnects,
                     (long long) rtt_min, (long long) (rtt_sum / answered_count), (long long) rtt_max);
        } else {
            ESP_LOGI(TAG,
                     "=== %s dist=%s n=%d ok=%d fail=%d timeout=%d disconnects=%lu | RTT us (answered): n/a ===",
                     run_label, run_dist, ROUNDS, ok_count, fail_count, timeout_count,
                     (unsigned long) disconnects);
        }
        send_summary();                                                     /* V-11 */
        bool clean = (fail_count == 0 && timeout_count == 0);
        cyd_ui_set_state(clean ? "done: all OK" : "done: with failures",
                         clean ? 0x1AA260 : 0xD64545);
    }
}

/* Deadline expired without a response (M-01). Runs in the esp_timer task. */
static void timeout_cb(void *arg)
{
    uint8_t nonce[NONCE_LEN];
    xSemaphoreTake(round_mux, portMAX_DELAY);
    if (!round_open) { xSemaphoreGive(round_mux); return; }   /* the notify already closed it */
    round_open = false;
    memcpy(nonce, last_nonce, NONCE_LEN);
    xSemaphoreGive(round_mux);
    finish_round(RES_TIMEOUT, -1, nonce, NULL, 0);
}

/* Stops the round in flight without counting it (disconnection). */
static void round_abort(const char *reason)
{
    xSemaphoreTake(round_mux, portMAX_DELAY);
    bool was_open = round_open;
    round_open = false;
    xSemaphoreGive(round_mux);
    esp_timer_stop(timeout_timer);
    esp_timer_stop(round_timer);
    if (was_open)
        ESP_LOGW(TAG, "round %d/%d interrupted (%s); not counted", round_i + 1, ROUNDS, reason);
    if (running) {
        running = false;
        ESP_LOGW(TAG, "campaign interrupted (%s); send START again after reconnecting", reason);
    }
}

/* ---- GATT discovery (chained through callbacks) ------------------------- */

static int on_subscribe(uint16_t conn, const struct ble_gatt_error *err,
                        struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0) {
        ESP_LOGE(TAG, "notify subscription failed (status=%d)", err->status);
        return 0;
    }
    subscribed = true;
    ESP_LOGI(TAG, "subscribed to notify; ready. Send START to begin the campaign");
    cyd_ui_set_state("ready: send START", ACCENT);
    return 0;
}

static int on_disc_dsc(uint16_t conn, const struct ble_gatt_error *err,
                       uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    if (err->status == 0) {
        if (ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(0x2902)) == 0) {
            h_cccd = dsc->handle;
        }
    } else if (err->status == BLE_HS_EDONE) {
        if (h_cccd == 0) { ESP_LOGE(TAG, "CCCD (0x2902) missing"); return 0; }
        uint8_t enable[2] = {0x01, 0x00};
        ble_gattc_write_flat(conn, h_cccd, enable, sizeof enable, on_subscribe, NULL);
    } else {
        ESP_LOGE(TAG, "disc dsc error=%d", err->status);
    }
    return 0;
}

static int on_disc_chr_response(uint16_t conn, const struct ble_gatt_error *err,
                                const struct ble_gatt_chr *chr, void *arg)
{
    if (err->status == 0) {
        h_response = chr->val_handle;
    } else if (err->status == BLE_HS_EDONE) {
        if (h_response == 0) { ESP_LOGE(TAG, "RESPONSE characteristic missing"); return 0; }
        ble_gattc_disc_all_dscs(conn, h_response, svc_end, on_disc_dsc, NULL);
    } else {
        ESP_LOGE(TAG, "disc chr response error=%d", err->status);
    }
    return 0;
}

static int on_disc_chr_challenge(uint16_t conn, const struct ble_gatt_error *err,
                                 const struct ble_gatt_chr *chr, void *arg)
{
    if (err->status == 0) {
        h_challenge = chr->val_handle;
    } else if (err->status == BLE_HS_EDONE) {
        if (h_challenge == 0) { ESP_LOGE(TAG, "CHALLENGE characteristic missing"); return 0; }
        ble_gattc_disc_chrs_by_uuid(conn, svc_start, svc_end,
                                    &CHR_RESPONSE_UUID.u, on_disc_chr_response, NULL);
    } else {
        ESP_LOGE(TAG, "disc chr challenge error=%d", err->status);
    }
    return 0;
}

static int on_disc_svc(uint16_t conn, const struct ble_gatt_error *err,
                       const struct ble_gatt_svc *svc, void *arg)
{
    if (err->status == 0) {
        svc_start = svc->start_handle;
        svc_end   = svc->end_handle;
    } else if (err->status == BLE_HS_EDONE) {
        if (svc_start == 0) { ESP_LOGE(TAG, "key service not found"); return 0; }
        ESP_LOGI(TAG, "service at %u..%u", svc_start, svc_end);
        ble_gattc_disc_chrs_by_uuid(conn, svc_start, svc_end,
                                    &CHR_CHALLENGE_UUID.u, on_disc_chr_challenge, NULL);
    } else {
        ESP_LOGE(TAG, "disc svc error=%d", err->status);
    }
    return 0;
}

/* ---- challenge-response round ------------------------------------------- */

static int on_write_challenge(uint16_t conn, const struct ble_gatt_error *err,
                              struct ble_gatt_attr *attr, void *arg)
{
    /* A write error does not close the round here: the timeout takes care of it. */
    if (err->status != 0) ESP_LOGE(TAG, "challenge write error=%d", err->status);
    return 0;
}

static void send_challenge(void)
{
    if (h_challenge == 0) return;

    xSemaphoreTake(round_mux, portMAX_DELAY);
    if (g_conn == BLE_HS_CONN_HANDLE_NONE || round_open) {
        xSemaphoreGive(round_mux);
        return;
    }
    esp_fill_random(last_nonce, NONCE_LEN);
    t_tx_us = esp_timer_get_time();
    round_open = true;
    esp_timer_stop(timeout_timer);   /* safety: it must not be armed */
    esp_timer_start_once(timeout_timer, (uint64_t) ROUND_TIMEOUT_MS * 1000);
    uint16_t conn = g_conn;
    uint8_t nonce[NONCE_LEN];
    memcpy(nonce, last_nonce, NONCE_LEN);
    int64_t t0 = t_tx_us;
    xSemaphoreGive(round_mux);

    ESP_LOGI(TAG, "HOP V tx_chal nonce=%02x%02x%02x%02x t=%lld us",
             nonce[0], nonce[1], nonce[2], nonce[3], (long long) t0);
    int rc = ble_gattc_write_flat(conn, h_challenge, nonce, NONCE_LEN,
                                  on_write_challenge, NULL);
    if (rc != 0) ESP_LOGE(TAG, "ble_gattc_write_flat rc=%d", rc);
}

static void round_timer_cb(void *arg) { send_challenge(); }

static void handle_notify(struct ble_gap_event *event)
{
    if (event->notify_rx.attr_handle != h_response) return;

    int64_t t_rx = esp_timer_get_time();
    uint8_t got[RESPONSE_LEN] = {0};
    uint16_t got_len = 0;
    ble_hs_mbuf_to_flat(event->notify_rx.om, got, RESPONSE_LEN, &got_len);

    uint8_t nonce[NONCE_LEN];
    int64_t rtt;
    xSemaphoreTake(round_mux, portMAX_DELAY);
    if (!round_open) {
        /* Round already closed by the timeout: a late response, discarded. */
        xSemaphoreGive(round_mux);
        ESP_LOGW(TAG, "response past the deadline (round already marked TIMEOUT); discarded");
        return;
    }
    round_open = false;
    rtt = t_rx - t_tx_us;
    memcpy(nonce, last_nonce, NONCE_LEN);
    xSemaphoreGive(round_mux);
    esp_timer_stop(timeout_timer);

    uint8_t expected[32];
    pkes_hmac_response(PSK, sizeof PSK, nonce, NONCE_LEN, expected);
    bool ok = (got_len == RESPONSE_LEN) && (memcmp(got, expected, RESPONSE_LEN) == 0);

    finish_round(ok ? RES_OK : RES_FAIL, rtt, nonce, got, got_len);
}

/* ---- GAP: scan, connection, notify -------------------------------------- */

static bool adv_has_service(const uint8_t *data, uint8_t len)
{
    struct ble_hs_adv_fields f;
    if (ble_hs_adv_parse_fields(&f, data, len) != 0) return false;
    for (int i = 0; i < f.num_uuids128; i++) {
        if (ble_uuid_cmp(&f.uuids128[i].u, &SVC_UUID.u) == 0) return true;
    }
    return false;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_DISC:
        if (!adv_has_service(event->disc.data, event->disc.length_data)) break;
        ESP_LOGI(TAG, "key found, connecting...");
        ble_gap_disc_cancel();
        {
            struct ble_gap_conn_params cp = ble_link_conn_params();   /* M-09b */
            if (ble_gap_connect(own_addr_type, &event->disc.addr, 30000, &cp,
                                gap_event, NULL) != 0) {
                ESP_LOGE(TAG, "ble_gap_connect failed; back to scanning");
                start_scan();
            }
        }
        break;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn = event->connect.conn_handle;
            svc_start = svc_end = 0;
            h_challenge = h_response = h_cccd = 0;
            subscribed = false;
            ESP_LOGI(TAG, "connected (handle %u); discovering service", g_conn);
            cyd_ui_set_link(CYD_LINK_BLE, 1);                                   /* V-06 */
            conn_itvl = ble_link_log_conn(TAG, g_conn);                        /* M-10 */
            ble_gattc_exchange_mtu(g_conn, ble_link_mtu_exchange_cb, NULL);   /* M-11 */
            cyd_ui_set_state("connected", 0x1AA260);
            ble_gattc_disc_svc_by_uuid(g_conn, &SVC_UUID.u, on_disc_svc, NULL);
        } else {
            ESP_LOGE(TAG, "connection failed (status=%d)", event->connect.status);
            start_scan();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        disconnects++;                                                       /* M-12 */
        ESP_LOGW(TAG, "disconnected (reason=%d, disconnects=%lu)",
                 event->disconnect.reason, (unsigned long) disconnects);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        subscribed = false;
        cyd_ui_set_link(CYD_LINK_BLE, 0);                                    /* V-06 */
        round_abort("disconnection");
        start_scan();
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:                                          /* M-10 */
        if (event->conn_update.status == 0)
            conn_itvl = ble_link_log_conn(TAG, event->conn_update.conn_handle);
        break;

    case BLE_GAP_EVENT_MTU:                                                  /* M-11 */
        conn_mtu = event->mtu.value;
        ble_link_log_mtu(TAG, event->mtu.conn_handle, event->mtu.value);
        break;

    case BLE_GAP_EVENT_NOTIFY_RX:
        handle_notify(event);
        break;

    default:
        break;
    }
    return 0;
}

static void start_scan(void)
{
    struct ble_gap_disc_params dp = {0};
    dp.filter_duplicates = 1;
    dp.passive = 0;
    int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &dp, gap_event, NULL);
    if (rc != 0) ESP_LOGE(TAG, "ble_gap_disc rc=%d", rc);
    else { ESP_LOGI(TAG, "scanning for the key service..."); cyd_ui_set_state("scanning", ACCENT); }
}

/* ---- host lifecycle ---------------------------------------------------- */

static void on_sync(void)
{
    if (ble_hs_util_ensure_addr(0) != 0) { ESP_LOGE(TAG, "no BLE address"); return; }
    if (ble_hs_id_infer_auto(0, &own_addr_type) != 0) { ESP_LOGE(TAG, "address inference failed"); return; }
    start_scan();
}

static void on_reset(int reason) { ESP_LOGE(TAG, "BLE reset (reason=%d)", reason); }

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    pkes_hmac_init();
    run_meta_print("vehicle");                                               /* M-13 */
    serial_ota_init();   /* reflash over UART0 without download mode (tools/serial_ota.py) */
    serial_ota_set_cmd_handler(bench_cmd);                                   /* M-14 / M-15 */

    /* Display/UI first, so the screen lights up even before BLE. */
    cyd_display_init();
    cyd_led_init();
    cyd_display_set_backlight(90);
    cyd_ui_cfg_t cfg = {
        .title  = "VEHICLE",
        .accent = ACCENT,
        .mascot = CYD_MASCOT_CAR,
        .fields = { "Round", "RTT", "OK", "Fail / TO", NULL },
        .hex0   = "nonce",
        .hex1   = "resp",
    };
    cyd_ui_init(&cfg);
    set_tag_label();                      /* V-06: mode = default label until RUN */
    cyd_ui_set_link(CYD_LINK_BLE, 0);     /* V-06: no connection yet */
    cyd_ui_splash(run_meta_commit(), 2000);                                    /* V-15 */

    round_mux = xSemaphoreCreateMutex();
    if (round_mux == NULL) { ESP_LOGE(TAG, "no round mutex"); return; }

    ret = nimble_port_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init rc=%d", ret); return; }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;

    const esp_timer_create_args_t ta = { .callback = round_timer_cb, .name = "round" };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &round_timer));
    const esp_timer_create_args_t tt = { .callback = timeout_cb, .name = "timeout" };
    ESP_ERROR_CHECK(esp_timer_create(&tt, &timeout_timer));

    nimble_port_freertos_init(host_task);
}
