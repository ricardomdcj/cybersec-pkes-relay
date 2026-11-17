#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * HMAC-SHA256(PSK, nonce) truncado em RESPONSE_LEN, compartilhado entre a chave (key/,
 * que o calcula para responder) e o veiculo (vehicle/, que o confere no baseline).
 *
 * Dois caminhos conforme a versao do mbedTLS (ver CLAUDE.md):
 *   - mbedTLS 4.x (IDF v6.1): PSA Crypto. Exige pkes_hmac_init() uma vez no app_main.
 *   - mbedTLS 3.x (IDF v5.4): API MD legada. pkes_hmac_init() e no-op.
 */

/* Chame uma vez no app_main antes do primeiro pkes_hmac_response(). */
void pkes_hmac_init(void);

/* out recebe 32 bytes (SHA-256 cheio); o chamador usa os primeiros RESPONSE_LEN. */
void pkes_hmac_response(const uint8_t *key, size_t key_len,
                        const uint8_t *msg, size_t msg_len, uint8_t out[32]);
