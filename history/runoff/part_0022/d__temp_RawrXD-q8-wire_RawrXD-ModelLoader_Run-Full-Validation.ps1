#!/usr/bin/env pwsh
# Complete RawrXD Validation Suite
# Tests everything: build, components, integration, and runtime

param(
    [switch]$SkipBuild,
    [switch]$SkipTests,
    [switch]$SkipRuntime,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$Results = @{
    Build = $null
    ComponentTests = $null
    RuntimeTest = $null
}

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "======================================" -ForegroundColor Cyan
    Write-Host $Title -ForegroundColor Cyan
    Write-Host "======================================" -ForegroundColor Cyan
}

function Write-Result {
    param([string]$Test, [bool]$Pass, [string]$Details = "")
    if ($Pass) {
        Write-Host "✓ $Test" -ForegroundColor Green
        if ($Details) { Write-Host "  $Details" -ForegroundColor Gray }
    } else {
        Write-Host "✗ $Test" -ForegroundColor Red
        if ($Details) { Write-Host "  $Details" -ForegroundColor Yellow }
    }
}

Write-Section "RawrXD Complete Validation Suite"
Write-Host "Testing: Build → Components → Runtime"
Write-Host ""

# ========================================
# PHASE 1: BUILD VALIDATION
# ========================================

if (-not $SkipBuild) {
    Write-Section "Phase 1: Build Validation"
    
    Push-Location $ProjectRoot
    try {
        # Clean build
        Write-Host "Configuring CMake..." -ForegroundColor Yellow
        cmake -B build -G "Visual Studio 17 2022" -A x64 2>&1 | Out-Null
        
        # Build main target
        Write-Host "Building RawrXD-QtShell..." -ForegroundColor Yellow
        $BuildOutput = cmake --build build --config Release --target RawrXD-QtShell 2>&1
        $BuildSuccess = $LASTEXITCODE -eq 0
        
        if ($BuildSuccess) {
            $ExePath = "build\bin\Release\RawrXD-QtShell.exe"
            $ExeSize = (Get-Item $ExePath -ErrorAction SilentlyContinue).Length / 1MB
            Write-Result "Main Executable Build" $true "Size: $($ExeSize.ToString('F2')) MB"
            $Results.Build = "PASS"
        } else {
            Write-Result "Main Executable Build" $false "Build failed"
            $Results.Build = "FAIL"
            Write-Host ($BuildOutput | Select-String "error" | Select-Object -First 5) -ForegroundColor Red
            exit 1
        }
        
        # Build test target
        Write-Host "Building test suite..." -ForegroundColor Yellow
        cmake --build build --config Release --target test_gui_components 2>&1 | Out-Null
        $TestBuildSuccess = $LASTEXITCODE -eq 0
        
        Write-Result "Test Suite Build" $TestBuildSuccess
        
        # Deploy Qt
        Write-Host "Deploying Qt dependencies..." -ForegroundColor Yellow
        $QtDeploy = "C:\Qt\6.7.3\msvc2022_64\bin\windeployqt.exe"
        if (Test-Path $QtDeploy) {
            & $QtDeploy "build\bin\Release\RawrXD-QtShell.exe" --release --no-translations --force 2>&1 | Out-Null
            if ($TestBuildSuccess) {
                & $QtDeploy "build\bin\Release\test_gui_components.exe" --release --no-translations --force 2>&1 | Out-Null
            }
            Write-Result "Qt Deployment" $true
        }
    }
    finally {
        Pop-Location
    }
} else {
    Write-Host "Skipping build phase" -ForegroundColor Gray
    $Results.Build = "SKIP"
}

# ========================================
# PHASE 2: COMPONENT TESTS
# ========================================

if (-not $SkipTests) {
    Write-Section "Phase 2: Component Tests"
    
    $TestExe = Join-Path $ProjectRoot "build\bin\Release\test_gui_components.exe"
    
    if (Test-Path $TestExe) {
        Push-Location (Split-Path $TestExe -Parent)
        try {
            Write-Host "Running isolated component tests..." -ForegroundColor Yellow
            Write-Host ""
            
            if ($Verbose) {
                & $TestExe -v2
            } else {
                & $TestExe
            }
            
            $TestSuccess = $LASTEXITCODE -eq 0
            $Results.ComponentTests = if ($TestSuccess) { "PASS" } else { "FAIL" }
            
            Write-Host ""
            Write-Result "Component Tests" $TestSuccess
            
            if (Test-Path "gui_component_tests.log") {
                $LogContent = Get-Content "gui_component_tests.log"
                $Summary = $LogContent | Select-String "SUMMARY" -Context 0,3
                if ($Summary) {
                    Write-Host ""
                    Write-Host "Test Summary:" -ForegroundColor Cyan
                    $Summary | ForEach-Object { Write-Host $_.Line -ForegroundColor Gray }
                }
            }
        }
        finally {
            Pop-Location
        }
    } else {
        Write-Result "Component Tests" $false "Test executable not found"
        $Results.ComponentTests = "FAIL"
    }
} else {
    Write-Host "Skipping component tests" -ForegroundColor Gray
    $Results.ComponentTests = "SKIP"
}

# ========================================
# PHASE 3: RUNTIME TEST
# ========================================

if (-not $SkipRuntime) {
    Write-Section "Phase 3: Runtime Test"
    
    $ExePath = Join-Path $ProjectRoot "build\bin\Release\RawrXD-QtShell.exe"
    
    if (Test-Path $ExePath) {
        Push-Location (Split-Path $ExePath -Parent)
        try {
            Write-Host "Starting RawrXD in minimal mode..." -ForegroundColor Yellow
            
            $env:RAWRXD_MINIMAL_GUI = "1"
            $env:QTAPP_DISABLE_GGUF = "1"
            
            # Start process and wait briefly
            $Process = Start-Process -FilePath ".\RawrXD-QtShell.exe" -PassThru -WindowStyle Hidden
            Start-Sleep -Seconds 3
            
            # Check if still running
            $StillRunning = Get-Process -Id $Process.Id -ErrorAction SilentlyContinue
            
            if ($StillRunning) {
                Write-Result "Runtime Startup" $true "Process launched successfully (PID: $($Process.Id))"
                
                # Check responsiveness
                $Responding = $StillRunning.Responding
                Write-Result "UI Responsiveness" $Responding
                
                # Memory check
                $MemoryMB = $StillRunning.WorkingSet64 / 1MB
                Write-Host "  Memory usage: $($MemoryMB.ToString('F2')) MB" -ForegroundColor Gray
                
                # Clean shutdown
                Stop-Process -Id $Process.Id -Force
                Start-Sleep -Seconds 1
                
                $Results.RuntimeTest = "PASS"
            } else {
                Write-Result "Runtime Startup" $false "Process crashed or exited immediately"
                $Results.RuntimeTest = "FAIL"
            }
        }
        finally {
            Pop-Location
            Remove-Item Env:\RAWRXD_MINIMAL_GUI -ErrorAction SilentlyContinue
            Remove-Item Env:\QTAPP_DISABLE_GGUF -ErrorAction SilentlyContinue
        }
    } else {
        Write-Result "Runtime Test" $false "Executable not found"
        $Results.RuntimeTest = "FAIL"
    }
} else {
    Write-Host "Skipping runtime test" -ForegroundColor Gray
    $Results.RuntimeTest = "SKIP"
}

# ========================================
# FINAL REPORT
# ========================================

Write-Section "Validation Results"

Write-Host ""
Write-Host "Phase Results:" -ForegroundColor Cyan
Write-Host "  Build:          $($Results.Build)" -ForegroundColor $(if ($Results.Build -eq "PASS") { "Green" } elseif ($Results.Build -eq "FAIL") { "Red" } else { "Gray" })
Write-Host "  Component Tests: $($Results.ComponentTests)" -ForegroundColor $(if ($Results.ComponentTests -eq "PASS") { "Green" } elseif ($Results.ComponentTests -eq "FAIL") { "Red" } else { "Gray" })
Write-Host "  Runtime Test:    $($Results.RuntimeTest)" -ForegroundColor $(if ($Results.RuntimeTest -eq "PASS") { "Green" } elseif ($Results.RuntimeTest -eq "FAIL") { "Red" } else { "Gray" })

$PassCount = ($Results.Values | Where-Object { $_ -eq "PASS" }).Count
$FailCount = ($Results.Values | Where-Object { $_ -eq "FAIL" }).Count
$TotalCount = ($Results.Values | Where-Object { $_ -ne "SKIP" }).Count

Write-Host ""
Write-Host "Overall: $PassCount/$TotalCount phases passed" -ForegroundColor Cyan

if ($FailCount -eq 0 -and $PassCount -gt 0) {
    Write-Host ""
    Write-Host "✓ VALIDATION SUCCESSFUL" -ForegroundColor Green
    Write-Host "All tested phases passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "✗ VALIDATION FAILED" -ForegroundColor Red
    Write-Host "$FailCount phase(s) failed" -ForegroundColor Red
    exit 1
}
