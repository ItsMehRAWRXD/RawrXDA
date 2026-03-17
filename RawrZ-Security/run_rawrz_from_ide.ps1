# RawrZ Security - Launch from MASM x64 IDE
# Ensures all RawrZ Security content is available (Payload Builder + Camellia)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$PayloadBuilder = Join-Path $PSScriptRoot 'RawrZ-Payload-Builder'

if (-not (Test-Path $PayloadBuilder)) {
    Write-Error "RawrZ-Payload-Builder not found at: $PayloadBuilder"
    exit 1
}

$packageJson = Join-Path $PayloadBuilder 'package.json'
if (-not (Test-Path $packageJson)) {
    Write-Error "package.json not found. Run from repo root or ensure RawrZ-Security was copied."
    exit 1
}

Write-Host "RawrZ Security - Starting Payload Builder (MASM IDE integration)" -ForegroundColor Cyan
Write-Host "Root: $Root" -ForegroundColor Gray
Write-Host "Camellia MASM x64: $Root\Ship\RawrZ_Camellia_MASM_x64.asm" -ForegroundColor Gray
Write-Host ""

$nodeModules = Join-Path $PayloadBuilder 'node_modules'
if (-not (Test-Path $nodeModules)) {
    Write-Host "Installing dependencies (npm install)..." -ForegroundColor Yellow
    Push-Location $PayloadBuilder
    npm install
    if ($LASTEXITCODE -ne 0) { Pop-Location; exit 1 }
    Pop-Location
}

Push-Location $PayloadBuilder
try {
    npm run dev
} finally {
    Pop-Location
}
