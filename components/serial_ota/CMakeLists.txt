# Serial OTA over UART0, shared by key/, vehicle/ and relays/.
# app_update = esp_ota_ops; esp_app_format = esp_app_get_description; mbedtls = base64.
idf_component_register(SRCS "serial_ota.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_uart app_update esp_app_format mbedtls)
