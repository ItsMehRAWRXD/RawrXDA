# Scenario 4 entry — delegates to canonical live IPC harness.
param(
    [string]$BinaryPath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [string]$RepoRoot = "D:\rawrxd",
    [int]$BootWaitMs = 800,
    [int]$IngestWaitMs = 600
)

$canonical = Join-Path $PSScriptRoot "4_LiveExtensionIpc.ps1"
if (-not (Test-Path $canonical)) {
    $canonical = Join-Path $PSScriptRoot "4_LiveHostIpcValidation.ps1"
}
& $canonical @PSBoundParameters
exit $LASTEXITCODE
