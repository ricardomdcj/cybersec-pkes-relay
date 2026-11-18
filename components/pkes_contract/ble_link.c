#include "ble_link_params.h"

#include "esp_log.h"

static const char *TAG_BLE = "BLE";

static const char *phy_str(uint8_t phy)
{
    switch (phy) {
    case BLE_GAP_LE_PHY_1M:    return "1M";
    case BLE_GAP_LE_PHY_2M:    return "2M";
    case BLE_GAP_LE_PHY_CODED: return "coded";
    default:                   return "?";
    }
}

uint16_t ble_link_log_conn(const char *tag, uint16_t conn_handle)
{
    struct ble_gap_conn_desc d;
    if (ble_gap_conn_find(conn_handle, &d) != 0) {
        ESP_LOGW(tag, "CONN handle=%u: description unavailable", conn_handle);
        return 0;
    }

    uint8_t tx_phy = 0, rx_phy = 0;
    if (ble_gap_read_le_phy(conn_handle, &tx_phy, &rx_phy) != 0) {
        tx_phy = rx_phy = 0;
    }

    ESP_LOGI(tag, "CONN handle=%u itvl=%u (%lu us) latency=%u supervision=%u ms role=%s phy_tx=%s phy_rx=%s",
             conn_handle, d.conn_itvl, (unsigned long) d.conn_itvl * 1250UL,
             d.conn_latency, (unsigned) d.supervision_timeout * 10,
             d.role == BLE_GAP_ROLE_MASTER ? "central" : "peripheral",
             phy_str(tx_phy), phy_str(rx_phy));
    return d.conn_itvl;
}

void ble_link_log_mtu(const char *tag, uint16_t conn_handle, uint16_t mtu)
{
    ESP_LOGI(tag, "MTU handle=%u mtu=%u", conn_handle, mtu);
}

int ble_link_mtu_exchange_cb(uint16_t conn, const struct ble_gatt_error *err,
                             uint16_t mtu, void *arg)
{
    if (err->status != 0) ESP_LOGW(TAG_BLE, "MTU exchange failed (status=%d)", err->status);
    return 0;
}
