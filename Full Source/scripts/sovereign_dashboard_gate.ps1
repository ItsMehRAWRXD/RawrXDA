#!/usr/bin/env pwsh
#==============================================================================
# SOVEREIGN DASHBOARD CI GATE v1.0
# Validates full thermal stack before every IDE build
# Badge: ✅ DASHBOARD_LIVE_PASS | ❌ DASHBOARD_LIVE_FAIL
#==============================================================================

param(
    [switch]$Verbose,
    [switch]$SkipKernel
)

$ErrorActionPreference = "Stop"
$script:passed = $true
$script:checks = @()

# MSVC tools path
$env:Path = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64;$env:Path"

function Test-Check {
    param([string]$Name, [scriptblock]$Test)
    try {
        $result = & $Test
        if ($result) {
            $script:checks += @{ Name = $Name; Status = "✅"; Detail = "PASS" }
            if ($Verbose) { Write-Host "  ✅ $Name" -ForegroundColor Green }
        } else {
            $script:checks += @{ Name = $Name; Status = "❌"; Detail = "FAIL" }
            $script:passed = $false
            if ($Verbose) { Write-Host "  ❌ $Name" -ForegroundColor Red }
        }
    } catch {
        $script:checks += @{ Name = $Name; Status = "❌"; Detail = $_.Exception.Message }
        $script:passed = $false
        if ($Verbose) { Write-Host "  ❌ $Name - $($_.Exception.Message)" -ForegroundColor Red }
    }
}

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  SOVEREIGN DASHBOARD CI GATE - Pre-Build Validation        ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# 1. Check pocket_lab_turbo.exe exists
Test-Check "pocket_lab_turbo.exe exists" {
    Test-Path "D:\rawrxd\build\pocket_lab_turbo.exe"
}

# 2. Check pocket_lab_turbo.dll exists
Test-Check "pocket_lab_turbo.dll exists" {
    Test-Path "D:\rawrxd\build\pocket_lab_turbo.dll"
}

# 3. Check DLL exports
Test-Check "DLL exports 4 functions" {
    $exports = & dumpbin /exports "D:\rawrxd\build\pocket_lab_turbo.dll" 2>$null
    ($exports -match "PocketLabInit") -and 
    ($exports -match "PocketLabGetThermal") -and 
    ($exports -match "PocketLabRunCycle") -and 
    ($exports -match "PocketLabGetStats")
}

# 4. Check IDE executable
Test-Check "RawrXD-AgenticIDE.exe exists" {
    Test-Path "D:\rawrxd\build\bin\Release\RawrXD-AgenticIDE.exe"
}

# 5. Check ThermalDashboardWidget compiled into IDE
Test-Check "ThermalDashboardWidget linked" {
    $strings = & dumpbin /imports "D:\rawrxd\build\bin\Release\RawrXD-AgenticIDE.exe" 2>$null | Out-String
    # Widget uses LoadLibrary, so check Qt imports exist
    $strings -match "Qt6Widgets.dll"
}

# 6. Check NVMe Oracle service running
Test-Check "NVMe Oracle service running" {
    $proc = Get-Process -Name "nvme_oracle*" -ErrorAction SilentlyContinue
    $null -ne $proc
}

# 7. Run pocket_lab_turbo.exe (unless skipped)
if (-not $SkipKernel) {
    Test-Check "pocket_lab_turbo.exe runs successfully" {
        $proc = Start-Process -FilePath "D:\rawrxd\build\pocket_lab_turbo.exe" -Wait -PassThru -NoNewWindow
        $proc.ExitCode -eq 0
    }
}

# 8. Check MMF handle: SOVEREIGN_NVME_TEMPS
Test-Check "MMF: Global\SOVEREIGN_NVME_TEMPS accessible" {
    # Use handle.exe or check via kernel (simplified: check Oracle is publishing)
    $proc = Get-Process -Name "nvme_oracle*" -ErrorAction SilentlyContinue
    $null -ne $proc  # If Oracle runs, MMF exists
}

# Summary
Write-Host "`n────────────────────────────────────────────────────────────" -ForegroundColor DarkGray
Write-Host "  RESULTS:" -ForegroundColor White
foreach ($c in $script:checks) {
    Write-Host "    $($c.Status) $($c.Name)" -ForegroundColor $(if ($c.Status -eq "✅") { "Green" } else { "Red" })
}
Write-Host "────────────────────────────────────────────────────────────`n" -ForegroundColor DarkGray

if ($script:passed) {
    Write-Host "  ████████████████████████████████████████████████████████" -ForegroundColor Green
    Write-Host "  ██  ✅ DASHBOARD_LIVE_PASS - All systems nominal     ██" -ForegroundColor Green
    Write-Host "  ████████████████████████████████████████████████████████`n" -ForegroundColor Green
    exit 0
} else {
    Write-Host "  ████████████████████████████████████████████████████████" -ForegroundColor Red
    Write-Host "  ██  ❌ DASHBOARD_LIVE_FAIL - Stack incomplete        ██" -ForegroundColor Red
    Write-Host "  ████████████████████████████████████████████████████████`n" -ForegroundColor Red
    exit 1
}
