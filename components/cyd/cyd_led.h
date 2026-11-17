#pragma once
#include <stdbool.h>
#include <stdint.h>

// LED RGB da CYD (ativo em nivel baixo). Adaptado do deskrab (main/led.c).
typedef enum {
    CYD_LED_OFF,
    CYD_LED_BREATHE,   // respira devagar na cor dada
    CYD_LED_BLINK,     // pisca na cor dada
    CYD_LED_SOLID,
} cyd_led_mode_t;

void cyd_led_init(void);
// Cor em 0xRRGGBB; o brilho maximo e limitado em cyd_led.c
void cyd_led_set(cyd_led_mode_t mode, uint32_t rgb);
