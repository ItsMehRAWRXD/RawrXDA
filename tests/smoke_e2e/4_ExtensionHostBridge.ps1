# Scenario 4: cross-process extension host bridge (production wire path).
param(
    [string]$IdePath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [string]$PingPath = "d:\rxdn_ninja\bin\IpcPingTool.exe",
    [switch]$FullSmoke
)

$BinPath = Split-Path -Parent $IdePath
$HostPath = Join-Path $BinPath "RawrXD-ExtensionHost.exe"

if (-not (Test-Path $PingPath)) {
    $PingPath = Join-Path $BinPath "RawrXD-IpcPingTool.exe"
}
if (-not (Test-Path $PingPath)) {
    $PingPath = Join-Path $BinPath "RawrXDIpcPing.exe"
}

if ($FullSmoke) {
    $env:RAWRXD_SMOKE_IPC_FULL = "1"
}

& (Join-Path $PSScriptRoot "4_LiveExtensionIpc.ps1") `
    -BinaryPath $IdePath `
    -HostPath $HostPath `
    -PingPath $PingPath `
    -IngestWaitMs 1200 `
    -FullSmoke:$FullSmoke

exit $LASTEXITCODE
