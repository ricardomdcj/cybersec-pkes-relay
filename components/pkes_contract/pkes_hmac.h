#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * HMAC-SHA256(PSK, nonce) truncated to RESPONSE_LEN, shared between the key (key/, which
 * computes it to answer) and the vehicle (vehicle/, which checks it in the baseline).
 *
 * Two paths, depending on the mbedTLS version (see CLAUDE.md):
 *   - mbedTLS 4.x (IDF v6.1): PSA Crypto. Requires one pkes_hmac_init() in app_main.
 *   - mbedTLS 3.x (IDF v5.4): legacy MD API. pkes_hmac_init() is a no-op.
 */

/* Call once in app_main, before the first pkes_hmac_response(). */
void pkes_hmac_init(void);

/* out receives 32 bytes (the full SHA-256); the caller uses the first RESPONSE_LEN. */
void pkes_hmac_response(const uint8_t *key, size_t key_len,
                        const uint8_t *msg, size_t msg_len, uint8_t out[32]);
