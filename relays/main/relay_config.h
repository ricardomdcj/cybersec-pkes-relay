#pragma once

/*
 * A single project builds both relays; the role is selected by a define.
 *
 *   RELAY_ROLE_A -> central, stays near the key (key/)
 *   RELAY_ROLE_B -> peripheral, stays near the vehicle
 *
 * Change RELAY_ROLE below before building each board. Both roles now run on the SAME CYD
 * (classic ESP32); the two boards differ only by the role flashed:
 *   Relay A:  idf.py set-target esp32  -DRELAY_ROLE=1
 *   Relay B:  idf.py set-target esp32  -DRELAY_ROLE=2
 *
 * Alternative without editing the header: idf.py build -DRELAY_ROLE=1  (or =2).
 */

#define RELAY_ROLE_A 1
#define RELAY_ROLE_B 2

#ifndef RELAY_ROLE
#define RELAY_ROLE RELAY_ROLE_A
#endif

/* Link self-test: A and B exchange frames without BLE, only to validate the ESP-NOW tunnel
 * and measure its overhead (Milestone 1). With BLE ready (Milestone 2), set 0 to run the real
 * chain phone -> A -> B -> vehicle.
 * Overridable on the command line: idf.py build -DRELAY_LINK_SELFTEST=0 */
#ifndef RELAY_LINK_SELFTEST
#define RELAY_LINK_SELFTEST 1
#endif

/* MAC address of the other relay (M-06b), in the form {0xAA,0xBB,...}. Each board needs the
 * OTHER board's MAC: Relay A puts Relay B's MAC here, and Relay B puts Relay A's MAC here.
 * The MACs are printed in the boot log ("this board MAC"). While this is all zeros, the link
 * uses broadcast, as before. Broadcast has no MAC-layer ACK, so tunnel losses do not show up
 * in the counters. */
#ifndef RELAY_PEER_MAC
#define RELAY_PEER_MAC {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#endif
