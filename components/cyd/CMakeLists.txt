# Presentation layer of the CYD (ST7789 display + RGB LED + status UI with mascot),
# shared by key/, vehicle/ and relays/. The lvgl/lvgl dependency comes from the
# idf_component.yml next to it (component manager).
# lvgl in REQUIRES (not PRIV_REQUIRES): the public headers cyd_display.h/cyd_ui.h
# include lvgl.h, so the dependency must propagate to the components that use the cyd.
idf_component_register(SRCS "cyd_display.c" "cyd_led.c" "cyd_ui.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl__lvgl esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_timer
                    PRIV_REQUIRES esp_hw_support)
