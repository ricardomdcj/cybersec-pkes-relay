#pragma once

#include <stdint.h>
#include "host/ble_uuid.h"

/*
 * Contrato GATT da bancada PKES relay — FONTE UNICA de verdade (C).
 *
 * Antes este arquivo era duplicado a mao em vehicle/ e relays/, e espelhado tambem
 * no BleContract.kt do app Android. Com a chave virando uma CYD (projeto key/), os
 * tres consumidores em C (key, vehicle, relays) passam a incluir ESTE header via o
 * componente compartilhado components/pkes_contract. O app phoneaskey/ segue como
 * registro historico e, se mantido, precisa continuar casando estes valores.
 *
 * Papeis:
 *   - key (peripheral + servidor GATT): anuncia SVC_UUID, recebe o nonce em CHALLENGE,
 *     calcula HMAC-SHA256(PSK, nonce) truncado em RESPONSE_LEN e notifica em RESPONSE.
 *   - vehicle (central): varre pelo SVC_UUID, escreve o nonce e confere o HMAC (baseline).
 *   - relays: Relay B clona o servico; Relay A conversa com a chave real. Os relays NAO
 *     interpretam o HMAC (sao canos burros), mas precisam dos mesmos UUIDs/tamanhos.
 *
 * UUIDs de 128 bits: BLE_UUID128_INIT recebe os bytes em LITTLE-ENDIAN, ou seja, na
 * ordem inversa da string. Ex.: 8f2a0001-1b2c-4e6a-9f3d-2a7c1e5b0a10 vira
 * 10,0a,5b,1e,7c,2a,3d,9f,6a,4e,2c,1b,01,00,2a,8f.
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
 * A confidencialidade da chave nao faz parte da superficie de ataque estudada; por
 * isso e fixa e identica nos dois lados. Os relays nao usam este array. */
static const uint8_t PSK[16] = {
    0x50, 0x4b, 0x45, 0x53, 0x2d, 0x52, 0x45, 0x4c,
    0x41, 0x59, 0x2d, 0x42, 0x45, 0x4e, 0x43, 0x48};
