/*
 * Implementacao do HMAC compartilhado (ver pkes_hmac.h). Extraido do antigo
 * vehicle/main/main.c para a chave (key/) poder reaproveitar o mesmo codigo.
 */

#include "pkes_hmac.h"
#include "esp_log.h"

#include "mbedtls/build_info.h"   /* define MBEDTLS_VERSION_NUMBER (v3.x e v4.x) */
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
#include "psa/crypto.h"           /* mbedTLS 4.x (IDF v6.1): HMAC pela PSA Crypto */
#else
#include "mbedtls/md.h"           /* mbedTLS 3.x (IDF v5.4): API MD legada */
#endif

static const char *TAG = "PKES_HMAC";

void pkes_hmac_init(void)
{
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
    if (psa_crypto_init() != PSA_SUCCESS) ESP_LOGE(TAG, "psa_crypto_init falhou");
#endif
}

void pkes_hmac_response(const uint8_t *key, size_t key_len,
                        const uint8_t *msg, size_t msg_len, uint8_t out[32])
{
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
    /* mbedTLS 4.x: a API MD legada (mbedtls_md_hmac) virou identificador privado; o
     * caminho suportado para a app e a PSA Crypto. pkes_hmac_init() roda no app_main. */
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, key_len * 8);

    psa_key_id_t kid = 0;
    if (psa_import_key(&attr, key, key_len, &kid) != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key falhou");
        return;
    }
    size_t out_len = 0;
    psa_status_t st = psa_mac_compute(kid, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                      msg, msg_len, out, 32, &out_len);
    if (st != PSA_SUCCESS) ESP_LOGE(TAG, "psa_mac_compute falhou (%d)", (int) st);
    psa_destroy_key(kid);
#else
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(info, key, key_len, msg, msg_len, out);
#endif
}
