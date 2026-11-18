# Monitors the 4 CYDs of the bench at the same time: multiplexes the 4 serial ports into a
# single log with a PC timestamp and a role tag, to correlate the rounds by nonce.
#
#   powershell -ExecutionPolicy Bypass -File tools\monitor4.ps1 -seconds 120 -out run.log
#
# Opens the ports WITHOUT touching DTR/RTS (it does not reset the boards). Adjust the port map
# if the COM numbers change (the CH340 re-enumerates when the board is powered back on).
param(
    [int]$seconds = 120,
    [string]$out = "run.log",
    [string]$key = "COM5",
    [string]$relayA = "COM7",
    [string]$relayB = "COM8",
    [string]$vehicle = "COM6",
    [switch]$resetVehicle   # pulses RTS on the vehicle at start, to trigger a new round campaign
)

$map = [ordered]@{ "KEY" = $key; "RELAY_A" = $relayA; "RELAY_B" = $relayB; "VEHICLE" = $vehicle }
$sp = @{}
$buf = @{}
foreach ($role in $map.Keys) {
    $x = New-Object System.IO.Ports.SerialPort $map[$role], 115200, None, 8, One
    $x.ReadTimeout = 50
    try { $x.Open(); $x.DtrEnable = $false; $x.RtsEnable = $false } catch { Write-Output "failed to open $($map[$role]) ($role): $_" }
    $sp[$role] = $x
    $buf[$role] = ""
}

if ($resetVehicle -and $sp["VEHICLE"].IsOpen) {
    # Reset the vehicle to restart the round campaign (BLE reconnects and runs ROUNDS again).
    $sp["VEHICLE"].RtsEnable = $true; Start-Sleep -Milliseconds 150; $sp["VEHICLE"].RtsEnable = $false
}

"# monitor started $(Get-Date -Format o) | KEY=$key RELAY_A=$relayA RELAY_B=$relayB VEHICLE=$vehicle" | Out-File -Encoding utf8 $out
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
"# monitor stopped $(Get-Date -Format o)" | Out-File -Append -Encoding utf8 $out
