#include "esp_err.h"
#include "nvs_flash.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

#include "relay_config.h"
#include "relay_a.h"
#include "relay_b.h"
#include "serial_ota.h"
#include "link.h"

/* Comandos de bancada pela UART0 (via serial_ota), p/ varrer a "distancia efetiva"
 * sem reflashar nem mover placas:
 *   TXPOW <n>   ajusta a potencia de TX do ESP-NOW (n em 0.25dBm, 8..84 = 2..20 dBm)
 * O valor aparece no log (prefixo TXPOW) p/ etiquetar a campanha no dataset. */
static void bench_cmd(const char *line)
{
    if (strncmp(line, "TXPOW ", 6) == 0) {
        int8_t n = (int8_t) atoi(line + 6);
        link_set_tx_power(n);
        ESP_LOGW("BENCH", "TXPOW set=%d (0.25dBm)", (int) n);
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();   /* WiFi/ESP-NOW precisam do NVS */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    serial_ota_init();   /* regravacao pela UART0 sem download mode (tools/serial_ota.py) */
    serial_ota_set_cmd_handler(bench_cmd);   /* comandos TXPOW... na mesma UART0 */

#if RELAY_ROLE == RELAY_ROLE_A
    relay_a_start();
#elif RELAY_ROLE == RELAY_ROLE_B
    relay_b_start();
#else
#error "Defina RELAY_ROLE como RELAY_ROLE_A ou RELAY_ROLE_B em relay_config.h"
#endif
}
