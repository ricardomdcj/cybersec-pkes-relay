# Latency-injection sweep at 0.99 m: one relayed campaign per DELAY value.
# Relay B holds every vehicle challenge for DELAY ms before it goes on the ESP-NOW tunnel (command
# "DELAY <ms>", relays/main/relay_b.c). The verifier authenticates a round only if its response
# arrives within ROUND_TIMEOUT_MS (2000 ms), so the success rate should fall where DELAY plus the
# honest relayed round trip reaches the timeout (analysed by tools/analyze_delay.py).
#
# Usage (from the repo root, with the four boards connected and the M-22 vehicle firmware and the
# DELAY relay firmware flashed):
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\campaign\run_sweep.ps1
#   ... -Delays 0,1650 -Dist 0.99
# Each run is retried up to three times (the same policy as the baseline and relay series).

param(
    [int[]]$Delays = @(0, 500, 1000, 1400, 1500, 1600, 1650, 1700, 1750, 1800, 1900, 2200),
    [string]$Dist = '0.99'
)

$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$failed = @()

foreach ($d in $Delays) {
    $label = 'relay-d{0:D4}' -f $d
    $ok = $false
    for ($try = 1; $try -le 3 -and -not $ok; $try++) {
        "=== sweep: DELAY $d ms, label $label, attempt $try"
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $here 'run_relay.ps1') -Label $label -Dist $Dist -DelayMs $d
        if ($LASTEXITCODE -eq 0) { $ok = $true }
    }
    if (-not $ok) { $failed += $d }
    "=== sweep: DELAY $d ms done, ok=$ok"
}

if ($failed.Count -gt 0) {
    "sweep: these delays failed after three attempts: $($failed -join ', ')"
    exit 1
}
"sweep: all $($Delays.Count) delays recorded"
exit 0
