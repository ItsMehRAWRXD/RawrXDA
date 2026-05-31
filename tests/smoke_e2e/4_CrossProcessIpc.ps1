# Scenario 4 entry — cross-process live IPC (IDE server + ExtensionHost client).
param(
    [string]$BinPath = "d:\rxdn_ninja\bin",
    [string]$BinaryPath = "",
    [string]$HostPath = "",
    [string]$RepoRoot = "D:\rawrxd"
)

if ([string]::IsNullOrWhiteSpace($BinaryPath)) {
    $BinaryPath = Join-Path $BinPath "RawrXD-Win32IDE.exe"
}
if ([string]::IsNullOrWhiteSpace($HostPath)) {
    $HostPath = Join-Path $BinPath "RawrXD-ExtensionHost.exe"
}

$env:RAWRXD_EXTENSION_HOST_MODE = "CLIENT_SMOKE"

& (Join-Path $PSScriptRoot "4_LiveExtensionIpc.ps1") `
    -BinaryPath $BinaryPath `
    -HostPath $HostPath `
    -PingPath (Join-Path $BinPath "RawrXDIpcPing.exe") `
    -RepoRoot $RepoRoot

exit $LASTEXITCODE
