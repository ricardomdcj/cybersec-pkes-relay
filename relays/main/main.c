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

/* Bench commands over UART0 (via serial_ota), to sweep the "effective distance"
 * without reflashing or moving boards:
 *   TXPOW <n>   sets the ESP-NOW TX power (n in 0.25 dBm, 8..84 = 2..20 dBm)
 *   DELAY <ms>  Relay B only: holds each vehicle challenge for <ms> before the tunnel (0 = off)
 * The value appears in the log (TXPOW / DELAY prefix) to label the campaign in the dataset. */
static void bench_cmd(const char *line)
{
    if (strncmp(line, "TXPOW ", 6) == 0) {
        int8_t n = (int8_t) atoi(line + 6);
        link_set_tx_power(n);
        ESP_LOGW("BENCH", "TXPOW set=%d (0.25dBm)", (int) n);
    } else if (strncmp(line, "DELAY ", 6) == 0) {
#if RELAY_ROLE == RELAY_ROLE_B
        uint32_t ms = (uint32_t) strtoul(line + 6, NULL, 10);
        relay_b_set_delay_ms(ms);
        ESP_LOGW("BENCH", "DELAY set=%lu ms (relay B, challenge path)", (unsigned long) ms);
#endif
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();   /* WiFi/ESP-NOW need NVS */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    serial_ota_init();   /* reflash over UART0 without download mode (tools/serial_ota.py) */
    serial_ota_set_cmd_handler(bench_cmd);   /* TXPOW commands on the same UART0 */

#if RELAY_ROLE == RELAY_ROLE_A
    relay_a_start();
#elif RELAY_ROLE == RELAY_ROLE_B
    relay_b_start();
#else
#error "Set RELAY_ROLE to RELAY_ROLE_A or RELAY_ROLE_B in relay_config.h"
#endif
}
