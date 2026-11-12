/*
 * Relay A — central BLE (perto do celular/chave) + enlace ESP-NOW.
 *
 * Papel no ataque:
 *   - recebe o nonce do Relay B por ESP-NOW (LINK_MSG_CHALLENGE);
 *   - escreve esse nonce na caracteristica CHALLENGE da chave real;
 *   - recebe o HMAC pela notify da chave e devolve por ESP-NOW (LINK_MSG_RESPONSE).
 *
 * ESTADO: esqueleto. O enlace ESP-NOW ja funciona (com autoteste). A metade BLE
 * central esta marcada com TODO e deve reaproveitar a logica de vehicle/main.c
 * (scan pelo SVC_UUID, conexao, descoberta, notify).
 */

#include "relay_a.h"
#include "relay_config.h"
#include "link.h"
#include "ble_contract.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "RELAY_A";

/* Recebe frames do Relay B. */
static void on_link_rx(link_msg_type_t type, const uint8_t *data, size_t len)
{
    switch (type) {
    case LINK_MSG_CHALLENGE:
#if RELAY_LINK_SELFTEST
        /* Autoteste: devolve a resposta na hora (finge ser a chave). */
        ESP_LOGI(TAG, "[selfteste] desafio recebido (%d B); ecoando resposta", (int) len);
        link_send(LINK_MSG_RESPONSE, data, len);
#else
        /* TODO(BLE): escrever 'data' (nonce) na caracteristica CHALLENGE da chave:
         *   ble_gattc_write_flat(conn, h_challenge, data, len, on_write_cb, NULL);
         * A resposta chega depois em BLE_GAP_EVENT_NOTIFY_RX -> forward_response(). */
        ESP_LOGI(TAG, "desafio recebido (%d B) -> encaminhar para a chave (TODO)", (int) len);
#endif
        break;
    default:
        break;
    }
}

/* TODO(BLE): chamar isto no NOTIFY_RX da chave, com o HMAC recebido. */
static inline void forward_response(const uint8_t *resp, size_t len)
{
    link_send(LINK_MSG_RESPONSE, resp, len);
}

void relay_a_start(void)
{
    ESP_ERROR_CHECK(link_init(on_link_rx));

    /* TODO(BLE central) — reaproveitar vehicle/main.c:
     *   1. nimble_port_init() + ble_hs_cfg.sync_cb/reset_cb
     *   2. on_sync -> ble_gap_disc() pelo SVC_UUID
     *   3. conectar na chave, descobrir CHALLENGE/RESPONSE, habilitar notify
     *   4. NOTIFY_RX -> forward_response(om, len)
     *   5. nimble_port_freertos_init(host_task)
     */

    ESP_LOGI(TAG, "Relay A iniciado (BLE central: TODO)");

#if !RELAY_LINK_SELFTEST
    (void) forward_response;   /* silencia 'unused' enquanto o BLE nao entra */
#endif
}
