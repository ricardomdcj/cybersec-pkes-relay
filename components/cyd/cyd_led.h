#pragma once
#include <stdbool.h>
#include <stdint.h>

// CYD RGB LED (active low). Adapted from deskrab (main/led.c).
typedef enum {
    CYD_LED_OFF,
    CYD_LED_BREATHE,   // slowly breathes in the given color
    CYD_LED_BLINK,     // blinks in the given color
    CYD_LED_SOLID,
} cyd_led_mode_t;

void cyd_led_init(void);
// Color as 0xRRGGBB; the maximum brightness is limited in cyd_led.c
void cyd_led_set(cyd_led_mode_t mode, uint32_t rgb);
