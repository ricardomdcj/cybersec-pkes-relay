# DelayMs: relay B holds each vehicle challenge for DelayMs before the tunnel (latency sweep).
# It is always sent, so a campaign never inherits the delay of the previous one; 0 is the default.
param(
    [string]$Label = 'relay',
    [string]$Dist = '0.52',
    [int]$TxPow = 0,
    [int]$DelayMs = 0
)
$ErrorActionPreference = 'Continue'
$py      = 'C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe'
$root    = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$state   = [IO.Path]::GetTempPath()
$stamp   = Get-Date -Format 'yyyyMMdd-HHmmss'
$session = Join-Path $root ("data\runs\{0}_{1}-session" -f $stamp, $Label)
New-Item -ItemType Directory -Force -Path $session | Out-Null
$chainLog = Join-Path $session 'chain_COM5_COM6_COM7.log'
$holdLog  = Join-Path $session 'hold_COM8.log'
$cmdFile  = Join-Path $state ("cmd_{0}.txt" -f $stamp)
$stopLog  = Join-Path $state ("stop_chain_{0}.flag" -f $stamp)
$stopHold = Join-Path $state ("stop_vehicle_hold_{0}.flag" -f $stamp)
$culture  = [Globalization.CultureInfo]::InvariantCulture

function Stop-Helpers {
    New-Item -ItemType File -Path $stopHold -Force | Out-Null
    New-Item -ItemType File -Path $stopLog -Force | Out-Null
}

function Stop-Bridge {
    Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'deskrab_bridge' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}

function Count-Log([string]$pattern) {
    return (Select-String -LiteralPath $chainLog -Pattern $pattern | Measure-Object).Count
}

function Wait-Count([string]$pattern, [int]$above, [int]$tries) {
    for ($i = 0; $i -lt $tries; $i++) {
        Start-Sleep -Milliseconds 500
        if ((Count-Log $pattern) -gt $above) { return $true }
    }
    return $false
}

function Log-Times([string]$pattern) {
    $times = @()
    foreach ($m in (Select-String -LiteralPath $chainLog -Pattern $pattern)) {
        $times += [datetime]::ParseExact($m.Line.Substring(0, 12), 'HH:mm:ss.fff', $culture)
    }
    return $times
}

Stop-Bridge

$holder = Start-Process -FilePath $py -ArgumentList @((Join-Path $PSScriptRoot 'hold_and_log.py'), '--hold', 'COM8', '--out', $holdLog, '--stop', $stopHold) -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 5
if (-not (Select-String -LiteralPath $holdLog -Pattern 'held COM8' -Quiet)) {
    'ABORT: could not hold the vehicle in reset (COM8 busy?)'
    Stop-Helpers; exit 1
}

$logger = Start-Process -FilePath $py -ArgumentList @((Join-Path $PSScriptRoot 'log_ports.py'), '--ports', 'COM5:RELAY_A,COM6:KEY,COM7:RELAY_B', '--pulse', 'COM5,COM7', '--cmd', $cmdFile, '--out', $chainLog, '--stop', $stopLog) -PassThru -WindowStyle Hidden
if (-not (Wait-Count 'LOGGER\] COM5 logged as RELAY_A' 0 20) -or -not (Wait-Count 'LOGGER\] COM6 logged as KEY' 0 20) -or -not (Wait-Count 'LOGGER\] COM7 logged as RELAY_B' 0 20)) {
    'ABORT: the passive logger could not open all three ports'
    Stop-Helpers; exit 1
}

if (-not (Wait-Count '\[RELAY_B\].*advertising the key clone' 0 60)) {
    'ABORT: relay B is not advertising'
    Stop-Helpers; exit 1
}

$confirmed = $false
for ($attempt = 1; $attempt -le 3 -and -not $confirmed; $attempt++) {
    $before = Count-Log '\[RELAY_A\].*connected to key'
    if (-not (Wait-Count '\[RELAY_A\].*connected to key' $before 120)) {
        "attempt ${attempt}: relay A did not connect"
    } else {
        Start-Sleep -Milliseconds 1500
        $tA = (Log-Times '\[RELAY_A\].*connected to key')[-1]
        $keyHit = @(Log-Times '\[KEY\].*vehicle connected' | Where-Object { ($_ - $tA).TotalSeconds -ge -1.0 -and ($_ - $tA).TotalSeconds -le 2.0 })
        if ($keyHit.Count -gt 0) {
            $confirmed = $true
            "attempt ${attempt}: key confirmed relay A's connection"
            break
        }
        "attempt ${attempt}: key did not confirm relay A's connection; resetting relay A"
    }
    Start-Sleep -Seconds 3
    Set-Content -LiteralPath $cmdFile -Value 'COM5 PULSE' -Encoding ascii
}
if (-not $confirmed) {
    'ABORT: relay A never got a confirmed connection to the key'
    Stop-Helpers; exit 1
}

if (-not (Wait-Count '\[RELAY_A\].*subscribed to key notify' 0 40)) {
    'ABORT: relay A did not subscribe'
    Stop-Helpers; exit 1
}
$drops = Count-Log '\[RELAY_A\].*key disconnected'
Start-Sleep -Seconds 3
if ((Count-Log '\[RELAY_A\].*key disconnected') -gt $drops) {
    'ABORT: relay A dropped the key after subscribing'
    Stop-Helpers; exit 1
}
'relay A subscribed and stayed up; relay B advertising'

if ($TxPow -gt 0) {
    Set-Content -LiteralPath $cmdFile -Value @("COM5 TXPOW $TxPow", "COM7 TXPOW $TxPow") -Encoding ascii
    $okA = $false; $okB = $false
    for ($i = 0; $i -lt 40; $i++) {
        Start-Sleep -Milliseconds 500
        $okA = Select-String -LiteralPath $chainLog -Pattern "\[RELAY_A\].*TXPOW set=$TxPow " -Quiet
        $okB = Select-String -LiteralPath $chainLog -Pattern "\[RELAY_B\].*TXPOW set=$TxPow " -Quiet
        if ($okA -and $okB) { break }
    }
    if (-not ($okA -and $okB)) {
        'ABORT: TXPOW was not applied on both relays'
        Stop-Helpers; exit 1
    }
    "TXPOW $TxPow applied on both relays"
}

Set-Content -LiteralPath $cmdFile -Value "COM7 DELAY $DelayMs" -Encoding ascii
$okD = $false
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500
    $okD = Select-String -LiteralPath $chainLog -Pattern "\[RELAY_B\].*DELAY set=$DelayMs ms" -Quiet
    if ($okD) { break }
}
if (-not $okD) {
    'ABORT: DELAY was not applied on relay B'
    Stop-Helpers; exit 1
}
"DELAY $DelayMs ms applied on relay B"

New-Item -ItemType File -Path $stopHold -Force | Out-Null
$holder.WaitForExit(10000) | Out-Null
Start-Sleep -Seconds 4

Set-Location -LiteralPath $root
$captureCode = 1
for ($try = 1; $try -le 3 -and $captureCode -ne 0; $try++) {
    Stop-Bridge
    Start-Sleep -Milliseconds 500
    & $py -I tools/capture_run.py --port COM8 --label $Label --dist $Dist --start --out $session --max-s 300 2>&1 | ForEach-Object { "$_" }
    $captureCode = $LASTEXITCODE
    if ($captureCode -ne 0) {
        "capture attempt $try failed (port busy); retrying"
        Get-ChildItem -LiteralPath $session -Directory | Where-Object { -not (Get-ChildItem -LiteralPath $_.FullName -Force) } | Remove-Item -Force
        Start-Sleep -Seconds 1
    }
}

New-Item -ItemType File -Path $stopLog -Force | Out-Null
$logger.WaitForExit(15000) | Out-Null
"capture exit=$captureCode"
$csvb = Count-Log 'CSVB,'
"relay CSVB lines: $csvb"
if ($csvb -eq 0) {
    'ABORT: campaign did not go through the relays; move this session to data/runs/_invalid'
    exit 1
}
if ($captureCode -ne 0) {
    'ABORT: capture did not complete'
    exit 1
}
"session: $session"
exit 0
