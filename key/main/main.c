/*
 * key — the digital key of the PKES bench, now on a CYD (ESP32-2432S028R).
 *
 * Port of the old Android app phoneaskey/ (KeyPeripheral.kt) to ESP-IDF/NimBLE:
 * BLE peripheral + GATT server that advertises the key service, receives the nonce from
 * the vehicle (or Relay B) on the CHALLENGE characteristic, computes
 * HMAC-SHA256(PSK, nonce) truncated to RESPONSE_LEN, and notifies on RESPONSE.
 *
 * The GATT layout mirrors relays/main/relay_b.c (the key clone), but here the key is the
 * real one: it COMPUTES the HMAC (via the pkes_contract component) instead of tunneling it.
 *
 * The display shows the state (advertising/connected), how many challenges were answered,
 * the compute+notify time, and the last nonce/response in hex, with the key mascot in the corner.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ble_contract.h"
#include "ble_link_params.h"
#include "run_meta.h"
#include "pkes_hmac.h"
#include "cyd_display.h"
#include "cyd_led.h"
#include "cyd_ui.h"
#include "serial_ota.h"

static const char *TAG = "KEY";

/* Accent color of the key (amber). */
#define ACCENT 0xF4B400

static uint8_t  own_addr_type;
static uint16_t g_conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t h_response_val;
static uint8_t  last_response[RESPONSE_LEN];
static uint32_t challenge_count;
static uint32_t disconnects;      /* drops of the vehicle connection since boot (M-12) */

/* IDs of the UI fields (order of the labels in cfg.fields). */
enum { F_CHALLENGES = 0, F_CALC_US = 1 };

static void start_advertising(void);

/* ---- GATT server (mirrors KeyPeripheral.kt, now computing the HMAC) ---- */

static void answer_challenge(const uint8_t *nonce, uint16_t nlen)
{
    int64_t t0 = esp_timer_get_time();

    uint8_t full[32];
    pkes_hmac_response(PSK, sizeof PSK, nonce, NONCE_LEN, full);
    memcpy(last_response, full, RESPONSE_LEN);

    if (g_conn != BLE_HS_CONN_HANDLE_NONE) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(last_response, RESPONSE_LEN);
        if (om) {
            int rc = ble_gatts_notify_custom(g_conn, h_response_val, om);  /* consome om */
            if (rc != 0) ESP_LOGE(TAG, "notify rc=%d", rc);
        }
    }

    int64_t dt = esp_timer_get_time() - t0;
    challenge_count++;

    /* nonce (4 B) correlates with the logs of the vehicle and the relays; dt = compute+notify. */
    ESP_LOGI(TAG, "HOP K resp nonce=%02x%02x%02x%02x calc_notify=%lld us (challenge %lu, %u B) t=%lld us",
             nonce[0], nonce[1], nonce[2], nonce[3], (long long) dt,
             (unsigned long) challenge_count, nlen, (long long) esp_timer_get_time());

    cyd_ui_set_field(F_CHALLENGES, "%lu", (unsigned long) challenge_count);
    char calc_txt[24];                                                 /* V-02 */
    cyd_fmt_us(calc_txt, sizeof calc_txt, dt);
    cyd_ui_set_field(F_CALC_US, "%s", calc_txt);
    cyd_ui_set_hex(0, nonce, nlen);
    cyd_ui_set_hex(1, last_response, RESPONSE_LEN);
    cyd_ui_pulse();
}

static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (ble_uuid_cmp(ctxt->chr->uuid, &CHR_CHALLENGE_UUID.u) == 0) {
            uint8_t nonce[NONCE_LEN];
            uint16_t olen = 0;
            int rc = ble_hs_mbuf_to_flat(ctxt->om, nonce, sizeof nonce, &olen);
            if (rc != 0) return BLE_ATT_ERR_UNLIKELY;
            answer_challenge(nonce, olen);
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

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn = event->connect.conn_handle;
            ESP_LOGI(TAG, "vehicle connected (handle %u)", g_conn);
            ble_link_log_conn(TAG, g_conn);                                    /* M-10 */
            cyd_ui_set_state("connected", 0x1AA260);
            cyd_ui_set_link(CYD_LINK_BLE, 1);                             /* V-06 */
        } else {
            ESP_LOGE(TAG, "connection failed (status=%d); re-advertising", event->connect.status);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        disconnects++;                                                    /* M-12 */
        ESP_LOGW(TAG, "vehicle disconnected (reason=%d, disconnects=%lu); re-advertising",
                 event->disconnect.reason, (unsigned long) disconnects);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        cyd_ui_set_link(CYD_LINK_BLE, 0);                                 /* V-06 */
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

/* ---- host lifecycle ----------------------------------------------------- */

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t[]){ SVC_UUID };
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    const char *name = ble_svc_gap_device_name();
    struct ble_hs_adv_fields rsp = {0};
    rsp.name = (uint8_t *) name;
    rsp.name_len = strlen(name);
    rsp.name_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params advp = {0};
    advp.conn_mode = BLE_GAP_CONN_MODE_UND;
    advp.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &advp, gap_event, NULL);
    if (rc != 0) ESP_LOGE(TAG, "adv_start rc=%d", rc);
    else { ESP_LOGI(TAG, "advertising the key; waiting for the vehicle"); cyd_ui_set_state("advertising", ACCENT); }
}

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

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    pkes_hmac_init();
    run_meta_print("key");                                                /* M-13b */
    serial_ota_init();   /* reflash over UART0 without download mode (tools/serial_ota.py) */

    /* UI first, so the screen lights up even before BLE. */
    cyd_display_init();
    cyd_led_init();
    cyd_display_set_backlight(90);
    cyd_ui_cfg_t cfg = {
        .title  = "KEY",
        .accent = ACCENT,
        .mascot = CYD_MASCOT_KEY,
        .fields = { "Challenges", "Calc", NULL },
        .hex0   = "nonce",
        .hex1   = "resp",
    };
    cyd_ui_init(&cfg);
    cyd_ui_set_field(F_CHALLENGES, "0");
    cyd_ui_set_link(CYD_LINK_BLE, 0);                                     /* V-06: no vehicle yet */
    cyd_ui_splash(run_meta_commit(), 2000);                               /* V-15 */

    ret = nimble_port_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init rc=%d", ret); return; }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("pkes-key");

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "gatts_count_cfg rc=%d", rc); return; }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) { ESP_LOGE(TAG, "gatts_add_svcs rc=%d", rc); return; }

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "key started (BLE peripheral + CYD display)");
}
