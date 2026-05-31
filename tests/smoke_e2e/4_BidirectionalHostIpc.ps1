# Scenario 4: bidirectional live IPC — delegates to canonical native-client harness.
# Uses RawrXD-ExtensionHost / RawrXDIpcPing (production MessageSegmenter), not PS pipe mock.
param(
    [string]$IdePath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [string]$LogPath = "",
    [int]$TimeoutSeconds = 5
)

$BinPath = Split-Path -Parent $IdePath
$HostPath = Join-Path $BinPath "RawrXD-ExtensionHost.exe"

$env:RAWRXD_EXTENSION_HOST_MODE = "CLIENT_SMOKE"

& (Join-Path $PSScriptRoot "4_LiveExtensionIpc.ps1") `
    -BinaryPath $IdePath `
    -HostPath $HostPath `
    -PingPath (Join-Path $BinPath "RawrXDIpcPing.exe") `
    -IngestWaitMs 800

exit $LASTEXITCODE
