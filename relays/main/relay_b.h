#pragma once

#include <stdint.h>

/* Relay B: BLE peripheral (near the vehicle) + ESP-NOW link to Relay A. */
void relay_b_start(void);

/* Bench only: delay (ms) that Relay B adds to each vehicle challenge before the tunnel. 0 = off. */
void relay_b_set_delay_ms(uint32_t ms);
