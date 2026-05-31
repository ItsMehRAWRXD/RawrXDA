# Scenario 4 alias — cross-process IPC validation (production wire path).
param(
    [string]$IdePath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [string]$HostPath = "d:\rxdn_ninja\bin\RawrXD-ExtensionHost.exe",
    [string]$LogDir = ""
)

& (Join-Path $PSScriptRoot "4_ExtensionHostBridge.ps1") -IdePath $IdePath -FullSmoke
exit $LASTEXITCODE
