#pragma once

#include <stdint.h>
#include "host/ble_uuid.h"

/*
 * Espelho de BleContract.kt (app) e do ble_contract.h do vehicle. MANTER IDENTICO.
 * O relay nao interpreta o HMAC; estes valores servem para o Relay B clonar o
 * servico da chave e para o Relay A conversar com a chave real.
 *
 * BLE_UUID128_INIT recebe os bytes em little-endian (ordem inversa da string).
 */

/* Servico: 8f2a0001-1b2c-4e6a-9f3d-2a7c1e5b0a10 */
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
