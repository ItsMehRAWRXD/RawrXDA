# Canonical alias — delegates to 4_LiveExtensionIpc.ps1 (production wire path).
param(
    [string]$BinaryPath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [string]$HostPath = "d:\rxdn_ninja\bin\RawrXD-ExtensionHost.exe",
    [string]$PingPath = "d:\rxdn_ninja\bin\RawrXDIpcPing.exe",
    [string]$RepoRoot = "D:\rawrxd"
)

& (Join-Path $PSScriptRoot "4_LiveExtensionIpc.ps1") @PSBoundParameters
exit $LASTEXITCODE
