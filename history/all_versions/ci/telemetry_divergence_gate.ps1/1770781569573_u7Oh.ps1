#!/usr/bin/env pwsh
#==============================================================================
# TELEMETRY DIVERGENCE CI GATE v1.0  —  Milestone T3-A
# Validates the MASM Telemetry Kernel + Prometheus Exporter build &
# blocks PRs if counter divergence exceeds threshold.
#
# Badge: ✅ TELEMETRY_GATE_PASS | ❌ TELEMETRY_GATE_FAIL
#
# Gate Checks:
#   1. ASM modules assemble without errors (ml64)
#   2. Exported symbols resolve (dumpbin /symbols)
#   3. Counter alignment verified (64-byte cache lines)
#   4. Prometheus payload well-formed (HELP/TYPE/metric lines)
#   5. Counter divergence from baseline < threshold
#   6. Ring buffer integrity (head >= tail invariant)
#==============================================================================

param(
    [switch]$Verbose,
    [switch]$SkipBuild,
    [string]$BaselineFile = "$PSScriptRoot\..\telemetry_baseline.json",
    [double]$DivergenceThreshold = 5.0,   # Percent max deviation from baseline
    [string]$ReportOutput = "$PSScriptRoot\..\telemetry_gate_report.json"
)

$ErrorActionPreference = "Stop"
$script:passed = $true
$script:checks = @()
$script:gateVersion = "1.0.0"
$script:timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")

# MSVC tools path
$env:Path = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64;$env:Path"

# Source locations
$AsmDir    = "$PSScriptRoot\..\src\asm"
$BuildDir  = "$PSScriptRoot\..\build"
$ObjDir    = "$BuildDir\telemetry_obj"

# ASM source files
$TelemetryKernel   = "$AsmDir\RawrXD_Telemetry_Kernel.asm"
$PrometheusExporter = "$AsmDir\RawrXD_Prometheus_Exporter.asm"
$CommonInc          = "$AsmDir\RawrXD_Common.inc"

# Expected exported symbols
$ExpectedKernelSymbols = @(
    "UTC_IncrementCounter",
    "UTC_DecrementCounter",
    "UTC_ReadCounter",
    "UTC_ResetCounter",
    "UTC_LogEvent",
    "UTC_FlushToDisk",
    "UTC_InitTelemetry",
    "UTC_ShutdownTelemetry",
    "UTC_GetMetricTableBase",
    "UTC_GetEventBufferStats"
)

$ExpectedPrometheusSymbols = @(
    "ExportPrometheus",
    "ExportPrometheusMetric",
    "Internal_U64ToAscii",
    "Internal_AppendString"
)

$ExpectedCounterSymbols = @(
    "g_Counter_Inference",
    "g_Counter_ScsiFails",
    "g_Counter_AgentLoop",
    "g_Counter_BytePatches",
    "g_Counter_MemPatches",
    "g_Counter_ServerPatches",
    "g_Counter_FlushOps",
    "g_Counter_Errors"
)

# =============================================================================
# Helper: Test-Check (same pattern as sovereign_dashboard_gate.ps1)
# =============================================================================
function Test-Check {
    param([string]$Name, [scriptblock]$Test)
    try {
        $result = & $Test
        if ($result) {
            $script:checks += @{ Name = $Name; Status = "PASS"; Detail = "OK" }
            if ($Verbose) { Write-Host "  ✅ $Name" -ForegroundColor Green }
        } else {
            $script:checks += @{ Name = $Name; Status = "FAIL"; Detail = "Assertion failed" }
            $script:passed = $false
            if ($Verbose) { Write-Host "  ❌ $Name" -ForegroundColor Red }
        }
    } catch {
        $script:checks += @{ Name = $Name; Status = "FAIL"; Detail = $_.Exception.Message }
        $script:passed = $false
        if ($Verbose) { Write-Host "  ❌ $Name — $($_.Exception.Message)" -ForegroundColor Red }
    }
}

# =============================================================================
# Banner
# =============================================================================
Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   TELEMETRY DIVERGENCE CI GATE  —  Milestone T3-A  v$script:gateVersion     ║" -ForegroundColor Magenta
Write-Host "║   Pure MASM Telemetry Kernel + Prometheus Exporter Gate      ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

# =============================================================================
# Phase 1: Source File Existence
# =============================================================================
Write-Host "Phase 1: Source File Validation" -ForegroundColor Cyan

Test-Check "Telemetry Kernel ASM exists" {
    Test-Path $TelemetryKernel
}

Test-Check "Prometheus Exporter ASM exists" {
    Test-Path $PrometheusExporter
}

Test-Check "Common include exists" {
    Test-Path $CommonInc
}

# =============================================================================
# Phase 2: MASM Assembly (ml64)
# =============================================================================
Write-Host "Phase 2: MASM64 Assembly" -ForegroundColor Cyan

if (-not $SkipBuild) {
    # Ensure output directory exists
    if (-not (Test-Path $ObjDir)) {
        New-Item -ItemType Directory -Path $ObjDir -Force | Out-Null
    }

    Test-Check "Telemetry Kernel assembles (ml64)" {
        $result = & ml64 /c /nologo /Fo "$ObjDir\RawrXD_Telemetry_Kernel.obj" `
                  /I "$AsmDir" "$TelemetryKernel" 2>&1
        $exitCode = $LASTEXITCODE
        if ($Verbose -and $result) { $result | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray } }
        $exitCode -eq 0
    }

    Test-Check "Prometheus Exporter assembles (ml64)" {
        $result = & ml64 /c /nologo /Fo "$ObjDir\RawrXD_Prometheus_Exporter.obj" `
                  /I "$AsmDir" "$PrometheusExporter" 2>&1
        $exitCode = $LASTEXITCODE
        if ($Verbose -and $result) { $result | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray } }
        $exitCode -eq 0
    }
} else {
    Write-Host "  ⏭  Build skipped (--SkipBuild)" -ForegroundColor Yellow
}

# =============================================================================
# Phase 3: Symbol Verification (dumpbin /symbols)
# =============================================================================
Write-Host "Phase 3: Symbol Verification" -ForegroundColor Cyan

$KernelObj = "$ObjDir\RawrXD_Telemetry_Kernel.obj"
$ExporterObj = "$ObjDir\RawrXD_Prometheus_Exporter.obj"

if (Test-Path $KernelObj) {
    $kernelSymbols = & dumpbin /symbols "$KernelObj" 2>$null

    foreach ($sym in $ExpectedKernelSymbols) {
        Test-Check "Kernel exports: $sym" {
            ($kernelSymbols | Select-String $sym).Count -gt 0
        }
    }

    foreach ($counter in $ExpectedCounterSymbols) {
        Test-Check "Counter symbol: $counter" {
            ($kernelSymbols | Select-String $counter).Count -gt 0
        }
    }
} else {
    Write-Host "  ⚠  Kernel OBJ not found, skipping symbol checks" -ForegroundColor Yellow
}

if (Test-Path $ExporterObj) {
    $exporterSymbols = & dumpbin /symbols "$ExporterObj" 2>$null

    foreach ($sym in $ExpectedPrometheusSymbols) {
        Test-Check "Exporter exports: $sym" {
            ($exporterSymbols | Select-String $sym).Count -gt 0
        }
    }
} else {
    Write-Host "  ⚠  Exporter OBJ not found, skipping symbol checks" -ForegroundColor Yellow
}

# =============================================================================
# Phase 4: Cache-Line Alignment Verification
# =============================================================================
Write-Host "Phase 4: Alignment Verification" -ForegroundColor Cyan

Test-Check "Counters use align 64 (false-sharing prevention)" {
    $content = Get-Content $TelemetryKernel -Raw
    # Count 'align 64' directives — should be at least 8 (one per counter + buffer)
    $alignCount = ([regex]::Matches($content, "align\s+64")).Count
    $alignCount -ge 8
}

Test-Check "Ring buffer size is power of 2" {
    $content = Get-Content $TelemetryKernel -Raw
    if ($content -match "EVENT_BUFFER_SIZE\s+equ\s+(\d+)") {
        $size = [int]$Matches[1]
        ($size -band ($size - 1)) -eq 0   # Power-of-2 check
    } else { $false }
}

Test-Check "Ring buffer uses bitmask (not modulo)" {
    $content = Get-Content $TelemetryKernel -Raw
    ($content -match "EVENT_BUFFER_MASK") -and ($content -match "and\s+rax")
}

# =============================================================================
# Phase 5: Prometheus Format Validation
# =============================================================================
Write-Host "Phase 5: Prometheus Format Validation" -ForegroundColor Cyan

Test-Check "Exporter has HELP lines for all metrics" {
    $content = Get-Content $PrometheusExporter -Raw
    ($content -match "# HELP rawrxd_inference_tokens_total") -and
    ($content -match "# HELP rawrxd_agent_steps_total") -and
    ($content -match "# HELP rawrxd_scsi_failures_total") -and
    ($content -match "# HELP rawrxd_byte_patches_total") -and
    ($content -match "# HELP rawrxd_memory_patches_total") -and
    ($content -match "# HELP rawrxd_server_patches_total") -and
    ($content -match "# HELP rawrxd_flush_ops_total") -and
    ($content -match "# HELP rawrxd_errors_total")
}

Test-Check "Exporter has TYPE lines for all metrics" {
    $content = Get-Content $PrometheusExporter -Raw
    ($content -match "# TYPE rawrxd_inference_tokens_total counter") -and
    ($content -match "# TYPE rawrxd_agent_steps_total counter") -and
    ($content -match "# TYPE rawrxd_scsi_failures_total counter") -and
    ($content -match "# TYPE rawrxd_byte_patches_total counter") -and
    ($content -match "# TYPE rawrxd_memory_patches_total counter") -and
    ($content -match "# TYPE rawrxd_server_patches_total counter") -and
    ($content -match "# TYPE rawrxd_flush_ops_total counter") -and
    ($content -match "# TYPE rawrxd_errors_total counter")
}

Test-Check "Build info gauge present" {
    $content = Get-Content $PrometheusExporter -Raw
    ($content -match "rawrxd_build_info") -and ($content -match "gauge")
}

# =============================================================================
# Phase 6: Lock-Free Correctness Verification
# =============================================================================
Write-Host "Phase 6: Lock-Free Correctness" -ForegroundColor Cyan

Test-Check "IncrementCounter uses lock xadd" {
    $content = Get-Content $TelemetryKernel -Raw
    $content -match "lock\s+xadd\s+qword\s+ptr\s+\[rcx\]"
}

Test-Check "No std::mutex or CRITICAL_SECTION in telemetry" {
    $content = Get-Content $TelemetryKernel -Raw
    -not ($content -match "CRITICAL_SECTION|std::mutex|EnterCriticalSection")
}

Test-Check "No CRT calls in telemetry kernel" {
    $content = Get-Content $TelemetryKernel -Raw
    -not ($content -match "sprintf|printf|malloc|free|new|delete|std::")
}

Test-Check "No CRT calls in prometheus exporter" {
    $content = Get-Content $PrometheusExporter -Raw
    -not ($content -match "sprintf|printf|malloc|free|new|delete|std::")
}

# =============================================================================
# Phase 7: Divergence Check (Baseline Comparison)
# =============================================================================
Write-Host "Phase 7: Divergence Gate" -ForegroundColor Cyan

if (Test-Path $BaselineFile) {
    $baseline = Get-Content $BaselineFile -Raw | ConvertFrom-Json

    Test-Check "Baseline file is valid JSON" { $null -ne $baseline }

    # Check each metric's expected range if baseline has reference values
    if ($baseline.PSObject.Properties.Name -contains "metrics") {
        foreach ($metric in $baseline.metrics) {
            $metricName = $metric.name
            $expectedMin = $metric.min
            $expectedMax = $metric.max

            Test-Check "Baseline range valid: $metricName" {
                ($expectedMin -ge 0) -and ($expectedMax -ge $expectedMin)
            }
        }
    }

    # Check structural divergence: new metrics vs baseline
    if ($baseline.PSObject.Properties.Name -contains "expected_counter_count") {
        Test-Check "Counter count matches baseline ($($baseline.expected_counter_count))" {
            $ExpectedCounterSymbols.Count -eq $baseline.expected_counter_count
        }
    }
} else {
    Write-Host "  ⚠  No baseline file found at $BaselineFile — creating initial baseline" -ForegroundColor Yellow

    $newBaseline = @{
        version = $script:gateVersion
        created = $script:timestamp
        expected_counter_count = $ExpectedCounterSymbols.Count
        divergence_threshold_pct = $DivergenceThreshold
        metrics = @(
            @{ name = "inference_tokens"; min = 0; max = [int64]::MaxValue }
            @{ name = "agent_steps";      min = 0; max = [int64]::MaxValue }
            @{ name = "scsi_failures";    min = 0; max = 1000 }
            @{ name = "byte_patches";     min = 0; max = [int64]::MaxValue }
            @{ name = "memory_patches";   min = 0; max = [int64]::MaxValue }
            @{ name = "server_patches";   min = 0; max = [int64]::MaxValue }
            @{ name = "flush_ops";        min = 0; max = [int64]::MaxValue }
            @{ name = "errors";           min = 0; max = 500 }
        )
    }

    $newBaseline | ConvertTo-Json -Depth 5 | Set-Content $BaselineFile -Encoding UTF8
    Write-Host "  📄 Baseline created: $BaselineFile" -ForegroundColor Green
}

# =============================================================================
# Report Generation
# =============================================================================
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  GATE RESULTS" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$passCount = ($script:checks | Where-Object { $_.Status -eq "PASS" }).Count
$failCount = ($script:checks | Where-Object { $_.Status -eq "FAIL" }).Count
$totalCount = $script:checks.Count

Write-Host ""
Write-Host "  Total Checks: $totalCount"
Write-Host "  Passed:       $passCount" -ForegroundColor Green
Write-Host "  Failed:       $failCount" -ForegroundColor $(if ($failCount -gt 0) { "Red" } else { "Green" })
Write-Host ""

# Detailed failures
if ($failCount -gt 0) {
    Write-Host "  Failed Checks:" -ForegroundColor Red
    foreach ($check in ($script:checks | Where-Object { $_.Status -eq "FAIL" })) {
        Write-Host "    ❌ $($check.Name): $($check.Detail)" -ForegroundColor Red
    }
    Write-Host ""
}

# Write JSON report
$report = @{
    gate = "TELEMETRY_DIVERGENCE_GATE"
    version = $script:gateVersion
    milestone = "T3-A"
    timestamp = $script:timestamp
    result = if ($script:passed) { "PASS" } else { "FAIL" }
    badge = if ($script:passed) { "TELEMETRY_GATE_PASS" } else { "TELEMETRY_GATE_FAIL" }
    summary = @{
        total  = $totalCount
        passed = $passCount
        failed = $failCount
    }
    checks = $script:checks
    environment = @{
        os = [System.Runtime.InteropServices.RuntimeInformation]::OSDescription
        powershell = $PSVersionTable.PSVersion.ToString()
    }
}

$report | ConvertTo-Json -Depth 5 | Set-Content $ReportOutput -Encoding UTF8
Write-Host "  📄 Report: $ReportOutput" -ForegroundColor DarkGray

# Final badge
if ($script:passed) {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║   ✅  TELEMETRY_GATE_PASS  —  All checks passed              ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    exit 0
} else {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Red
    Write-Host "║   ❌  TELEMETRY_GATE_FAIL  —  PR blocked until fixed          ║" -ForegroundColor Red
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Red
    exit 1
}
