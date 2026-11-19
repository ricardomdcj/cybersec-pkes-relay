param(
    [string]$Label = 'baseline',
    [string]$Dist = '0.52'
)
$ErrorActionPreference = 'Continue'
$py      = 'C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe'
$root    = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$state   = [IO.Path]::GetTempPath()
$helper  = Join-Path $PSScriptRoot 'hold_and_log.py'
$stamp   = Get-Date -Format 'yyyyMMdd-HHmmss'
$session = Join-Path $root ("data\runs\{0}_{1}-session" -f $stamp, $Label)
New-Item -ItemType Directory -Force -Path $session | Out-Null
$keyLog   = Join-Path $session 'key_COM6.log'
$stopFile = Join-Path $state ("stop_hold_{0}.flag" -f $stamp)

function Stop-Bridge {
    Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'deskrab_bridge' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}

Stop-Bridge
$helperProc = Start-Process -FilePath $py -ArgumentList @($helper, '--hold', 'COM5,COM7', '--log', 'COM6', '--out', $keyLog, '--stop', $stopFile) -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 3
$status = Get-Content -LiteralPath $keyLog -ErrorAction SilentlyContinue
if (($status | Select-String -Pattern 'ERROR|UNEXPECTED') -or $helperProc.HasExited) {
    'ABORT: relays were not held cleanly'
    $status
    New-Item -ItemType File -Path $stopFile -Force | Out-Null
    exit 1
}
'held OK'

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

New-Item -ItemType File -Path $stopFile -Force | Out-Null
$helperProc.WaitForExit(15000) | Out-Null
"capture exit=$captureCode"
"session: $session"
