#pragma once
#include <stdint.h>

// Virtual clock of the simulator, in microseconds since the start of the run.
extern int64_t g_sim_us;

// Verbose mode: prints the LED calls with the virtual time.
extern int g_sim_verbose;

// Advances the virtual clock by 'ms': ticks LVGL, fires the esp_timer callbacks and runs the
// LVGL timers (animations, blinks) in steps of 5 ms, like the 2 ms tick of the device.
void sim_advance_ms(int ms);

// Redraws the screen and writes the 320x240 framebuffer to a 24-bit BMP. Returns 0 on success.
int sim_write_bmp(const char *path);
