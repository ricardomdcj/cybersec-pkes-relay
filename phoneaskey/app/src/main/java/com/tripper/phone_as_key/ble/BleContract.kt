package com.tripper.phone_as_key.ble

import java.util.UUID
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

/**
 * Contrato GATT compartilhado entre o app (chave) e o firmware do "veiculo" (ESP32).
 *
 * Qualquer mudanca aqui precisa ser espelhada no lado do ESP32. Estes UUIDs, o formato
 * do desafio-resposta e o segredo pre-compartilhado sao o "protocolo" da bancada.
 *
 * Fluxo (papel da chave = peripheral + servidor GATT):
 *   1. veiculo (central) conecta e habilita notificacoes na caracteristica RESPONSE (CCCD);
 *   2. veiculo escreve um nonce de NONCE_LEN bytes na caracteristica CHALLENGE;
 *   3. a chave calcula HMAC-SHA256(PSK, nonce), trunca em RESPONSE_LEN bytes e notifica.
 *
 * O HMAC prova posse do segredo E vivacidade (o nonce muda a cada rodada, entao replay
 * ingenuo nao passa). O relay ainda funciona porque transporta a troca ao vivo: ele forja
 * distancia, nao identidade. Essa e a tese do trabalho.
 */
object BleContract {

    val SERVICE_UUID: UUID = UUID.fromString("8f2a0001-1b2c-4e6a-9f3d-2a7c1e5b0a10")

    /** WRITE (veiculo -> chave): nonce de NONCE_LEN bytes. */
    val CHALLENGE_UUID: UUID = UUID.fromString("8f2a0002-1b2c-4e6a-9f3d-2a7c1e5b0a10")

    /** NOTIFY/READ (chave -> veiculo): MAC de RESPONSE_LEN bytes. */
    val RESPONSE_UUID: UUID = UUID.fromString("8f2a0003-1b2c-4e6a-9f3d-2a7c1e5b0a10")

    /** Client Characteristic Configuration Descriptor padrao (habilita notificacao). */
    val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    const val NONCE_LEN = 16
    const val RESPONSE_LEN = 16

    /**
     * Segredo pre-compartilhado APENAS de bancada (o texto "PKES-RELAY-BENCH").
     * A confidencialidade da chave NAO faz parte da superficie de ataque estudada; por isso
     * ela e fixa e identica nos dois lados. O mesmo array precisa existir no firmware do veiculo.
     */
    val PSK: ByteArray = byteArrayOf(
        0x50, 0x4b, 0x45, 0x53, 0x2d, 0x52, 0x45, 0x4c,
        0x41, 0x59, 0x2d, 0x42, 0x45, 0x4e, 0x43, 0x48
    )

    /** Calcula a resposta ao desafio: HMAC-SHA256(PSK, nonce) truncado em RESPONSE_LEN bytes. */
    fun computeResponse(nonce: ByteArray): ByteArray {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(PSK, "HmacSHA256"))
        return mac.doFinal(nonce).copyOf(RESPONSE_LEN)
    }

    fun toHex(bytes: ByteArray): String = bytes.joinToString("") { "%02x".format(it) }
}
