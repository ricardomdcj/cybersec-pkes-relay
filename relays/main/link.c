#include "link.h"

#include <string.h>
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"

/* Canal 2.4 GHz fixo para os dois relays. Precisa ser o mesmo em A e B. */
#define LINK_CHANNEL 1

static const char *TAG = "LINK";
static link_rx_cb_t s_cb;
static const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len < 1 || s_cb == NULL) return;
    s_cb((link_msg_type_t) data[0], data + 1, (size_t) (len - 1));
}

/* A assinatura do callback de envio do ESP-NOW mudou no IDF v5.4: o 1o argumento deixou
 * de ser 'const uint8_t *mac' e passou a ser 'const esp_now_send_info_t *'. Mantemos as
 * duas formas para compilar tanto no v5.x antigo quanto no v6.1 desta maquina. */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
static void on_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
#else
static void on_sent(const uint8_t *mac, esp_now_send_status_t status)
#endif
{
    if (status != ESP_NOW_SEND_SUCCESS) ESP_LOGW(TAG, "envio ESP-NOW falhou");
}

esp_err_t link_init(link_rx_cb_t cb)
{
    s_cb = cb;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(LINK_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_sent));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = LINK_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW pronto (canal %d)", LINK_CHANNEL);
    return ESP_OK;
}

esp_err_t link_send(link_msg_type_t type, const uint8_t *data, size_t len)
{
    uint8_t buf[ESP_NOW_MAX_DATA_LEN];
    if (len + 1 > sizeof buf) return ESP_ERR_INVALID_SIZE;
    buf[0] = (uint8_t) type;
    if (len) memcpy(buf + 1, data, len);
    return esp_now_send(BCAST, buf, len + 1);
}
