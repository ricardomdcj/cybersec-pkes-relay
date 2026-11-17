# Monitora as 4 CYDs da bancada ao mesmo tempo: multiplexa as 4 seriais num unico log
# com timestamp do PC + tag de papel, para correlacionar as rodadas pelo nonce.
#
#   powershell -ExecutionPolicy Bypass -File tools\monitor4.ps1 -seconds 120 -out run.log
#
# Abre as portas SEM mexer em DTR/RTS (nao reseta as placas). Ajuste o mapa de portas
# se os numeros COM mudarem (CH340 re-enumera ao religar).
param(
    [int]$seconds = 120,
    [string]$out = "run.log",
    [string]$key = "COM5",
    [string]$relayA = "COM7",
    [string]$relayB = "COM8",
    [string]$vehicle = "COM6",
    [switch]$resetVehicle   # pulsa RTS no carro ao iniciar, p/ disparar uma campanha nova de rodadas
)

$map = [ordered]@{ "KEY" = $key; "RELAY_A" = $relayA; "RELAY_B" = $relayB; "VEHICLE" = $vehicle }
$sp = @{}
$buf = @{}
foreach ($role in $map.Keys) {
    $x = New-Object System.IO.Ports.SerialPort $map[$role], 115200, None, 8, One
    $x.ReadTimeout = 50
    try { $x.Open(); $x.DtrEnable = $false; $x.RtsEnable = $false } catch { Write-Output "falha ao abrir $($map[$role]) ($role): $_" }
    $sp[$role] = $x
    $buf[$role] = ""
}

if ($resetVehicle -and $sp["VEHICLE"].IsOpen) {
    # Reset do carro para recomecar a campanha de rodadas (BLE reconecta e refaz ROUNDS).
    $sp["VEHICLE"].RtsEnable = $true; Start-Sleep -Milliseconds 150; $sp["VEHICLE"].RtsEnable = $false
}

"# monitor iniciado $(Get-Date -Format o) | KEY=$key RELAY_A=$relayA RELAY_B=$relayB VEHICLE=$vehicle" | Out-File -Encoding utf8 $out
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $seconds) {
    foreach ($role in $map.Keys) {
        if (-not $sp[$role].IsOpen) { continue }
        try { $buf[$role] += $sp[$role].ReadExisting() } catch {}
        while ($buf[$role].Contains("`n")) {
            $i = $buf[$role].IndexOf("`n")
            $line = $buf[$role].Substring(0, $i).TrimEnd("`r")
            $buf[$role] = $buf[$role].Substring($i + 1)
            if ($line.Length -gt 0) {
                ("{0:HH:mm:ss.fff} [{1,-7}] {2}" -f (Get-Date), $role, $line) | Out-File -Append -Encoding utf8 $out
            }
        }
    }
    Start-Sleep -Milliseconds 20
}
foreach ($role in $map.Keys) { if ($sp[$role].IsOpen) { $sp[$role].Close() } }
"# monitor encerrado $(Get-Date -Format o)" | Out-File -Append -Encoding utf8 $out
