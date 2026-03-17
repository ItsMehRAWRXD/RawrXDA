param(
    [string]$SourceDir = "D:\RawrXD-Compilers",
    [string]$TargetDir = "C:\RawrXD",
    [switch]$BuildFull,
    [switch]$SkipLink
)

$ErrorActionPreference = "Stop"

$scriptPath = Join-Path $SourceDir "Build-And-Wire.ps1"
if (!(Test-Path $scriptPath)) {
    throw "Build-And-Wire.ps1 not found at $scriptPath"
}

& $scriptPath -SourceDir $SourceDir -TargetDir $TargetDir -BuildFull:$BuildFull -SkipLink:$SkipLink
