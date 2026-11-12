package com.tripper.phone_as_key

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.tripper.phone_as_key.ble.BleContract
import com.tripper.phone_as_key.ble.KeyPeripheral
import com.tripper.phone_as_key.ble.PeripheralStatus
import com.tripper.phone_as_key.ui.theme.PhoneaskeyTheme

class MainActivity : ComponentActivity() {

    private lateinit var peripheral: KeyPeripheral

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        peripheral = KeyPeripheral(this)
        enableEdgeToEdge()
        setContent {
            PhoneaskeyTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    KeyScreen(peripheral, Modifier.padding(innerPadding))
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        peripheral.stop()
    }
}

/** Permissoes de runtime necessarias no Android 12+ (nenhuma abaixo disso). */
private fun requiredBlePerms(): Array<String> =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
        arrayOf(Manifest.permission.BLUETOOTH_ADVERTISE, Manifest.permission.BLUETOOTH_CONNECT)
    else emptyArray()

private fun permsGranted(context: Context, perms: Array<String>): Boolean =
    perms.all { ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED }

@Composable
fun KeyScreen(peripheral: KeyPeripheral, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val state by peripheral.state.collectAsState()
    val perms = remember { requiredBlePerms() }
    var granted by remember { mutableStateOf(permsGranted(context, perms)) }

    val launcher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        granted = perms.isEmpty() || result.values.all { it }
        if (granted) peripheral.start()
    }

    val idle = state.status == PeripheralStatus.IDLE || state.status == PeripheralStatus.ERROR

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("phone-as-key", style = MaterialTheme.typography.headlineSmall)
        Text(
            "Chave digital BLE (peripheral + servidor GATT)",
            style = MaterialTheme.typography.bodyMedium
        )

        StatusRow(state.status, state.peer)

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = {
                    if (perms.isEmpty() || permsGranted(context, perms)) {
                        granted = true
                        peripheral.start()
                    } else {
                        launcher.launch(perms)
                    }
                },
                enabled = idle
            ) { Text("Iniciar chave") }

            OutlinedButton(
                onClick = { peripheral.stop() },
                enabled = !idle
            ) { Text("Parar") }
        }

        Text(
            "Servico: ${BleContract.SERVICE_UUID}",
            style = MaterialTheme.typography.labelSmall,
            fontFamily = FontFamily.Monospace
        )

        HorizontalDivider()
        Text("Eventos", style = MaterialTheme.typography.titleSmall)
        LazyColumn(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(2.dp)
        ) {
            items(state.log.asReversed()) { line ->
                Text(
                    "[%6d ms] %s".format(line.tMs, line.text),
                    style = MaterialTheme.typography.bodySmall,
                    fontFamily = FontFamily.Monospace,
                    fontSize = 12.sp
                )
            }
        }
    }
}

@Composable
private fun StatusRow(status: PeripheralStatus, peer: String?) {
    val (label, color) = when (status) {
        PeripheralStatus.IDLE -> "Parada" to MaterialTheme.colorScheme.outline
        PeripheralStatus.ADVERTISING -> "Anunciando" to MaterialTheme.colorScheme.primary
        PeripheralStatus.CONNECTED ->
            ("Conectada" + (peer?.let { " ($it)" } ?: "")) to MaterialTheme.colorScheme.tertiary
        PeripheralStatus.ERROR -> "Erro" to MaterialTheme.colorScheme.error
    }
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text("Estado:", style = MaterialTheme.typography.titleSmall)
        Text(label, color = color, style = MaterialTheme.typography.titleSmall)
    }
}
