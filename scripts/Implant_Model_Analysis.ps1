<#
.SYNOPSIS
    One-shot implant: wires Vulkan + analysis and ensures ModelAnalysis is buildable (no surgery).
.DESCRIPTION
    Run from repo root. Ensures all analysis sources exist, runs Vulkan AutoWire, generates ical,
    then optionally runs a quick build. Use for "production ready in 15 mins".
.EXAMPLE
    .\scripts\Implant_Model_Analysis.ps1
    .\scripts\Implant_Model_Analysis.ps1 -Build
#>
param(
    [string]$ProjectRoot = (Get-Location).Path,
    [switch]$Build,
    [switch]$SkipVulkanWire
)

$ErrorActionPreference = "Stop"

if (-not [System.IO.Path]::IsPathRooted($ProjectRoot)) {
    $ProjectRoot = Join-Path (Get-Location) $ProjectRoot
}
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
$ScriptDir = $PSScriptRoot

Write-Host "[*] RawrXD Model Analysis Implant" -ForegroundColor Cyan
Write-Host "    Root: $ProjectRoot" -ForegroundColor Gray

# 1. Ensure required source files exist (they are created by this deliverable)
$required = @(
    "src\core\model_anatomy.hpp",
    "src\core\model_anatomy.cpp",
    "src\core\neurological_diff.hpp",
    "src\core\neurological_diff.cpp",
    "src\tools\model_analysis_cli.cpp"
)
$missing = @()
foreach ($r in $required) {
    $p = Join-Path $ProjectRoot $r
    if (-not (Test-Path $p)) { $missing += $r }
}
if ($missing.Count -gt 0) {
    Write-Host "[-] Missing files (add them first):" -ForegroundColor Red
    $missing | ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
    exit 1
}
Write-Host "[+] All analysis sources present" -ForegroundColor Green

# 2. Vulkan + ical auto-wire
if (-not $SkipVulkanWire) {
    $autoWire = Join-Path $ScriptDir "RawrXD_Vulkan_Analysis_AutoWire.ps1"
    if (Test-Path $autoWire) {
        & $autoWire -ProjectRoot $ProjectRoot -GenerateIcal
    } else {
        Write-Host "[!] RawrXD_Vulkan_Analysis_AutoWire.ps1 not found; skipping Vulkan wire" -ForegroundColor Yellow
    }
} else {
    Write-Host "[*] Skipping Vulkan wire (SkipVulkanWire)" -ForegroundColor Gray
}

# 3. Ensure analysis.ical.json exists for IDE/tooling
$icalPath = Join-Path $ProjectRoot "analysis.ical.json"
if (-not (Test-Path $icalPath)) {
    @{
        schema    = "RawrXD-Analysis-1.0"
        generated = (Get-Date -Format "o")
        vulkan    = @{ sdk_path = $env:VULKAN_SDK }
        targets   = @(
            @{ name = "RawrXD-ModelAnalysis"; executable = "RawrXD-ModelAnalysis.exe"; stream_to_terminal = $true }
        )
    } | ConvertTo-Json -Depth 4 | Set-Content $icalPath -Force -Encoding UTF8
    Write-Host "[+] Created $icalPath" -ForegroundColor Green
}

# 4. Optional build
if ($Build) {
    Write-Host "[*] Running build (RawrXD-ModelAnalysis)..." -ForegroundColor Cyan
    $buildScript = Join-Path $ProjectRoot "BUILD_ORCHESTRATOR.ps1"
    if (Test-Path $buildScript) {
        Push-Location $ProjectRoot
        try {
            & $buildScript -Mode quick 2>&1 | Out-Host
            if ($LASTEXITCODE -ne 0) {
                Write-Host "[-] Build reported exit code $LASTEXITCODE" -ForegroundColor Yellow
            } else {
                $exe = Join-Path $ProjectRoot "build\bin\RawrXD-ModelAnalysis.exe"
                if (Test-Path $exe) {
                    Write-Host "[+] Built: $exe" -ForegroundColor Green
                }
            }
        } finally {
            Pop-Location
        }
    } else {
        Write-Host "[!] BUILD_ORCHESTRATOR.ps1 not found; run CMake build manually" -ForegroundColor Yellow
        Write-Host "    cmake --build build --target RawrXD-ModelAnalysis" -ForegroundColor Gray
    }
}

Write-Host "`n[+] Implant complete. Run:" -ForegroundColor Green
Write-Host "    .\build\bin\RawrXD-ModelAnalysis.exe --autopsy <model.gguf> [--json]" -ForegroundColor White
Write-Host "    .\build\bin\RawrXD-ModelAnalysis.exe --diff <A.gguf> <B.gguf> [--json]" -ForegroundColor White
