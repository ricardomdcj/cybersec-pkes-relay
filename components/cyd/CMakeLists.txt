# Camada de apresentacao da CYD (display ST7789 + LED RGB + UI de status com mascote),
# compartilhada por key/, vehicle/ e relays/. A dependencia lvgl/lvgl vem do
# idf_component.yml ao lado (component manager).
# lvgl em REQUIRES (nao PRIV_REQUIRES): os headers publicos cyd_display.h/cyd_ui.h
# incluem lvgl.h, entao a dependencia precisa ser propagada aos componentes que usam a cyd.
idf_component_register(SRCS "cyd_display.c" "cyd_led.c" "cyd_ui.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl__lvgl esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_timer
                    PRIV_REQUIRES esp_hw_support)
