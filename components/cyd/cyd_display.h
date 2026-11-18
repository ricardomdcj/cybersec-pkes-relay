#pragma once
#include <stdint.h>
#include "lvgl.h"

/*
 * ST7789 display + LVGL 8.3 for the CYD. Adapted from deskrab (main/display.c), without the
 * rotation feature driven by settings/NVS: the PKES bench is view-only and fixed.
 *
 * Every LVGL call (creating/changing objects) must happen between cyd_display_lock() and
 * cyd_display_unlock(), because the 'lvgl' task redraws in parallel.
 */
void cyd_display_init(void);
void cyd_display_lock(void);
void cyd_display_unlock(void);
void cyd_display_set_backlight(uint8_t percent);  // 0-100
