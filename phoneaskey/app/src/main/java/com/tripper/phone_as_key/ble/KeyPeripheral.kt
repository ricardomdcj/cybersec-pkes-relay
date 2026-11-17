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

/** One log line with a relative timestamp (ms since the key started). */
data class LogLine(val tMs: Long, val text: String)

data class PeripheralState(
    val status: PeripheralStatus = PeripheralStatus.IDLE,
    val peer: String? = null,
    val log: List<LogLine> = emptyList()
)

/**
 * The digital key: BLE peripheral + GATT server that answers the vehicle's challenge.
 *
 * The permissions (BLUETOOTH_ADVERTISE / BLUETOOTH_CONNECT on Android 12+) are checked in the
 * Activity BEFORE start() is called; hence the SuppressLint here.
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

    // ---- state / log -------------------------------------------------------

    private fun log(msg: String) {
        val line = LogLine(SystemClock.elapsedRealtime() - t0, msg)
        val cur = _state.value
        _state.value = cur.copy(log = (cur.log + line).takeLast(200))
    }

    private fun setStatus(s: PeripheralStatus, peer: String? = _state.value.peer) {
        _state.value = _state.value.copy(status = s, peer = peer)
    }

    // ---- lifecycle ----------------------------------------------------------

    fun start() {
        if (adapter == null) { log("Bluetooth adapter missing"); setStatus(PeripheralStatus.ERROR); return }
        if (!adapter.isEnabled) { log("Bluetooth is off, turn it on and try again"); setStatus(PeripheralStatus.ERROR); return }
        advertiser = adapter.bluetoothLeAdvertiser
        if (advertiser == null) { log("This radio does not support BLE advertising"); setStatus(PeripheralStatus.ERROR); return }

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

        // advertising only starts in onServiceAdded, which ensures the GATT server is ready.
        gattServer?.addService(service)
        log("GATT server created, adding service...")
    }

    fun stop() {
        try { advertiser?.stopAdvertising(advCallback) } catch (_: Exception) {}
        try { gattServer?.close() } catch (_: Exception) {}
        gattServer = null
        subscribers.clear()
        setStatus(PeripheralStatus.IDLE, peer = null)
        log("Stopped")
    }

    // ---- advertising --------------------------------------------------------

    private fun startAdvertising() {
        val settings = AdvertiseSettings.Builder()
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
            .setConnectable(true)
            .build()
        // The device name goes in the scan response, so the 31-byte advertising packet does not overflow.
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
            log("Advertising service ${BleContract.SERVICE_UUID}")
        }
        override fun onStartFailure(errorCode: Int) {
            setStatus(PeripheralStatus.ERROR)
            log("Advertising failed (code $errorCode)")
        }
    }

    // ---- GATT server --------------------------------------------------------

    private val gattCallback = object : BluetoothGattServerCallback() {

        override fun onServiceAdded(status: Int, service: BluetoothGattService?) {
            log("Service added (status $status), starting advertising")
            startAdvertising()
        }

        override fun onConnectionStateChange(device: BluetoothDevice, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    setStatus(PeripheralStatus.CONNECTED, peer = device.address)
                    log("Connected: ${device.address}")
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    subscribers.remove(device)
                    setStatus(PeripheralStatus.ADVERTISING, peer = null)
                    log("Disconnected: ${device.address}")
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
                log("Challenge received (${data.size} B): ${BleContract.toHex(data)}")
                if (responseNeeded) {
                    gattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, data)
                }
                lastResponse = BleContract.computeResponse(data)
                notifyResponse(device, lastResponse)
                val dtUs = (SystemClock.elapsedRealtimeNanos() - tRxNs) / 1000
                log("Response sent (${lastResponse.size} B) in ${dtUs} us: ${BleContract.toHex(lastResponse)}")
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
                    subscribers.add(device); log("Notifications enabled by ${device.address}")
                } else {
                    subscribers.remove(device); log("Notifications disabled by ${device.address}")
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
