#include "serial_ota.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "driver/uart.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OTA_UART   UART_NUM_0
#define OTA_LINE_MAX   1200          /* cabe um bloco de 512 B em base64 (~684 ch) + prefixo */

static const char *TAG = "serial_ota";

static esp_ota_handle_t        s_h;
static const esp_partition_t  *s_part;
static bool                    s_busy;
static size_t                  s_written;
static serial_ota_cmd_cb_t     s_cmd_cb;

void serial_ota_set_cmd_handler(serial_ota_cmd_cb_t cb) { s_cmd_cb = cb; }

static void reply(const char *s) {
    uart_write_bytes(OTA_UART, s, strlen(s));
    uart_write_bytes(OTA_UART, "\r\n", 2);
}

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) sobre os bytes decodificados do bloco:
 * pega bit-flips que mantem o base64 estruturalmente valido (o que falhava so no
 * esp_ota_end, tarde demais). */
static uint16_t crc16(const uint8_t *d, size_t n) {
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        c ^= (uint16_t) d[i] << 8;
        for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}

static void abort_ota(const char *why) {
    if (s_busy) { esp_ota_abort(s_h); s_busy = false; }
    char buf[64];
    snprintf(buf, sizeof buf, "OTA:ERR %s", why);
    reply(buf);
}

static void handle_line(char *line) {
    if (strncmp(line, "OTA?", 4) == 0) {
        const esp_partition_t *run = esp_ota_get_running_partition();
        const esp_app_desc_t *d = esp_app_get_description();
        char buf[96];
        snprintf(buf, sizeof buf, "OTA:ID %s part=%s crc=1",
                 d ? d->project_name : "?", run ? run->label : "?");
        reply(buf);
        return;
    }

    if (strncmp(line, "OTA:BEGIN", 9) == 0) {
        if (s_busy) { esp_ota_abort(s_h); s_busy = false; }
        s_part = esp_ota_get_next_update_partition(NULL);
        if (!s_part) { reply("OTA:ERR no-ota-part"); return; }
        if (esp_ota_begin(s_part, OTA_SIZE_UNKNOWN, &s_h) != ESP_OK) {
            reply("OTA:ERR begin");
            return;
        }
        s_busy = true;
        s_written = 0;
        char buf[64];
        snprintf(buf, sizeof buf, "OTA:READY part=%s", s_part->label);
        reply(buf);
        ESP_LOGW(TAG, "OTA iniciada -> particao %s", s_part->label);
        return;
    }

    if (strncmp(line, "OTA:D ", 6) == 0) {
        if (!s_busy) { reply("OTA:ERR not-begun"); return; }
        /* Formato novo (host com crc): "OTA:D <crc16hex> <b64>". Um espaco separa o CRC
         * do base64; como base64 nunca tem espaco, a presenca dele identifica o formato.
         * Sem espaco = formato legado (so b64), aceito p/ compatibilidade. */
        const char *b64 = line + 6;
        int have_crc = 0;
        uint16_t want_crc = 0;
        const char *sp = strchr(line + 6, ' ');
        if (sp) {
            want_crc = (uint16_t) strtol(line + 6, NULL, 16);
            b64 = sp + 1;
            have_crc = 1;
        }
        static uint8_t data[768];
        size_t olen = 0;
        /* b64 invalido = linha corrompida na serial (byte perdido): NAK e NAO aborta;
         * nada foi escrito, entao o host reenvia o mesmo bloco. Robustez ao ruido. */
        if (mbedtls_base64_decode(data, sizeof data, &olen,
                                  (const uint8_t *) b64, strlen(b64)) != 0) {
            reply("OTA:NAK");
            return;
        }
        /* CRC confere o CONTEUDO (pega bit-flip que o base64 nao detecta). */
        if (have_crc && crc16(data, olen) != want_crc) {
            reply("OTA:NAK");
            return;
        }
        if (esp_ota_write(s_h, data, olen) != ESP_OK) {
            abort_ota("write");   /* falha de escrita e fatal */
            return;
        }
        s_written += olen;
        char buf[48];
        snprintf(buf, sizeof buf, "OTA:ACK %u", (unsigned) s_written);
        reply(buf);
        return;
    }

    if (strncmp(line, "OTA:END", 7) == 0) {
        if (!s_busy) { reply("OTA:ERR not-begun"); return; }
        esp_err_t e = esp_ota_end(s_h);
        s_busy = false;
        if (e != ESP_OK) { reply("OTA:ERR end"); return; }
        if (esp_ota_set_boot_partition(s_part) != ESP_OK) { reply("OTA:ERR setboot"); return; }
        char buf[48];
        snprintf(buf, sizeof buf, "OTA:OK %u", (unsigned) s_written);
        reply(buf);
        ESP_LOGW(TAG, "OTA concluida (%u B); reiniciando na imagem nova", (unsigned) s_written);
        vTaskDelay(pdMS_TO_TICKS(400));
        esp_restart();
        return;
    }

    if (strncmp(line, "OTA:ABORT", 9) == 0) {
        if (s_busy) { esp_ota_abort(s_h); s_busy = false; }
        reply("OTA:ABORTED");
        return;
    }
}

static void ota_task(void *arg) {
    static char line[OTA_LINE_MAX];
    size_t len = 0;
    bool overflow = false;
    uint8_t buf[256];
    for (;;) {
        int n = uart_read_bytes(OTA_UART, buf, sizeof buf, pdMS_TO_TICKS(100));
        for (int i = 0; i < n; i++) {
            char c = (char) buf[i];
            if (c == '\n' || c == '\r') {
                if (len > 0 && !overflow) {
                    line[len] = 0;
                    if (strncmp(line, "OTA", 3) == 0) handle_line(line);
                    else if (s_cmd_cb) s_cmd_cb(line);   /* comandos de bancada (TXPOW...) */
                }
                len = 0;
                overflow = false;
            } else if (len < OTA_LINE_MAX - 1) {
                line[len++] = c;
            } else {
                overflow = true;   /* linha grande demais: descarta */
            }
        }
    }
}

void serial_ota_init(void) {
    /* UART0 e compartilhada com o console (igual a bancada deskrab): instala o driver so
     * para LER o RX; os logs continuam saindo pelo TX e o host ignora o que nao e "OTA:". */
    if (uart_driver_install(OTA_UART, 4096, 0, 0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install falhou; OTA serial indisponivel");
        return;
    }
    xTaskCreate(ota_task, "serial_ota", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "OTA serial pronto na UART0 (host: tools/serial_ota.py)");
}
