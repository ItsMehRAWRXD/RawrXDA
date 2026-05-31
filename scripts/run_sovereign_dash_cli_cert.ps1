param(
    [string]$ExePath = "d:\rawrxd\build\dist\rawrxd.exe",
    [string]$DashboardRoot = "d:\rawrxd\sandbox\sovereign-dash",
    [string]$OutputDir = "d:\rawrxd\sandbox\sovereign-dash\cert-output",
    [int]$ApiPort = 4180,
    [int]$WebPort = 5180
)

$ErrorActionPreference = "Stop"

function Test-PortListening {
    param([int]$Port)
    return $null -ne (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)
}

function Wait-Port {
    param(
        [int]$Port,
        [int]$TimeoutSeconds = 20
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-PortListening -Port $Port) {
            return $true
        }
        Start-Sleep -Milliseconds 500
    }

    return $false
}

if (-not (Test-Path $ExePath)) {
    throw "RawrXD CLI not found: $ExePath"
}

if (-not (Test-Path $DashboardRoot)) {
    throw "Dashboard root not found: $DashboardRoot"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$summaryPath = Join-Path $OutputDir "sovereign_dash_cert_summary_$timestamp.json"
$stdoutPath = Join-Path $OutputDir "sovereign_dash_cli_$timestamp.log"
$verifyScriptPath = Join-Path $OutputDir "verify_sovereign_dash_$timestamp.mjs"
$commandFilePath = Join-Path $OutputDir "sovereign_dash_cli_$timestamp.txt"

$verifyScript = @'
const endpoints = [
    "http://127.0.0.1:__API_PORT__/api/health",
    "http://127.0.0.1:__API_PORT__/api/stats",
    "http://127.0.0.1:__WEB_PORT__/"
];

for (const endpoint of endpoints) {
    const response = await fetch(endpoint);
    const contentType = response.headers.get('content-type') || '';
    const body = contentType.includes('application/json')
        ? JSON.stringify(await response.json())
        : (await response.text()).slice(0, 240);
    console.log(`ENDPOINT ${endpoint}`);
    console.log(body);
}
'@
$verifyScript = $verifyScript.Replace('__API_PORT__', [string]$ApiPort).Replace('__WEB_PORT__', [string]$WebPort)
Set-Content -Path $verifyScriptPath -Value $verifyScript -Encoding UTF8

$startedProcess = $null
if (-not (Test-PortListening -Port $ApiPort) -or -not (Test-PortListening -Port $WebPort)) {
    $startedProcess = Start-Process powershell -ArgumentList @(
        '-NoProfile',
        '-Command',
        "Set-Location '$DashboardRoot'; npm run dev"
    ) -WorkingDirectory $DashboardRoot -PassThru

    if (-not (Wait-Port -Port $ApiPort) -or -not (Wait-Port -Port $WebPort)) {
        if ($startedProcess) {
            Stop-Process -Id $startedProcess.Id -Force -ErrorAction SilentlyContinue
        }
        throw "Dashboard services did not start on ports $ApiPort/$WebPort"
    }
}

$commands = @(
    "help",
    "status",
    "server",
    "quit"
)

Set-Content -Path $commandFilePath -Value ($commands -join [Environment]::NewLine) -Encoding UTF8

$cmdLine = ('type "{0}" | "{1}" --headless --repl > "{2}" 2>&1' -f $commandFilePath, $ExePath, $stdoutPath)
cmd.exe /d /c $cmdLine | Out-Null

$buildOutput = & cmd.exe /d /c ('cd /d "{0}" && npm run build' -f $DashboardRoot) 2>&1 | Out-String
Add-Content -Path $stdoutPath -Value "`r`n=== BUILD OUTPUT ===`r`n$buildOutput"

$verifyOutput = & cmd.exe /d /c ('node "{0}"' -f $verifyScriptPath) 2>&1 | Out-String
Add-Content -Path $stdoutPath -Value "`r`n=== ENDPOINT VERIFICATION ===`r`n$verifyOutput"

$summary = [ordered]@{
    exePath = $ExePath
    mode = "headless-repl"
    dashboardRoot = $DashboardRoot
    stdoutLog = $stdoutPath
    verifierScript = $verifyScriptPath
    commandsFile = $commandFilePath
    apiPort = $ApiPort
    webPort = $WebPort
    replaySupported = $false
    limitation = "Current build exposes headless REPL commands but not replay/governor CLI commands in this runtime surface."
    buildVerified = $buildOutput -match 'vite build' -and $buildOutput -match 'built in'
    endpointsVerified = $verifyOutput -match '/api/health' -and $verifyOutput -match '/api/stats' -and $verifyOutput -match "http://127.0.0.1:$WebPort/"
    timestamp = (Get-Date).ToString('o')
}

$summary | ConvertTo-Json -Depth 5 | Set-Content -Path $summaryPath -Encoding UTF8

Write-Host "CLI certification log: $stdoutPath"
Write-Host "Certification summary: $summaryPath"
Write-Host "Verifier script: $verifyScriptPath"

if ($startedProcess) {
    Stop-Process -Id $startedProcess.Id -Force -ErrorAction SilentlyContinue
}