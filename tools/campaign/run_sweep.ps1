# Latency-injection sweep at 0.99 m: one relayed campaign per DELAY value.
# Relay B holds every vehicle challenge for DELAY ms before it goes on the ESP-NOW tunnel (command
# "DELAY <ms>", relays/main/relay_b.c). The verifier authenticates a round only if its response
# arrives within ROUND_TIMEOUT_MS (2000 ms), so the success rate should fall where DELAY plus the
# honest relayed round trip reaches the timeout (analysed by tools/analyze_delay.py).
#
# Usage (from the repo root, with the four boards connected, the M-22 vehicle firmware and the
# DELAY relay firmware flashed):
#   & .\tools\campaign\run_sweep.ps1 -Delays 1650,1700 -Dist 0.99
#   (run it from a PowerShell prompt: `powershell -File` passes a comma list as one string and fails)
#
# Each run is retried up to five times. Before each attempt the deskrab status-line bridge is
# stopped: it holds the COM ports and respawns while the session is active, which makes the passive
# logger fail to open them. A failed attempt leaves a session folder without data; those folders
# are moved to data/runs/_invalid/aborted so that they are not read as campaigns.

param(
    [int[]]$Delays = @(0, 500, 1000, 1400, 1500, 1600, 1650, 1700, 1750, 1800, 1900, 2200),
    [string]$Dist = '0.99',
    [int]$Attempts = 5
)

$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
$root = (Resolve-Path (Join-Path $here '..\..')).Path
$runs = Join-Path $root 'data\runs'
$aborted = Join-Path $runs '_invalid\aborted'
New-Item -ItemType Directory -Force -Path $aborted | Out-Null
$failed = @()

function Stop-Bridge {
    Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'deskrab_bridge' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}

foreach ($d in $Delays) {
    $label = 'relay-d{0:D4}' -f $d
    $ok = $false
    for ($try = 1; $try -le $Attempts -and -not $ok; $try++) {
        "=== sweep: DELAY $d ms, label $label, attempt $try"
        $started = Get-Date
        Stop-Bridge
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $here 'run_relay.ps1') -Label $label -Dist $Dist -DelayMs $d
        if ($LASTEXITCODE -eq 0) { $ok = $true; break }
        # run_relay has returned, so no campaign is running: any logger or holder still alive is a leftover
        # of this attempt and would keep the ports closed for the next one.
        Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'log_ports\.py|hold_and_log\.py|capture_run\.py' } |
            ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
        Get-ChildItem -LiteralPath $runs -Directory -Filter "*_$label-session" |
            Where-Object { $_.CreationTime -ge $started.AddSeconds(-5) } | ForEach-Object {
                Move-Item -LiteralPath $_.FullName -Destination $aborted
                "moved failed session aside: $($_.Name)"
            }
        Start-Sleep -Seconds 5
    }
    if (-not $ok) { $failed += $d }
    "=== sweep: DELAY $d ms done, ok=$ok"
}

if ($failed.Count -gt 0) {
    "sweep: these delays failed after $Attempts attempts: $($failed -join ', ')"
    exit 1
}
"sweep: all $($Delays.Count) delays recorded"
exit 0
