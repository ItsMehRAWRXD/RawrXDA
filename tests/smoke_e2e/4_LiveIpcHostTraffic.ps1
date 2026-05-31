# Scenario 4 alias — live IDE server + native ExtensionHost client (production wire path).
param(
    [string]$IdePath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [string]$HostPath = "d:\rxdn_ninja\bin\RawrXD-ExtensionHost.exe",
    [string]$LogPath = ""
)

$BinPath = Split-Path -Parent $IdePath
& (Join-Path $PSScriptRoot "4_LiveExtensionIpc.ps1") `
    -BinaryPath $IdePath `
    -HostPath $HostPath `
    -PingPath (Join-Path $BinPath "RawrXDIpcPing.exe")

exit $LASTEXITCODE
