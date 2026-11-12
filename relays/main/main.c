#include "esp_err.h"
#include "nvs_flash.h"

#include "relay_config.h"
#include "relay_a.h"
#include "relay_b.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();   /* WiFi/ESP-NOW precisam do NVS */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if RELAY_ROLE == RELAY_ROLE_A
    relay_a_start();
#elif RELAY_ROLE == RELAY_ROLE_B
    relay_b_start();
#else
#error "Defina RELAY_ROLE como RELAY_ROLE_A ou RELAY_ROLE_B em relay_config.h"
#endif
}
