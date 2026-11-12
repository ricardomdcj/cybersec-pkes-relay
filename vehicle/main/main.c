/*
 * Veiculo (verificador) da bancada PKES relay.
 *
 * Central BLE que reproduz o par legitimo com a chave (app phoneaskey):
 *   varre -> conecta -> descobre servico/caracteristicas -> habilita notify ->
 *   escreve nonce -> confere HMAC-SHA256(PSK, nonce) -> mede o RTT.
 *
 * O RTT medido aqui (escrita do desafio -> chegada da notify) e a LINHA DE BASE
 * honesta, sem relay. E contra ela que vamos comparar o custo do relay depois.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "mbedtls/md.h"

#include "ble_contract.h"

#define ROUNDS       20     /* quantas rodadas de desafio-resposta por conexao */
#define INTERVAL_MS  1000   /* intervalo entre rodadas */

static const char *TAG = "VEICULO";

static uint8_t  own_addr_type;
static uint16_t g_conn = BLE_HS_CONN_HANDLE_NONE;

/* Handles descobertos na conexao atual. */
static uint16_t svc_start, svc_end;
static uint16_t h_challenge, h_response, h_cccd;

/* Estado da rodada e estatisticas de RTT (em microssegundos). */
static uint8_t  last_nonce[NONCE_LEN];
static int64_t  t_tx_us;
static int      round_i, ok_count, fail_count;
static int64_t  rtt_min, rtt_max, rtt_sum;
static esp_timer_handle_t round_timer;

/* Prototipos. */
static void start_scan(void);
static int  gap_event(struct ble_gap_event *event, void *arg);
static void send_challenge(void);

/* ---- cripto ------------------------------------------------------------- */

static void hmac_sha256(const uint8_t *key, size_t kl,
                        const uint8_t *msg, size_t ml, uint8_t out[32])
{
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(info, key, kl, msg, ml, out);
}

/* ---- descoberta GATT (encadeada por callbacks) -------------------------- */

static int on_subscribe(uint16_t conn, const struct ble_gatt_error *err,
                        struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0) {
        ESP_LOGE(TAG, "falha ao inscrever em notify (status=%d)", err->status);
        return 0;
    }
    ESP_LOGI(TAG, "inscrito em notify; iniciando %d rodadas", ROUNDS);
    round_i = 0; ok_count = 0; fail_count = 0;
    rtt_sum = 0; rtt_min = INT64_MAX; rtt_max = 0;
    send_challenge();
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
        if (h_cccd == 0) { ESP_LOGE(TAG, "CCCD (0x2902) ausente"); return 0; }
        uint8_t enable[2] = {0x01, 0x00};
        ble_gattc_write_flat(conn, h_cccd, enable, sizeof enable, on_subscribe, NULL);
    } else {
        ESP_LOGE(TAG, "disc dsc erro=%d", err->status);
    }
    return 0;
}

static int on_disc_chr_response(uint16_t conn, const struct ble_gatt_error *err,
                                const struct ble_gatt_chr *chr, void *arg)
{
    if (err->status == 0) {
        h_response = chr->val_handle;
    } else if (err->status == BLE_HS_EDONE) {
        if (h_response == 0) { ESP_LOGE(TAG, "caracteristica RESPONSE ausente"); return 0; }
        ble_gattc_disc_all_dscs(conn, h_response, svc_end, on_disc_dsc, NULL);
    } else {
        ESP_LOGE(TAG, "disc chr response erro=%d", err->status);
    }
    return 0;
}

static int on_disc_chr_challenge(uint16_t conn, const struct ble_gatt_error *err,
                                 const struct ble_gatt_chr *chr, void *arg)
{
    if (err->status == 0) {
        h_challenge = chr->val_handle;
    } else if (err->status == BLE_HS_EDONE) {
        if (h_challenge == 0) { ESP_LOGE(TAG, "caracteristica CHALLENGE ausente"); return 0; }
        ble_gattc_disc_chrs_by_uuid(conn, svc_start, svc_end,
                                    &CHR_RESPONSE_UUID.u, on_disc_chr_response, NULL);
    } else {
        ESP_LOGE(TAG, "disc chr challenge erro=%d", err->status);
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
        if (svc_start == 0) { ESP_LOGE(TAG, "servico da chave nao encontrado"); return 0; }
        ESP_LOGI(TAG, "servico em %u..%u", svc_start, svc_end);
        ble_gattc_disc_chrs_by_uuid(conn, svc_start, svc_end,
                                    &CHR_CHALLENGE_UUID.u, on_disc_chr_challenge, NULL);
    } else {
        ESP_LOGE(TAG, "disc svc erro=%d", err->status);
    }
    return 0;
}

/* ---- rodada de desafio-resposta ----------------------------------------- */

static int on_write_challenge(uint16_t conn, const struct ble_gatt_error *err,
                              struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0) ESP_LOGE(TAG, "escrita do desafio erro=%d", err->status);
    return 0;
}

static void send_challenge(void)
{
    if (g_conn == BLE_HS_CONN_HANDLE_NONE || h_challenge == 0) return;
    esp_fill_random(last_nonce, NONCE_LEN);
    t_tx_us = esp_timer_get_time();
    int rc = ble_gattc_write_flat(g_conn, h_challenge, last_nonce, NONCE_LEN,
                                  on_write_challenge, NULL);
    if (rc != 0) ESP_LOGE(TAG, "ble_gattc_write_flat rc=%d", rc);
}

static void round_timer_cb(void *arg) { send_challenge(); }

static void handle_notify(struct ble_gap_event *event)
{
    if (event->notify_rx.attr_handle != h_response) return;

    int64_t rtt = esp_timer_get_time() - t_tx_us;

    uint8_t got[RESPONSE_LEN] = {0};
    uint16_t got_len = 0;
    ble_hs_mbuf_to_flat(event->notify_rx.om, got, RESPONSE_LEN, &got_len);

    uint8_t expected[32];
    hmac_sha256(PSK, sizeof PSK, last_nonce, NONCE_LEN, expected);
    bool ok = (got_len == RESPONSE_LEN) && (memcmp(got, expected, RESPONSE_LEN) == 0);

    if (ok) ok_count++; else fail_count++;
    if (rtt < rtt_min) rtt_min = rtt;
    if (rtt > rtt_max) rtt_max = rtt;
    rtt_sum += rtt;

    ESP_LOGI(TAG, "rodada %d/%d: %s  RTT=%lld us",
             round_i + 1, ROUNDS, ok ? "OK" : "FALHA", (long long) rtt);

    round_i++;
    if (round_i < ROUNDS) {
        esp_timer_start_once(round_timer, (uint64_t) INTERVAL_MS * 1000);
    } else {
        ESP_LOGI(TAG,
                 "=== BASELINE n=%d ok=%d falha=%d | RTT us: min=%lld avg=%lld max=%lld ===",
                 round_i, ok_count, fail_count,
                 (long long) rtt_min, (long long) (rtt_sum / round_i), (long long) rtt_max);
    }
}

/* ---- GAP: scan, conexao, notify ----------------------------------------- */

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
        ESP_LOGI(TAG, "chave encontrada, conectando...");
        ble_gap_disc_cancel();
        if (ble_gap_connect(own_addr_type, &event->disc.addr, 30000, NULL,
                            gap_event, NULL) != 0) {
            ESP_LOGE(TAG, "ble_gap_connect falhou; volto a escanear");
            start_scan();
        }
        break;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn = event->connect.conn_handle;
            svc_start = svc_end = 0;
            h_challenge = h_response = h_cccd = 0;
            ESP_LOGI(TAG, "conectado (handle %u); descobrindo servico", g_conn);
            ble_gattc_disc_svc_by_uuid(g_conn, &SVC_UUID.u, on_disc_svc, NULL);
        } else {
            ESP_LOGE(TAG, "conexao falhou (status=%d)", event->connect.status);
            start_scan();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "desconectado (reason=%d)", event->disconnect.reason);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        start_scan();
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
    else ESP_LOGI(TAG, "escaneando pelo servico da chave...");
}

/* ---- ciclo de vida do host ---------------------------------------------- */

static void on_sync(void)
{
    if (ble_hs_util_ensure_addr(0) != 0) { ESP_LOGE(TAG, "sem endereco BLE"); return; }
    if (ble_hs_id_infer_auto(0, &own_addr_type) != 0) { ESP_LOGE(TAG, "infer addr falhou"); return; }
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

    ret = nimble_port_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init rc=%d", ret); return; }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;

    const esp_timer_create_args_t ta = { .callback = round_timer_cb, .name = "round" };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &round_timer));

    nimble_port_freertos_init(host_task);
}
