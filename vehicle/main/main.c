/*
 * Veiculo (verificador) da bancada PKES relay.
 *
 * Central BLE que reproduz o par legitimo com a chave (projeto key/):
 *   varre -> conecta -> descobre servico/caracteristicas -> habilita notify ->
 *   [comando START] -> escreve nonce -> confere HMAC-SHA256(PSK, nonce) -> mede o RTT.
 *
 * O RTT medido aqui (escrita do desafio -> chegada da notify) e a LINHA DE BASE
 * honesta, sem relay. E contra ela que vamos comparar o custo do relay depois.
 *
 * Comandos pela UART0 (mesma porta do console e do OTA):
 *   RUN label=<rotulo> dist=<metros>   marca a campanha (aparece no CSV e no display)
 *   START                              inicia as ROUNDS rodadas (so apos a inscricao)
 * Sem RUN o rotulo fica "run" e a distancia "-", com aviso no log.
 *
 * Resultado de cada rodada (coluna resultado do CSV):
 *   OK       resposta chegou dentro de ROUND_TIMEOUT_MS e o HMAC confere
 *   FALHA    resposta chegou dentro do prazo, mas o HMAC nao confere
 *   TIMEOUT  nenhuma resposta dentro de ROUND_TIMEOUT_MS (rtt_us = -1)
 * Resposta que chega depois do prazo e descartada: a rodada ja foi contada como TIMEOUT.
 *
 * Formato do CSV (CSVHDR v2): label,dist_m,rodada,nonce,t_tx_us,rtt_us,resultado,
 * rssi_dbm,itvl_1_25ms,mtu. t_tx_us e o instante (us desde o boot) do envio do desafio.
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
#include "ble_link_params.h"  /* parametros de conexao explicitos (M-09) */
#include "run_meta.h"         /* linha RUN_META no boot (M-13) */
#include "pkes_hmac.h"        /* HMAC compartilhado com a chave (key/) */
#include "cyd_display.h"
#include "cyd_led.h"
#include "cyd_ui.h"
#include "serial_ota.h"

#define ROUNDS       50     /* quantas rodadas de desafio-resposta por campanha */
#define INTERVAL_MS  1000   /* periodo da grade de rodadas (uma a cada INTERVAL_MS) */

/* Prazo por rodada (M-01): sem resposta nesse tempo, a rodada vira TIMEOUT.
 * Tem de ser bem maior que o RTT esperado (baseline ~90 ms, relay ~300-450 ms). */
#ifndef ROUND_TIMEOUT_MS
#define ROUND_TIMEOUT_MS  2000
#endif

/* Faixa do baseline na sparkline (V-07b), em microssegundos. Preencha com o min e o max do
 * baseline medido (linha "min ... max" do resumo). 0 = sem faixa. */
#ifndef BASELINE_BAND_MIN_US
#define BASELINE_BAND_MIN_US  0
#endif
#ifndef BASELINE_BAND_MAX_US
#define BASELINE_BAND_MAX_US  0
#endif

/* Rotulo padrao da campanha, usado se nao houver comando RUN. */
#ifndef RUN_LABEL
#define RUN_LABEL "run"
#endif

static const char *TAG = "VEICULO";

static uint8_t  own_addr_type;
static uint16_t g_conn = BLE_HS_CONN_HANDLE_NONE;

/* Handles descobertos na conexao atual. */
static uint16_t svc_start, svc_end;
static uint16_t h_challenge, h_response, h_cccd;

/* Estado da conexao e da campanha. subscribed = pronto para START; running = campanha em
 * andamento. Escritos pela task do host, pelo esp_timer e pelo comando serial. */
static volatile bool subscribed;
static volatile bool running;
static uint16_t conn_itvl;                /* intervalo negociado, 1.25 ms (M-10) */
static uint16_t conn_mtu = 23;            /* MTU ATT negociado (M-11) */
static uint32_t desconexoes;              /* quedas desde o boot (M-12) */

/* Marcacao da campanha vinda do comando RUN (M-14). */
static char run_label[24] = RUN_LABEL;
static char run_dist[16]  = "-";
static bool run_recebido;

/* Estado da rodada em voo. round_open vai de true (desafio enviado) para false quando
 * alguem fecha a rodada: a notify (task do host NimBLE) ou o timeout (task do esp_timer).
 * O mutex garante que so um dos dois fecha a rodada. */
typedef enum { RES_OK, RES_FALHA, RES_TIMEOUT } round_res_t;
static SemaphoreHandle_t round_mux;
static bool     round_open;
static uint8_t  last_nonce[NONCE_LEN];
static int64_t  t_tx_us;
static esp_timer_handle_t round_timer, timeout_timer;

/* Contadores e estatisticas (RTT em microssegundos, so rodadas respondidas). */
static int      round_i, ok_count, fail_count, timeout_count, answered_count;
static int64_t  rtt_min, rtt_max, rtt_sum;
static int64_t  s_ans[ROUNDS];    /* RTTs respondidos da campanha, para a mediana (V-08) */
static int      s_n_ans;

/* Prototipos. */
static void start_scan(void);
static int  gap_event(struct ble_gap_event *event, void *arg);
static void send_challenge(void);
static void round_abort(const char *motivo);

/* Cor de destaque do veiculo (azul) e indices dos cards na UI. */
#define ACCENT 0x2962FF
enum { F_RODADA = 0, F_RTT = 1, F_OK = 2, F_FALHA = 3 };

static const char *res_str(round_res_t r)
{
    return r == RES_OK ? "OK" : r == RES_FALHA ? "FALHA" : "TIMEOUT";
}

/* Modo no cabecalho (V-06): o rotulo do RUN em maiusculas, ate 8 caracteres ASCII. */
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

/* Estatistica ao vivo (V-08): min, mediana e max das rodadas respondidas, em ms com 1 casa. */
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
    for (int i = 1; i < s_n_ans; i++) {          /* insercao: 50 valores, custo irrelevante */
        int64_t v = s[i];
        int j = i - 1;
        while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
        s[j + 1] = v;
    }
    int64_t med = (s_n_ans % 2) ? s[s_n_ans / 2]
                                : (s[s_n_ans / 2 - 1] + s[s_n_ans / 2]) / 2;
    /* tamanhos folgados: o GCC avalia o pior caso de int64 e reclama se nao couber */
    char a[24], m[24], x[24], line[96];
    fmt_ms1(a, sizeof a, s[0]);
    fmt_ms1(m, sizeof m, med);
    fmt_ms1(x, sizeof x, s[s_n_ans - 1]);
    snprintf(line, sizeof line, "min %s  med %s  max %s ms", a, m, x);
    cyd_ui_set_stats(line);
}

/* ---- comandos de bancada (RUN, START) ----------------------------------- */

/* Copia o valor de "chave=valor" de uma linha, ate o proximo espaco. */
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
    if (running) { ESP_LOGW(TAG, "START ignorado: campanha ja em andamento"); return; }
    if (!subscribed || h_challenge == 0) {
        ESP_LOGW(TAG, "START ignorado: sem chave inscrita (espere 'inscrito em notify')");
        return;
    }
    if (!run_recebido)
        ESP_LOGW(TAG, "RUN nao enviado: campanha com rotulo padrao '%s' e dist=- (avisar no dataset)",
                 run_label);

    running = true;
    round_i = 0; ok_count = 0; fail_count = 0; timeout_count = 0; answered_count = 0;
    rtt_sum = 0; rtt_min = INT64_MAX; rtt_max = 0;
    s_n_ans = 0;
    update_stats_line();
    cyd_ui_spark_reset();                                                      /* V-07a */
    cyd_ui_spark_band(BASELINE_BAND_MIN_US, BASELINE_BAND_MAX_US);             /* V-07b */
    ESP_LOGI(TAG, "campanha iniciada: rotulo=%s dist=%s rodadas=%d intervalo=%d ms timeout=%d ms",
             run_label, run_dist, ROUNDS, INTERVAL_MS, ROUND_TIMEOUT_MS);
    ESP_LOGI(TAG, "CSVHDR v2 label,dist_m,rodada,nonce,t_tx_us,rtt_us,resultado,rssi_dbm,itvl_1_25ms,mtu");
    cyd_ui_set_state("medindo", ACCENT);
    cyd_ui_set_progress(0, ROUNDS);
    send_challenge();
}

static void bench_cmd(const char *line)
{
    if (strncmp(line, "RUN", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) {
        if (running) { ESP_LOGW(TAG, "RUN ignorado: campanha em andamento"); return; }
        char lbl[sizeof run_label], dst[sizeof run_dist];
        if (kv_get(line, "label=", lbl, sizeof lbl)) snprintf(run_label, sizeof run_label, "%s", lbl);
        if (kv_get(line, "dist=", dst, sizeof dst))  snprintf(run_dist, sizeof run_dist, "%s", dst);
        run_recebido = true;
        set_tag_label();
        ESP_LOGW(TAG, "RUN label=%s dist=%s", run_label, run_dist);
    } else if (strcmp(line, "START") == 0) {
        start_campaign();
    }
}

/* ---- fechamento da rodada (log, CSV, display, proxima rodada) ----------- */

static void finish_round(round_res_t res, int64_t rtt, const uint8_t *nonce,
                         const uint8_t *got, uint16_t got_len)
{
    int8_t rssi = 0;                                  /* 0 = indisponivel */
    if (g_conn != BLE_HS_CONN_HANDLE_NONE && ble_gap_conn_rssi(g_conn, &rssi) != 0) rssi = 0;

    switch (res) {
    case RES_OK:      ok_count++; break;
    case RES_FALHA:   fail_count++; break;
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
        ESP_LOGW(TAG, "rodada %d/%d: TIMEOUT  sem resposta em %d ms  RSSI=%d dBm  nonce=%02x%02x%02x%02x",
                 round_i + 1, ROUNDS, ROUND_TIMEOUT_MS, rssi,
                 nonce[0], nonce[1], nonce[2], nonce[3]);
    } else {
        ESP_LOGI(TAG, "rodada %d/%d: %s  RTT=%lld us  RSSI=%d dBm  nonce=%02x%02x%02x%02x",
                 round_i + 1, ROUNDS, res_str(res), (long long) rtt, rssi,
                 nonce[0], nonce[1], nonce[2], nonce[3]);
    }
    /* Linha parseavel por rodada (colunas em CSVHDR v2). rtt_us = -1 quando TIMEOUT. */
    ESP_LOGI(TAG, "CSV,%s,%s,%d,%02x%02x%02x%02x,%lld,%lld,%s,%d,%u,%u",
             run_label, run_dist, round_i + 1,
             nonce[0], nonce[1], nonce[2], nonce[3],
             (long long) t_tx_us,
             (long long) (res == RES_TIMEOUT ? -1 : rtt),
             res_str(res), rssi, (unsigned) conn_itvl, (unsigned) conn_mtu);

    /* Espelha a rodada no display. */
    int feitas = round_i + 1;
    char rtt_txt[24];
    cyd_ui_set_field(F_RODADA, "%d/%d", feitas, ROUNDS);
    if (res == RES_TIMEOUT) {
        cyd_ui_set_field(F_RTT, "timeout");
    } else {
        cyd_fmt_us(rtt_txt, sizeof rtt_txt, rtt);          /* V-02: "91.2 ms" */
        cyd_ui_set_field(F_RTT, "%s", rtt_txt);
    }
    cyd_ui_set_field(F_OK, "%d (%d%%)", ok_count, ok_count * 100 / feitas);   /* V-05 */
    cyd_ui_set_field(F_FALHA, "%d / %d", fail_count, timeout_count);
    cyd_ui_set_progress(feitas, ROUNDS);                                       /* V-05 */
    update_stats_line();                                                       /* V-08 */
    cyd_ui_set_hex(0, nonce, NONCE_LEN);
    cyd_ui_set_hex(1, got, got_len);
    cyd_ui_pulse();
    /* V-09: expressao do mascote e balao com o resultado; V-07a: um ponto na sparkline. */
    cyd_ui_expression(res == RES_OK ? CYD_EXPR_OK : res == RES_FALHA ? CYD_EXPR_FALHA
                                                                     : CYD_EXPR_TIMEOUT, 1500);
    if (res == RES_TIMEOUT) {
        cyd_ui_badge("TIMEOUT", 0xB8860B, 1200);
    } else {
        char badge[32];
        snprintf(badge, sizeof badge, "%s %s", rtt_txt, res == RES_OK ? "OK" : "FALHA");
        cyd_ui_badge(badge, res == RES_OK ? 0x1AA260 : 0xD64545, 1200);
    }
    cyd_ui_spark_push(res == RES_TIMEOUT ? -1 : rtt);

    /* LED por resultado (V-10): verde OK, vermelho FALHA, amarelo TIMEOUT. */
    cyd_ui_flash(res == RES_OK ? 0x1AA260 : res == RES_FALHA ? 0xD64545 : 0xF4B400, 1200);

    round_i++;
    if (round_i < ROUNDS) {
        /* Grade fixa (M-16): o proximo desafio sai INTERVAL_MS depois do anterior, e nao
         * RTT + 1 s. Se a rodada demorou mais que o intervalo, sai logo. */
        int64_t delay = (t_tx_us + (int64_t) INTERVAL_MS * 1000) - esp_timer_get_time();
        if (delay < 1000) delay = 1000;
        esp_timer_start_once(round_timer, (uint64_t) delay);
    } else {
        running = false;
        if (answered_count > 0) {
            ESP_LOGI(TAG,
                     "=== %s dist=%s n=%d ok=%d falha=%d timeout=%d desconexoes=%lu | RTT us (respondidas): min=%lld avg=%lld max=%lld ===",
                     run_label, run_dist, ROUNDS, ok_count, fail_count, timeout_count,
                     (unsigned long) desconexoes,
                     (long long) rtt_min, (long long) (rtt_sum / answered_count), (long long) rtt_max);
        } else {
            ESP_LOGI(TAG,
                     "=== %s dist=%s n=%d ok=%d falha=%d timeout=%d desconexoes=%lu | RTT us (respondidas): n/a ===",
                     run_label, run_dist, ROUNDS, ok_count, fail_count, timeout_count,
                     (unsigned long) desconexoes);
        }
        bool limpo = (fail_count == 0 && timeout_count == 0);
        cyd_ui_set_state(limpo ? "fim: tudo OK" : "fim: com falhas",
                         limpo ? 0x1AA260 : 0xD64545);
    }
}

/* Prazo estourado sem resposta (M-01). Roda na task do esp_timer. */
static void timeout_cb(void *arg)
{
    uint8_t nonce[NONCE_LEN];
    xSemaphoreTake(round_mux, portMAX_DELAY);
    if (!round_open) { xSemaphoreGive(round_mux); return; }   /* a notify ja fechou */
    round_open = false;
    memcpy(nonce, last_nonce, NONCE_LEN);
    xSemaphoreGive(round_mux);
    finish_round(RES_TIMEOUT, -1, nonce, NULL, 0);
}

/* Para a rodada em voo sem contar (desconexao). */
static void round_abort(const char *motivo)
{
    xSemaphoreTake(round_mux, portMAX_DELAY);
    bool estava_aberta = round_open;
    round_open = false;
    xSemaphoreGive(round_mux);
    esp_timer_stop(timeout_timer);
    esp_timer_stop(round_timer);
    if (estava_aberta)
        ESP_LOGW(TAG, "rodada %d/%d interrompida (%s); nao contada", round_i + 1, ROUNDS, motivo);
    if (running) {
        running = false;
        ESP_LOGW(TAG, "campanha interrompida (%s); envie START de novo apos reconectar", motivo);
    }
}

/* ---- descoberta GATT (encadeada por callbacks) -------------------------- */

static int on_subscribe(uint16_t conn, const struct ble_gatt_error *err,
                        struct ble_gatt_attr *attr, void *arg)
{
    if (err->status != 0) {
        ESP_LOGE(TAG, "falha ao inscrever em notify (status=%d)", err->status);
        return 0;
    }
    subscribed = true;
    ESP_LOGI(TAG, "inscrito em notify; pronto. Envie START para iniciar a campanha");
    cyd_ui_set_state("pronto: envie START", ACCENT);
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
    /* Erro de escrita nao fecha a rodada aqui: o timeout cuida dela. */
    if (err->status != 0) ESP_LOGE(TAG, "escrita do desafio erro=%d", err->status);
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
    esp_timer_stop(timeout_timer);   /* seguranca: nao deve estar armado */
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
        /* Rodada ja fechada pelo timeout: resposta tardia, descartada. */
        xSemaphoreGive(round_mux);
        ESP_LOGW(TAG, "resposta fora do prazo (rodada ja marcada como TIMEOUT); descartada");
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

    finish_round(ok ? RES_OK : RES_FALHA, rtt, nonce, got, got_len);
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
        {
            struct ble_gap_conn_params cp = ble_link_conn_params();   /* M-09b */
            if (ble_gap_connect(own_addr_type, &event->disc.addr, 30000, &cp,
                                gap_event, NULL) != 0) {
                ESP_LOGE(TAG, "ble_gap_connect falhou; volto a escanear");
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
            ESP_LOGI(TAG, "conectado (handle %u); descobrindo servico", g_conn);
            cyd_ui_set_link(CYD_LINK_BLE, 1);                                   /* V-06 */
            conn_itvl = ble_link_log_conn(TAG, g_conn);                        /* M-10 */
            ble_gattc_exchange_mtu(g_conn, ble_link_mtu_exchange_cb, NULL);   /* M-11 */
            cyd_ui_set_state("conectado", 0x1AA260);
            ble_gattc_disc_svc_by_uuid(g_conn, &SVC_UUID.u, on_disc_svc, NULL);
        } else {
            ESP_LOGE(TAG, "conexao falhou (status=%d)", event->connect.status);
            start_scan();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        desconexoes++;                                                       /* M-12 */
        ESP_LOGW(TAG, "desconectado (reason=%d, desconexoes=%lu)",
                 event->disconnect.reason, (unsigned long) desconexoes);
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        subscribed = false;
        cyd_ui_set_link(CYD_LINK_BLE, 0);                                    /* V-06 */
        round_abort("desconexao");
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
    else { ESP_LOGI(TAG, "escaneando pelo servico da chave..."); cyd_ui_set_state("escaneando", ACCENT); }
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

    pkes_hmac_init();
    run_meta_print("veiculo");                                               /* M-13 */
    serial_ota_init();   /* regravacao pela UART0 sem download mode (tools/serial_ota.py) */
    serial_ota_set_cmd_handler(bench_cmd);                                   /* M-14 / M-15 */

    /* Display/UI primeiro, para a tela acender antes mesmo do BLE. */
    cyd_display_init();
    cyd_led_init();
    cyd_display_set_backlight(90);
    cyd_ui_cfg_t cfg = {
        .title  = "VEICULO",
        .accent = ACCENT,
        .mascot = CYD_MASCOT_CAR,
        .fields = { "Rodada", "RTT", "OK", "Falha / tmo", NULL },
        .hex0   = "nonce",
        .hex1   = "resp",
    };
    cyd_ui_init(&cfg);
    set_tag_label();                      /* V-06: modo = rotulo padrao ate o RUN */
    cyd_ui_set_link(CYD_LINK_BLE, 0);     /* V-06: sem conexao ainda */

    round_mux = xSemaphoreCreateMutex();
    if (round_mux == NULL) { ESP_LOGE(TAG, "sem mutex de rodada"); return; }

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
