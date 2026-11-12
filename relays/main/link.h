#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/*
 * Enlace lateral entre os dois relays (ESP-NOW). O relay tuneliza a troca BLE:
 * o desafio anda B -> A, a resposta anda A -> B.
 */
typedef enum {
    LINK_MSG_CHALLENGE = 1,  /* B -> A : nonce vindo do veiculo */
    LINK_MSG_RESPONSE  = 2,  /* A -> B : resposta (HMAC) vinda da chave */
} link_msg_type_t;

/* Chamado quando chega um frame do outro relay. Roda no contexto do WiFi. */
typedef void (*link_rx_cb_t)(link_msg_type_t type, const uint8_t *data, size_t len);

/* Sobe WiFi (STA, sem associar) + ESP-NOW em canal fixo e registra o callback. */
esp_err_t link_init(link_rx_cb_t cb);

/* Envia um frame ao outro relay (broadcast). Payload ate 249 bytes. */
esp_err_t link_send(link_msg_type_t type, const uint8_t *data, size_t len);
