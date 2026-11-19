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

/* Board MACs, as printed in the boot log ("this board MAC"). Each relay unicasts to the
 * other one, so the tunnel gets a MAC-level ACK and its losses show up in tx_failures.
 * Set RELAY_PEER_MAC to all zeros to go back to broadcast (no ACK). */
#define RELAY_MAC_A {0xb8, 0xbb, 0x11, 0x83, 0xa9, 0x80}
#define RELAY_MAC_B {0x48, 0xaf, 0xf3, 0x70, 0x1d, 0x58}
#ifndef RELAY_PEER_MAC
#if RELAY_ROLE == RELAY_ROLE_A
#define RELAY_PEER_MAC RELAY_MAC_B
#else
#define RELAY_PEER_MAC RELAY_MAC_A
#endif
#endif
