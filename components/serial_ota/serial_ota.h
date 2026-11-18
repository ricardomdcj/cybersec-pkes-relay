#pragma once

/*
 * Minimal serial OTA over UART0 (the same port as the console / CH340), to reflash the CYD
 * without entering download mode or pressing BOOT on every flash.
 *
 * The firmware that is already running listens for "OTA:*" lines on UART0, writes the
 * received image to the other app partition (esp_ota), and then reboots into it. The host
 * sends it with tools/serial_ota.py. The FIRST flash of each board still goes through esptool
 * (bootstrap); from then on, updates go through this path, without the button.
 *
 * Requires a partition table with two OTA app slots (CONFIG_PARTITION_TABLE_TWO_OTA_LARGE).
 *
 * Protocol (lines end with \n; they coexist with console logs, which the host ignores):
 *   host -> "OTA?"              dev -> "OTA:ID <proj> part=<slot>"
 *   host -> "OTA:BEGIN <size>"  dev -> "OTA:READY part=<slot>"  (or "OTA:ERR <reason>")
 *   host -> "OTA:D <base64>"    dev -> "OTA:ACK <bytes>"        (one per block, flow control)
 *   host -> "OTA:END"           dev -> "OTA:OK <bytes>" and reboots into the new image
 *   host -> "OTA:ABORT"         dev -> "OTA:ABORTED"
 */

void serial_ota_init(void);

/* Optional handler for UART0 lines that do NOT start with "OTA" (e.g. bench commands such as
 * "TXPOW <n>"). It reuses the same UART0 reader as OTA, so there is no second driver on the
 * port. Register it before or after serial_ota_init(). */
typedef void (*serial_ota_cmd_cb_t)(const char *line);
void serial_ota_set_cmd_handler(serial_ota_cmd_cb_t cb);
