#pragma once

/*
 * OTA serial minimo pela UART0 (a mesma do console/CH340), para regravar a CYD sem
 * precisar entrar em download mode / apertar BOOT a cada flash.
 *
 * O firmware que ja esta rodando escuta linhas "OTA:*" na UART0 e grava a imagem
 * recebida na outra particao de app (esp_ota), depois reinicia nela. O host envia com
 * tools/serial_ota.py. A PRIMEIRA gravacao de cada placa ainda e via esptool (bootstrap);
 * dai em diante as atualizacoes sao por aqui, sem botao.
 *
 * Requer tabela de particoes com dois slots de app OTA (CONFIG_PARTITION_TABLE_TWO_OTA_LARGE).
 *
 * Protocolo (linhas terminadas em \n, convivem com os logs do console que o host ignora):
 *   host -> "OTA?"              dev -> "OTA:ID <proj> part=<slot>"
 *   host -> "OTA:BEGIN <size>"  dev -> "OTA:READY part=<slot>"  (ou "OTA:ERR <motivo>")
 *   host -> "OTA:D <base64>"    dev -> "OTA:ACK <bytes>"        (um por bloco, controle de fluxo)
 *   host -> "OTA:END"           dev -> "OTA:OK <bytes>"  e reinicia na imagem nova
 *   host -> "OTA:ABORT"         dev -> "OTA:ABORTED"
 */

void serial_ota_init(void);

/* Handler opcional para linhas da UART0 que NAO comecam com "OTA" (ex.: comandos de
 * bancada como "TXPOW <n>"). Reaproveita o mesmo leitor da UART0 do OTA, evitando um
 * segundo driver na porta. Registre antes/depois de serial_ota_init(). */
typedef void (*serial_ota_cmd_cb_t)(const char *line);
void serial_ota_set_cmd_handler(serial_ota_cmd_cb_t cb);
