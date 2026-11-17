#pragma once

#include <stdint.h>
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

/*
 * BLE connection parameters of the bench (M-09a). Used by the vehicle and by Relay A, which
 * are centrals and choose the parameters when connecting. NimBLE units:
 *   interval: 1.25 ms;  supervision: 10 ms;  scan and ce: 0.625 ms.
 *
 * These values ARE the NimBLE defaults (ble_gap_conn_params_dflt): making them explicit does
 * not change behavior, it only makes them visible and logged. Changing them here affects the
 * baseline and the relays at the same time. Every campaign must use the same value, and the
 * negotiated value goes to the log (CONN line) and to the CSV (itvl column).
 */
#define BLE_LINK_SCAN_ITVL     0x0010   /* 10 ms */
#define BLE_LINK_SCAN_WINDOW   0x0010   /* 10 ms */
#define BLE_LINK_ITVL_MIN      24       /* 30 ms */
#define BLE_LINK_ITVL_MAX      40       /* 50 ms */
#define BLE_LINK_LATENCY       0
#define BLE_LINK_SUP_TIMEOUT   0x0100   /* 2.56 s */
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

/* Logs the CONN line (interval, latency, supervision, role and PHY) of the connection and
 * returns the effective interval in 1.25 ms units (0 if the connection no longer exists). */
uint16_t ble_link_log_conn(const char *tag, uint16_t conn_handle);

/* Logs the MTU line (M-11) when the ATT MTU changes. */
void ble_link_log_mtu(const char *tag, uint16_t conn_handle, uint16_t mtu);

/* Callback for ble_gattc_exchange_mtu: only records failures; the value arrives in the MTU event. */
int ble_link_mtu_exchange_cb(uint16_t conn, const struct ble_gatt_error *err,
                             uint16_t mtu, void *arg);
