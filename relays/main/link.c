#include "link.h"
#include "relay_config.h"

#include <stdbool.h>
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

/* Destino do tunel: MAC do outro relay (M-06b) se configurado, senao broadcast. */
static uint8_t s_dst[6];
static bool    s_unicast;

/* Contadores de envio (M-07). Escritos no callback do WiFi e na chamada de link_send. */
static volatile uint32_t s_tx_fail;
static volatile uint32_t s_tx_total;

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
    if (status != ESP_NOW_SEND_SUCCESS) {
        s_tx_fail++;
        ESP_LOGW(TAG, "envio ESP-NOW falhou (falhas=%lu)", (unsigned long) s_tx_fail);
    }
}

static bool peer_configured(const uint8_t *mac)
{
    for (int i = 0; i < 6; i++) if (mac[i] != 0) return true;
    return false;
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

    /* M-08: sem power save o WiFi nao dorme entre frames; o padrao adiciona latencia. */
    esp_err_t ps_rc = esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "power save WiFi = NONE (rc=%d)", (int) ps_rc);

    /* M-06a: MAC desta placa, para colar no RELAY_PEER_MAC do outro relay. */
    uint8_t mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        ESP_LOGI(TAG, "MAC desta placa: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_sent));

    const uint8_t peer_mac[6] = RELAY_PEER_MAC;
    s_unicast = peer_configured(peer_mac);
    memcpy(s_dst, s_unicast ? peer_mac : BCAST, 6);

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_dst, 6);
    peer.channel = LINK_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    if (s_unicast) {
        ESP_LOGI(TAG, "ESP-NOW pronto (canal %d), unicast para %02x:%02x:%02x:%02x:%02x:%02x",
                 LINK_CHANNEL, s_dst[0], s_dst[1], s_dst[2], s_dst[3], s_dst[4], s_dst[5]);
    } else {
        ESP_LOGW(TAG, "ESP-NOW pronto (canal %d), RELAY_PEER_MAC nao configurado: usando broadcast",
                 LINK_CHANNEL);
    }
    return ESP_OK;
}

esp_err_t link_send(link_msg_type_t type, const uint8_t *data, size_t len)
{
    uint8_t buf[ESP_NOW_MAX_DATA_LEN];
    if (len + 1 > sizeof buf) return ESP_ERR_INVALID_SIZE;
    buf[0] = (uint8_t) type;
    if (len) memcpy(buf + 1, data, len);

    s_tx_total++;
    esp_err_t rc = esp_now_send(s_dst, buf, len + 1);
    if (rc != ESP_OK) s_tx_fail++;   /* erro sincrono: o callback nao vai rodar */
    return rc;
}

void link_tx_stats(uint32_t *falhas, uint32_t *total)
{
    if (falhas) *falhas = s_tx_fail;
    if (total)  *total  = s_tx_total;
}

esp_err_t link_set_tx_power(int8_t power_qdbm)
{
    if (power_qdbm < 8)  power_qdbm = 8;    /* ~2 dBm (minimo) */
    if (power_qdbm > 84) power_qdbm = 84;   /* ~20 dBm (maximo) */
    esp_err_t e = esp_wifi_set_max_tx_power(power_qdbm);
    ESP_LOGI(TAG, "TX power -> %d (0.25dBm) = %.2f dBm (rc=%d)",
             power_qdbm, power_qdbm / 4.0, (int) e);
    return e;
}

int8_t link_get_tx_power(void)
{
    int8_t p = 0;
    esp_wifi_get_max_tx_power(&p);
    return p;
}
