#requires -Version 5.1
<#
.SYNOPSIS
    Launch RawrXD IDE from the built location (D:\rawrxd\build\bin).

.DESCRIPTION
    Finds RawrXD-Win32IDE.exe or rawrxd.exe under build\bin and runs it.
    Pass-through: any arguments are forwarded to the executable (e.g. --help, --version).

.EXAMPLE
    .\Launch-RawrXD.ps1
    .\Launch-RawrXD.ps1 --help
    .\Launch-RawrXD.ps1 --version
#>
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Passthrough
)

$ProjectRoot = if ($PSScriptRoot) { $PSScriptRoot } else { 'D:\rawrxd' }
$buildBin = Join-Path $ProjectRoot 'build\bin'
$buildBinRelease = Join-Path $ProjectRoot 'build\bin\Release'

$candidates = @(
    (Join-Path $buildBin 'RawrXD-Win32IDE.exe'),
    (Join-Path $buildBin 'rawrxd.exe'),
    (Join-Path $buildBinRelease 'RawrXD-Win32IDE.exe'),
    (Join-Path $buildBinRelease 'rawrxd.exe')
)

$exe = $null
foreach ($path in $candidates) {
    if (Test-Path -LiteralPath $path) {
        $exe = $path
        break
    }
}

if (-not $exe) {
    Write-Host "RawrXD IDE not found. Build it first:" -ForegroundColor Yellow
    Write-Host "  cd D:\rawrxd" -ForegroundColor Gray
    Write-Host "  .\BUILD_ORCHESTRATOR.ps1 -Mode quick" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Expected locations:" -ForegroundColor Gray
    foreach ($p in $candidates) { Write-Host "  $p" -ForegroundColor DarkGray }
    exit 1
}

Write-Host "Launching: $exe" -ForegroundColor Cyan
if ($Passthrough) {
    & $exe @Passthrough
} else {
    Start-Process -FilePath $exe
}
