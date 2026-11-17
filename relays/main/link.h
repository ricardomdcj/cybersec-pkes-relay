#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/*
 * Enlace lateral entre os dois relays (ESP-NOW). O relay tuneliza a troca BLE:
 * o desafio anda B -> A, a resposta anda A -> B.
 *
 * Formato dos frames (little-endian, sem padding):
 *   CHALLENGE  B -> A : [seq:4][nonce:16]                  = LINK_CHAL_LEN (20 B)
 *   RESPONSE   A -> B : [seq:4][turnaround de A em us:4][hmac:16] = LINK_RESP_LEN (24 B)
 *
 * seq e o numero da rodada: o Relay B atribui um seq a cada desafio do veiculo e o
 * Relay A devolve o mesmo seq na resposta. Assim B so entrega ao veiculo a resposta
 * do desafio pendente; resposta atrasada ou duplicada e descartada (M-04).
 * O turnaround de A viaja junto para o B poder decompor o tempo da rodada (M-05).
 */
typedef enum {
    LINK_MSG_CHALLENGE = 1,  /* B -> A : seq + nonce vindo do veiculo */
    LINK_MSG_RESPONSE  = 2,  /* A -> B : seq + turnaround de A + resposta (HMAC) da chave */
} link_msg_type_t;

#define LINK_SEQ_LEN   4
#define LINK_CHAL_LEN  (LINK_SEQ_LEN + 16)          /* 16 = NONCE_LEN (pkes_contract) */
#define LINK_RESP_LEN  (LINK_SEQ_LEN + 4 + 16)      /* 16 = RESPONSE_LEN (pkes_contract) */

/* Chamado quando chega um frame do outro relay. Roda no contexto do WiFi. */
typedef void (*link_rx_cb_t)(link_msg_type_t type, const uint8_t *data, size_t len);

/* Sobe WiFi (STA, sem associar) + ESP-NOW em canal fixo e registra o callback. */
esp_err_t link_init(link_rx_cb_t cb);

/* Envia um frame ao outro relay (unicast para RELAY_PEER_MAC, ou broadcast se nao
 * configurado). Payload ate 249 bytes. */
esp_err_t link_send(link_msg_type_t type, const uint8_t *data, size_t len);

/* Contadores de envio deste relay desde o boot (M-07): falhas (erro na chamada ou status
 * de falha do callback) e total de tentativas. Cumulativos: a diferenca entre rodadas e
 * o que interessa. Qualquer ponteiro pode ser NULL. */
void link_tx_stats(uint32_t *falhas, uint32_t *total);

/* Ajusta a potencia maxima de TX do radio (afeta o alcance do tunel ESP-NOW).
 * Unidade: 0.25 dBm; faixa valida ~[8, 84] = 2..20 dBm. Menos potencia = sinal mais
 * fraco = simula distancia maior num espaco pequeno. Ajustavel em tempo real. */
esp_err_t link_set_tx_power(int8_t power_qdbm);
int8_t    link_get_tx_power(void);
