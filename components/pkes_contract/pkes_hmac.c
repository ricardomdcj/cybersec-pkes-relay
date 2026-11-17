/*
 * Shared HMAC implementation (see pkes_hmac.h). Extracted from the old
 * vehicle/main/main.c so that the key (key/) can reuse the same code.
 */

#include "pkes_hmac.h"
#include "esp_log.h"

#include "mbedtls/build_info.h"   /* defines MBEDTLS_VERSION_NUMBER (v3.x and v4.x) */
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
#include "psa/crypto.h"           /* mbedTLS 4.x (IDF v6.1): HMAC through PSA Crypto */
#else
#include "mbedtls/md.h"           /* mbedTLS 3.x (IDF v5.4): legacy MD API */
#endif

static const char *TAG = "PKES_HMAC";

void pkes_hmac_init(void)
{
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
    if (psa_crypto_init() != PSA_SUCCESS) ESP_LOGE(TAG, "psa_crypto_init failed");
#endif
}

void pkes_hmac_response(const uint8_t *key, size_t key_len,
                        const uint8_t *msg, size_t msg_len, uint8_t out[32])
{
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
    /* mbedTLS 4.x: the legacy MD API (mbedtls_md_hmac) became a private identifier; the
     * supported path for the app is PSA Crypto. pkes_hmac_init() runs in app_main. */
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, key_len * 8);

    psa_key_id_t kid = 0;
    if (psa_import_key(&attr, key, key_len, &kid) != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed");
        return;
    }
    size_t out_len = 0;
    psa_status_t st = psa_mac_compute(kid, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                      msg, msg_len, out, 32, &out_len);
    if (st != PSA_SUCCESS) ESP_LOGE(TAG, "psa_mac_compute failed (%d)", (int) st);
    psa_destroy_key(kid);
#else
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(info, key, key_len, msg, msg_len, out);
#endif
}
