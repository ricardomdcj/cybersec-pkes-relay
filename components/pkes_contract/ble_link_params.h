#pragma once

#include <stdint.h>
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

/*
 * Parametros de conexao BLE da bancada (M-09a). Usados pelo veiculo e pelo Relay A, que
 * sao centrais e escolhem os parametros ao conectar. Unidades do NimBLE:
 *   intervalo: 1.25 ms;  supervisao: 10 ms;  scan e ce: 0.625 ms.
 *
 * Estes valores SAO os padroes do NimBLE (ble_gap_conn_params_dflt): trocar para
 * explicito nao muda o comportamento, so o torna visivel e registrado. Mudar aqui afeta
 * o baseline e os relays ao mesmo tempo. Toda campanha deve usar o mesmo valor, e o valor
 * negociado vai para o log (linha CONN) e para o CSV (coluna itvl).
 */
#define BLE_LINK_SCAN_ITVL     0x0010   /* 10 ms */
#define BLE_LINK_SCAN_WINDOW   0x0010   /* 10 ms */
#define BLE_LINK_ITVL_MIN      24       /* 30 ms */
#define BLE_LINK_ITVL_MAX      40       /* 50 ms */
#define BLE_LINK_LATENCY       0
#define BLE_LINK_SUP_TIMEOUT   0x0100   /* 2,56 s */
#define BLE_LINK_CE_MIN        0
#define BLE_LINK_CE_MAX        0

static inline struct ble_gap_conn_params ble_link_conn_params(void)
{
    struct ble_gap_conn_params p = {
        .scan_itvl           = BLE_LINK_SCAN_ITVL,
        .scan_window         = BLE_LINK_SCAN_WINDOW,
        .itvl_min            = BLE_LINK_ITVL_MIN,
        .itvl_max            = BLE_LINK_ITVL_MAX,
        .latency             = BLE_LINK_LATENCY,
        .supervision_timeout = BLE_LINK_SUP_TIMEOUT,
        .min_ce_len          = BLE_LINK_CE_MIN,
        .max_ce_len          = BLE_LINK_CE_MAX,
    };
    return p;
}

/* Loga a linha CONN (intervalo, latencia, supervisao, papel e PHY) da conexao e devolve
 * o intervalo efetivo em unidades de 1.25 ms (0 se a conexao nao existe mais). */
uint16_t ble_link_log_conn(const char *tag, uint16_t conn_handle);

/* Loga a linha MTU (M-11) quando o ATT MTU muda. */
void ble_link_log_mtu(const char *tag, uint16_t conn_handle, uint16_t mtu);

/* Callback para ble_gattc_exchange_mtu: so registra falha, o valor chega no evento MTU. */
int ble_link_mtu_exchange_cb(uint16_t conn, const struct ble_gatt_error *err,
                             uint16_t mtu, void *arg);
