#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/*
 * Side link between the two relays (ESP-NOW). The relays tunnel the BLE exchange:
 * the challenge travels B -> A, the response travels A -> B.
 *
 * Frame format (little-endian, no padding):
 *   CHALLENGE  B -> A : [seq:4][nonce:16]                        = LINK_CHAL_LEN (20 B)
 *   RESPONSE   A -> B : [seq:4][turnaround of A in us:4][hmac:16] = LINK_RESP_LEN (24 B)
 *
 * seq is the round number: Relay B assigns a seq to each vehicle challenge, and Relay A
 * returns the same seq in its response. So B only delivers to the vehicle the response to
 * the pending challenge; a late or duplicate response is discarded (M-04).
 * A's turnaround travels along, so B can break down the round's time (M-05).
 */
typedef enum {
    LINK_MSG_CHALLENGE = 1,  /* B -> A : seq + nonce from the vehicle */
    LINK_MSG_RESPONSE  = 2,  /* A -> B : seq + turnaround of A + response (HMAC) from the key */
} link_msg_type_t;

#define LINK_SEQ_LEN   4
#define LINK_CHAL_LEN  (LINK_SEQ_LEN + 16)          /* 16 = NONCE_LEN (pkes_contract) */
#define LINK_RESP_LEN  (LINK_SEQ_LEN + 4 + 16)      /* 16 = RESPONSE_LEN (pkes_contract) */

/* Called when a frame from the other relay arrives. Runs in the WiFi task context. */
typedef void (*link_rx_cb_t)(link_msg_type_t type, const uint8_t *data, size_t len);

/* Brings up WiFi (STA, not associated) and ESP-NOW on a fixed channel, and registers the callback. */
esp_err_t link_init(link_rx_cb_t cb);

/* Sends a frame to the other relay (unicast to RELAY_PEER_MAC, or broadcast if it is not set).
 * Payload up to 249 bytes. */
esp_err_t link_send(link_msg_type_t type, const uint8_t *data, size_t len);

/* Send counters of this relay since boot (M-07): failures (an error in the call, or a failure
 * status from the callback) and total attempts. They are cumulative: the difference between
 * rounds is what matters. Either pointer may be NULL. */
void link_tx_stats(uint32_t *failures, uint32_t *total);

/* Sets the radio's maximum TX power (this changes the range of the ESP-NOW tunnel).
 * Unit: 0.25 dBm; valid range ~[8, 84] = 2..20 dBm. Less power = weaker signal =
 * simulates a longer distance in a small space. Can be changed at run time. */
esp_err_t link_set_tx_power(int8_t power_qdbm);
int8_t    link_get_tx_power(void);
