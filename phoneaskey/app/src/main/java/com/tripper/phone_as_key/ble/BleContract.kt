package com.tripper.phone_as_key.ble

import java.util.UUID
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

/**
 * GATT contract shared by the app (the key) and the "vehicle" firmware (ESP32).
 *
 * Any change here must be mirrored on the ESP32 side. These UUIDs, the challenge-response
 * format and the pre-shared secret are the bench's "protocol".
 *
 * Flow (the key's role = peripheral + GATT server):
 *   1. the vehicle (central) connects and enables notifications on the RESPONSE characteristic (CCCD);
 *   2. the vehicle writes a NONCE_LEN-byte nonce to the CHALLENGE characteristic;
 *   3. the key computes HMAC-SHA256(PSK, nonce), truncates it to RESPONSE_LEN bytes and notifies.
 *
 * The HMAC proves possession of the secret AND liveness (the nonce changes every round, so a
 * naive replay fails). The relay still works because it carries the live exchange: it forges
 * distance, not identity. That is the thesis of the work.
 */
object BleContract {

    val SERVICE_UUID: UUID = UUID.fromString("8f2a0001-1b2c-4e6a-9f3d-2a7c1e5b0a10")

    /** WRITE (vehicle -> key): NONCE_LEN-byte nonce. */
    val CHALLENGE_UUID: UUID = UUID.fromString("8f2a0002-1b2c-4e6a-9f3d-2a7c1e5b0a10")

    /** NOTIFY/READ (key -> vehicle): RESPONSE_LEN-byte MAC. */
    val RESPONSE_UUID: UUID = UUID.fromString("8f2a0003-1b2c-4e6a-9f3d-2a7c1e5b0a10")

    /** Standard Client Characteristic Configuration Descriptor (enables notifications). */
    val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    const val NONCE_LEN = 16
    const val RESPONSE_LEN = 16

    /**
     * Bench-only pre-shared secret (the text "PKES-RELAY-BENCH").
     * Key confidentiality is NOT part of the attack surface under study; so it is fixed and
     * identical on both sides. The same array must exist in the vehicle firmware.
     */
    val PSK: ByteArray = byteArrayOf(
        0x50, 0x4b, 0x45, 0x53, 0x2d, 0x52, 0x45, 0x4c,
        0x41, 0x59, 0x2d, 0x42, 0x45, 0x4e, 0x43, 0x48
    )

    /** Computes the challenge response: HMAC-SHA256(PSK, nonce) truncated to RESPONSE_LEN bytes. */
    fun computeResponse(nonce: ByteArray): ByteArray {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(PSK, "HmacSHA256"))
        return mac.doFinal(nonce).copyOf(RESPONSE_LEN)
    }

    fun toHex(bytes: ByteArray): String = bytes.joinToString("") { "%02x".format(it) }
}
