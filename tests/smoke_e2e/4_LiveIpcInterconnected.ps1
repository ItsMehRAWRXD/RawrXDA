# Scenario 4: live multi-process IPC (IDE server + native ping / extension host client).
param(
    [string]$IdePath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [string]$PingToolPath = "d:\rxdn_ninja\bin\RawrXD-IpcPingTool.exe",
    [int]$TimeoutMilliseconds = 4000
)

$BinPath = Split-Path -Parent $IdePath
$HostPath = Join-Path $BinPath "RawrXD-ExtensionHost.exe"

# Prefer production ExtensionHost bridge; fall back to RawrXDIpcPing / IpcPingTool aliases.
if (-not (Test-Path $PingToolPath)) {
    $PingToolPath = Join-Path $BinPath "RawrXDIpcPing.exe"
}
if (-not (Test-Path $PingToolPath)) {
    $PingToolPath = Join-Path $BinPath "IpcPingTool.exe"
}

& (Join-Path $PSScriptRoot "4_LiveExtensionIpc.ps1") `
    -BinaryPath $IdePath `
    -HostPath $HostPath `
    -PingPath $PingToolPath `
    -IngestWaitMs 800

exit $LASTEXITCODE
