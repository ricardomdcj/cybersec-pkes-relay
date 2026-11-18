#pragma once

#include <stdint.h>
#include "host/ble_uuid.h"

/*
 * GATT contract of the PKES relay bench: the SINGLE source of truth (C).
 *
 * This header used to be copied by hand into vehicle/ and relays/, and mirrored in the
 * BleContract.kt of the Android app. Now that the key is a CYD (project key/), the three C
 * consumers (key, vehicle, relays) include THIS header through the shared component
 * components/pkes_contract. The phoneaskey/ app stays as a historical record; if it is kept,
 * it must keep matching these values.
 *
 * Roles:
 *   - key (peripheral + GATT server): advertises SVC_UUID, receives the nonce on CHALLENGE,
 *     computes HMAC-SHA256(PSK, nonce) truncated to RESPONSE_LEN and notifies on RESPONSE.
 *   - vehicle (central): scans for SVC_UUID, writes the nonce and checks the HMAC (baseline).
 *   - relays: Relay B clones the service; Relay A talks to the real key. The relays do NOT
 *     interpret the HMAC (they are dumb pipes), but they need the same UUIDs and sizes.
 *
 * 128-bit UUIDs: BLE_UUID128_INIT takes the bytes in LITTLE-ENDIAN order, that is, reversed
 * relative to the string. Example: 8f2a0001-1b2c-4e6a-9f3d-2a7c1e5b0a10 becomes
 * 10,0a,5b,1e,7c,2a,3d,9f,6a,4e,2c,1b,01,00,2a,8f.
 */

/* Service: 8f2a0001-1b2c-4e6a-9f3d-2a7c1e5b0a10 */
static const ble_uuid128_t SVC_UUID = BLE_UUID128_INIT(
    0x10, 0x0a, 0x5b, 0x1e, 0x7c, 0x2a, 0x3d, 0x9f,
    0x6a, 0x4e, 0x2c, 0x1b, 0x01, 0x00, 0x2a, 0x8f);

/* Challenge (WRITE): 8f2a0002-... */
static const ble_uuid128_t CHR_CHALLENGE_UUID = BLE_UUID128_INIT(
    0x10, 0x0a, 0x5b, 0x1e, 0x7c, 0x2a, 0x3d, 0x9f,
    0x6a, 0x4e, 0x2c, 0x1b, 0x02, 0x00, 0x2a, 0x8f);

/* Response (NOTIFY/READ): 8f2a0003-... */
static const ble_uuid128_t CHR_RESPONSE_UUID = BLE_UUID128_INIT(
    0x10, 0x0a, 0x5b, 0x1e, 0x7c, 0x2a, 0x3d, 0x9f,
    0x6a, 0x4e, 0x2c, 0x1b, 0x03, 0x00, 0x2a, 0x8f);

#define NONCE_LEN     16
#define RESPONSE_LEN  16

/* Bench-only pre-shared secret: the text "PKES-RELAY-BENCH".
 * Confidentiality of the key is not part of the attack surface under study, so it is fixed
 * and identical on both sides. The relays do not use this array. */
static const uint8_t PSK[16] = {
    0x50, 0x4b, 0x45, 0x53, 0x2d, 0x52, 0x45, 0x4c,
    0x41, 0x59, 0x2d, 0x42, 0x45, 0x4e, 0x43, 0x48};
