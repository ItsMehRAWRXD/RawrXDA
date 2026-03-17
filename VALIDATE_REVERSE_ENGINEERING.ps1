#requires -Version 7.0
<#
.SYNOPSIS
    CI-style validation for RawrXD production lane.
.DESCRIPTION
    - Auto-detects repo root (fallback D:\rawrxd)
    - Builds RawrXD-Win32IDE self_test_gate target
    - Fails if stub sources are linked into RawrXD-Win32IDE
    - Runs RawrXD-Win32IDE --selftest and fails on non-zero exit
#>

$ErrorActionPreference = "Stop"

function Resolve-ProjectRoot {
    if ($PSCommandPath) {
        $scriptDir = Split-Path -Parent $PSCommandPath
        if (Test-Path (Join-Path $scriptDir "CMakeLists.txt")) { return $scriptDir }
    }
    if (Test-Path "D:\rawrxd\CMakeLists.txt") { return "D:\rawrxd" }
    if (Test-Path (Join-Path $PWD "CMakeLists.txt")) { return $PWD.Path }
    throw "Could not locate project root (expected CMakeLists.txt)."
}

function Resolve-BuildDir([string]$root) {
    $candidates = @(
        (Join-Path $root "build_validation_run"),
        (Join-Path $root "build_real_lane"),
        (Join-Path $root "build")
    )
    foreach ($c in $candidates) {
        if (Test-Path (Join-Path $c "CMakeCache.txt")) { return $c }
    }
    return $candidates[0]
}

function Require-ZeroStubObjects([string]$buildDir) {
    $ninjaPath = Join-Path $buildDir "build.ninja"
    if (!(Test-Path $ninjaPath)) {
        throw "build.ninja not found at $ninjaPath"
    }
    $ninja = Get-Content $ninjaPath -Raw
    $matches = [regex]::Matches($ninja, 'RawrXD-Win32IDE\.dir\\src\\[^ \r\n]*(?:stub|stubs)[^ \r\n]*\.cpp\.obj', 'IgnoreCase')
    if ($matches.Count -gt 0) {
        Write-Host "Stub-linked objects detected in RawrXD-Win32IDE:" -ForegroundColor Red
        $unique = $matches.Value | Sort-Object -Unique
        foreach ($m in $unique) { Write-Host "  $m" -ForegroundColor Red }
        throw "Validation failed: stub source units are linked in default build."
    }
    Write-Host "Stub-link guard: PASS (no stub objects linked)." -ForegroundColor Green
}

$ProjectRoot = Resolve-ProjectRoot
$BuildDir = Resolve-BuildDir -root $ProjectRoot

Write-Host "ProjectRoot: $ProjectRoot" -ForegroundColor Cyan
Write-Host "BuildDir:    $BuildDir" -ForegroundColor Cyan

Push-Location $ProjectRoot
try {
    if (!(Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
        & cmake -S $ProjectRoot -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { throw "Configure failed." }
    }

    & cmake --build $BuildDir --config Release --target self_test_gate
    if ($LASTEXITCODE -ne 0) { throw "Build self_test_gate failed." }

    Require-ZeroStubObjects -buildDir $BuildDir

    $exeCandidates = @(
        (Join-Path $BuildDir "bin\RawrXD-Win32IDE.exe"),
        (Join-Path $BuildDir "Release\RawrXD-Win32IDE.exe")
    )
    $exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $exe) { throw "RawrXD-Win32IDE.exe not found in build output." }

    Write-Host "Running selftest: $exe --selftest" -ForegroundColor Yellow
    & $exe --selftest
    if ($LASTEXITCODE -ne 0) { throw "Runtime selftest failed with exit code $LASTEXITCODE." }

    $usageCandidates = @(
        (Join-Path $ProjectRoot "logs\command_usage_runtime.json"),
        (Join-Path $BuildDir "bin\logs\command_usage_runtime.json"),
        (Join-Path $BuildDir "logs\command_usage_runtime.json")
    )
    $usageJson = $usageCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $usageJson) {
        throw "Runtime telemetry missing (checked root/build paths)."
    }

    $mapScript = Join-Path $ProjectRoot "tools\generate_command_map.ps1"
    if (!(Test-Path $mapScript)) {
        throw "Command map generator missing: $mapScript"
    }
    & $mapScript -Root $ProjectRoot -UsageJson $usageJson
    if ($LASTEXITCODE -ne 0) { throw "Command map generation failed." }

    $mapOut = Join-Path $ProjectRoot "docs\COMMAND_MAP.md"
    if (!(Test-Path $mapOut)) {
        throw "Command map output missing: $mapOut"
    }

    Write-Host "VALIDATION PASS" -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "VALIDATION FAIL: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
