package com.tripper.phone_as_key.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattServer
import android.bluetooth.BluetoothGattServerCallback
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.content.Context
import android.os.Build
import android.os.ParcelUuid
import android.os.SystemClock
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

enum class PeripheralStatus { IDLE, ADVERTISING, CONNECTED, ERROR }

/** Uma linha de log com carimbo de tempo relativo (ms desde o start da chave). */
data class LogLine(val tMs: Long, val text: String)

data class PeripheralState(
    val status: PeripheralStatus = PeripheralStatus.IDLE,
    val peer: String? = null,
    val log: List<LogLine> = emptyList()
)

/**
 * A chave digital: peripheral BLE + servidor GATT que responde ao desafio do veiculo.
 *
 * As permissoes (BLUETOOTH_ADVERTISE / BLUETOOTH_CONNECT no Android 12+) sao verificadas
 * na Activity ANTES de start() ser chamado; por isso o SuppressLint aqui.
 */
@SuppressLint("MissingPermission")
class KeyPeripheral(context: Context) {

    private val appContext = context.applicationContext
    private val btManager = appContext.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val adapter: BluetoothAdapter? = btManager.adapter

    private var gattServer: BluetoothGattServer? = null
    private var advertiser: BluetoothLeAdvertiser? = null
    private var responseChar: BluetoothGattCharacteristic? = null
    private var lastResponse: ByteArray = ByteArray(0)

    private val subscribers = mutableSetOf<BluetoothDevice>()
    private var t0 = SystemClock.elapsedRealtime()

    private val _state = MutableStateFlow(PeripheralState())
    val state: StateFlow<PeripheralState> = _state.asStateFlow()

    fun isBluetoothReady(): Boolean = adapter?.isEnabled == true

    // ---- estado / log -------------------------------------------------------

    private fun log(msg: String) {
        val line = LogLine(SystemClock.elapsedRealtime() - t0, msg)
        val cur = _state.value
        _state.value = cur.copy(log = (cur.log + line).takeLast(200))
    }

    private fun setStatus(s: PeripheralStatus, peer: String? = _state.value.peer) {
        _state.value = _state.value.copy(status = s, peer = peer)
    }

    // ---- ciclo de vida ------------------------------------------------------

    fun start() {
        if (adapter == null) { log("Adaptador Bluetooth ausente"); setStatus(PeripheralStatus.ERROR); return }
        if (!adapter.isEnabled) { log("Bluetooth desligado, ative e tente de novo"); setStatus(PeripheralStatus.ERROR); return }
        advertiser = adapter.bluetoothLeAdvertiser
        if (advertiser == null) { log("Este radio nao suporta advertising BLE"); setStatus(PeripheralStatus.ERROR); return }

        t0 = SystemClock.elapsedRealtime()
        _state.value = PeripheralState(status = PeripheralStatus.IDLE)

        gattServer = btManager.openGattServer(appContext, gattCallback)

        val service = BluetoothGattService(BleContract.SERVICE_UUID, BluetoothGattService.SERVICE_TYPE_PRIMARY)

        val challenge = BluetoothGattCharacteristic(
            BleContract.CHALLENGE_UUID,
            BluetoothGattCharacteristic.PROPERTY_WRITE or BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
            BluetoothGattCharacteristic.PERMISSION_WRITE
        )
        val response = BluetoothGattCharacteristic(
            BleContract.RESPONSE_UUID,
            BluetoothGattCharacteristic.PROPERTY_READ or BluetoothGattCharacteristic.PROPERTY_NOTIFY,
            BluetoothGattCharacteristic.PERMISSION_READ
        )
        response.addDescriptor(
            BluetoothGattDescriptor(
                BleContract.CCCD_UUID,
                BluetoothGattDescriptor.PERMISSION_READ or BluetoothGattDescriptor.PERMISSION_WRITE
            )
        )
        service.addCharacteristic(challenge)
        service.addCharacteristic(response)
        responseChar = response

        // advertising so comeca em onServiceAdded, garantindo o GATT pronto.
        gattServer?.addService(service)
        log("Servidor GATT criado, adicionando servico...")
    }

    fun stop() {
        try { advertiser?.stopAdvertising(advCallback) } catch (_: Exception) {}
        try { gattServer?.close() } catch (_: Exception) {}
        gattServer = null
        subscribers.clear()
        setStatus(PeripheralStatus.IDLE, peer = null)
        log("Parada")
    }

    // ---- advertising --------------------------------------------------------

    private fun startAdvertising() {
        val settings = AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
            .setConnectable(true)
            .build()
        // O nome do aparelho vai na scan response para nao estourar os 31 bytes do pacote.
        val advData = AdvertiseData.Builder()
            .setIncludeDeviceName(false)
            .addServiceUuid(ParcelUuid(BleContract.SERVICE_UUID))
            .build()
        val scanResponse = AdvertiseData.Builder()
            .setIncludeDeviceName(true)
            .build()
        advertiser?.startAdvertising(settings, advData, scanResponse, advCallback)
    }

    private val advCallback = object : AdvertiseCallback() {
        override fun onStartSuccess(settingsInEffect: AdvertiseSettings?) {
            setStatus(PeripheralStatus.ADVERTISING)
            log("Anunciando servico ${BleContract.SERVICE_UUID}")
        }
        override fun onStartFailure(errorCode: Int) {
            setStatus(PeripheralStatus.ERROR)
            log("Falha ao anunciar (codigo $errorCode)")
        }
    }

    // ---- servidor GATT ------------------------------------------------------

    private val gattCallback = object : BluetoothGattServerCallback() {

        override fun onServiceAdded(status: Int, service: BluetoothGattService?) {
            log("Servico adicionado (status $status), iniciando advertising")
            startAdvertising()
        }

        override fun onConnectionStateChange(device: BluetoothDevice, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    setStatus(PeripheralStatus.CONNECTED, peer = device.address)
                    log("Conectado: ${device.address}")
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    subscribers.remove(device)
                    setStatus(PeripheralStatus.ADVERTISING, peer = null)
                    log("Desconectado: ${device.address}")
                }
            }
        }

        override fun onCharacteristicWriteRequest(
            device: BluetoothDevice, requestId: Int,
            characteristic: BluetoothGattCharacteristic,
            preparedWrite: Boolean, responseNeeded: Boolean,
            offset: Int, value: ByteArray?
        ) {
            val data = value ?: ByteArray(0)
            if (characteristic.uuid == BleContract.CHALLENGE_UUID) {
                val tRxNs = SystemClock.elapsedRealtimeNanos()
                log("Desafio recebido (${data.size} B): ${BleContract.toHex(data)}")
                if (responseNeeded) {
                    gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, data)
                }
                lastResponse = BleContract.computeResponse(data)
                notifyResponse(device, lastResponse)
                val dtUs = (SystemClock.elapsedRealtimeNanos() - tRxNs) / 1000
                log("Resposta enviada (${lastResponse.size} B) em ${dtUs} us: ${BleContract.toHex(lastResponse)}")
            } else if (responseNeeded) {
                gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, offset, null)
            }
        }

        override fun onCharacteristicReadRequest(
            device: BluetoothDevice, requestId: Int, offset: Int,
            characteristic: BluetoothGattCharacteristic
        ) {
            val value = if (characteristic.uuid == BleContract.RESPONSE_UUID) lastResponse else ByteArray(0)
            val slice = if (offset >= value.size) ByteArray(0) else value.copyOfRange(offset, value.size)
            gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, slice)
        }

        override fun onDescriptorWriteRequest(
            device: BluetoothDevice, requestId: Int,
            descriptor: BluetoothGattDescriptor,
            preparedWrite: Boolean, responseNeeded: Boolean,
            offset: Int, value: ByteArray?
        ) {
            if (descriptor.uuid == BleContract.CCCD_UUID) {
                val enable = value != null && value.isNotEmpty() && value[0].toInt() != 0
                if (enable) {
                    subscribers.add(device); log("Notificacoes habilitadas por ${device.address}")
                } else {
                    subscribers.remove(device); log("Notificacoes desabilitadas por ${device.address}")
                }
            }
            if (responseNeeded) {
                gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, value ?: ByteArray(0))
            }
        }
    }

    private fun notifyResponse(device: BluetoothDevice, value: ByteArray) {
        val ch = responseChar ?: return
        val server = gattServer ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            server.notifyCharacteristicChanged(device, ch, false, value)
        } else {
            @Suppress("DEPRECATION")
            run {
                ch.value = value
                server.notifyCharacteristicChanged(device, ch, false)
            }
        }
    }
}
