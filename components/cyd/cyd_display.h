#pragma once
#include <stdint.h>
#include "lvgl.h"

/*
 * Display ST7789 + LVGL 8.3 da CYD. Adaptado do deskrab (main/display.c), sem o
 * recurso de rotacao por settings/NVS: a bancada PKES e view-only e fixa.
 *
 * Toda chamada ao LVGL (criar/alterar objetos) tem que ocorrer entre
 * cyd_display_lock() e cyd_display_unlock(), pois a task 'lvgl' redesenha em paralelo.
 */
void cyd_display_init(void);
void cyd_display_lock(void);
void cyd_display_unlock(void);
void cyd_display_set_backlight(uint8_t percent);  // 0-100
