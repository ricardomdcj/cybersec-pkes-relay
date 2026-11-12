#pragma once

#include <stdint.h>
#include "host/ble_uuid.h"

/*
 * Espelho de BleContract.kt do app (phoneaskey). MANTER IDENTICO nos dois lados.
 *
 * Papel do veiculo = central. Ele varre pelo SVC_UUID, conecta, habilita notify na
 * caracteristica RESPONSE, escreve um nonce em CHALLENGE e confere o HMAC de volta.
 *
 * UUIDs de 128 bits: a macro BLE_UUID128_INIT recebe os bytes em LITTLE-ENDIAN,
 * ou seja, na ordem inversa da string. Ex.: 8f2a0001-1b2c-4e6a-9f3d-2a7c1e5b0a10
 * vira 10,0a,5b,1e,7c,2a,3d,9f,6a,4e,2c,1b,01,00,2a,8f.
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

/* Segredo pre-compartilhado APENAS de bancada: o texto "PKES-RELAY-BENCH".
 * A confidencialidade da chave nao faz parte da superficie de ataque estudada. */
static const uint8_t PSK[16] = {
    0x50, 0x4b, 0x45, 0x53, 0x2d, 0x52, 0x45, 0x4c,
    0x41, 0x59, 0x2d, 0x42, 0x45, 0x4e, 0x43, 0x48};
