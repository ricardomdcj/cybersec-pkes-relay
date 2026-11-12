/*
 * Relay B — peripheral BLE (perto do veiculo) + enlace ESP-NOW.
 *
 * Papel no ataque:
 *   - clona o servico da chave e anuncia, para o veiculo conectar nele;
 *   - quando o veiculo escreve o desafio, manda o nonce por ESP-NOW (LINK_MSG_CHALLENGE);
 *   - quando chega a resposta do Relay A (LINK_MSG_RESPONSE), notifica o veiculo.
 *
 * ESTADO: esqueleto. O enlace ESP-NOW ja funciona. No autoteste, o Relay B faz o
 * papel de "originador" (como o veiculo faria): dispara um desafio periodico e mede
 * o tempo de ida e volta pelo tunel A<->B (overhead do enlace, sem BLE).
 * A metade BLE peripheral esta marcada com TODO e deve espelhar o KeyPeripheral.kt.
 */

#include "relay_b.h"
#include "relay_config.h"
#include "link.h"
#include "ble_contract.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "RELAY_B";

#if RELAY_LINK_SELFTEST
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
    esp_timer_start_once(st_timer, 1000000); /* 1 s */
}
#endif

/* Recebe frames do Relay A. */
static void on_link_rx(link_msg_type_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case LINK_MSG_RESPONSE:
#if RELAY_LINK_SELFTEST
    {
        int64_t rtt = esp_timer_get_time() - st_tx_us;
        ESP_LOGI(TAG, "[selfteste] resposta recebida (%d B); RTT tunel=%lld us",
                 (int) len, (long long) rtt);
    }
#else
        /* TODO(BLE): notificar 'data' (HMAC) na caracteristica RESPONSE para o veiculo:
         *   ble_gatts_notify_custom(conn, h_response_val, om_from(data,len)); */
        ESP_LOGI(TAG, "resposta recebida (%d B) -> notificar o veiculo (TODO)", (int) len);
#endif
        break;
    default:
        break;
    }
}

/* TODO(BLE): chamar isto no write da caracteristica CHALLENGE feito pelo veiculo. */
static inline void forward_challenge(const uint8_t *nonce, size_t len)
{
    link_send(LINK_MSG_CHALLENGE, nonce, len);
}

void relay_b_start(void)
{
    ESP_ERROR_CHECK(link_init(on_link_rx));

    /* TODO(BLE peripheral) — espelhar phoneaskey/KeyPeripheral.kt em NimBLE:
     *   1. nimble_port_init() + ble_hs_cfg.sync_cb/reset_cb
     *   2. registrar servico SVC_UUID com CHALLENGE (write) e RESPONSE (notify)
     *   3. write em CHALLENGE -> forward_challenge(data, len)
     *   4. anunciar (ble_gap_adv_start) com o SVC_UUID
     *   5. nimble_port_freertos_init(host_task)
     */

    ESP_LOGI(TAG, "Relay B iniciado (BLE peripheral: TODO)");

#if RELAY_LINK_SELFTEST
    const esp_timer_create_args_t ta = { .callback = selftest_fire, .name = "selftest" };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &st_timer));
    esp_timer_start_once(st_timer, 1000000);
    ESP_LOGI(TAG, "[selfteste] originando desafios pelo tunel a cada 1 s");
#else
    (void) forward_challenge;  /* silencia 'unused' enquanto o BLE nao entra */
#endif
}
