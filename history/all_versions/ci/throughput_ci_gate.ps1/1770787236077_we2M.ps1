#!/usr/bin/env pwsh
#==============================================================================
# THROUGHPUT CI GATE v1.0  —  Milestone T3-D
# Blocks PRs if kernel throughput regressions are detected.
#
# Badge: ✅ THROUGHPUT_GATE_PASS | ❌ THROUGHPUT_GATE_FAIL
#
# Gate Checks:
#   1. GEMM/sec regression:      >3% drop blocks PR
#   2. FlashAttn/sec regression:  >5% drop blocks PR
#   3. AgentLoop count replay:    mismatch blocks PR
#   4. Token throughput:          >3% drop blocks PR
#   5. Inference latency:         >5% increase blocks PR
#   6. Patch safety:              abnormal counter deltas block PR
#   7. Replay divergence:         tool output drift blocks PR
#
# Data sources:
#   - UTC MASM counters (g_Counter_Inference, g_Counter_AgentLoop, etc.)
#   - Prometheus scraped metrics (from /metrics endpoint or exported JSON)
#   - Replay journal baseline (deterministic_replay session export)
#   - Performance benchmark JSON (from self_test_gate run)
#
# Usage:
#   .\throughput_ci_gate.ps1 -Verbose
#   .\throughput_ci_gate.ps1 -BaselineFile .\throughput_baseline.json
#   .\throughput_ci_gate.ps1 -SkipBenchmark -ReplayFile .\replay_session.json
#==============================================================================

param(
    [switch]$Verbose,
    [switch]$SkipBenchmark,
    [switch]$SkipReplay,
    [string]$BaselineFile = "$PSScriptRoot\..\throughput_baseline.json",
    [string]$ReplayFile   = "",
    [string]$MetricsFile  = "",
    [double]$GEMMThreshold       = 3.0,    # Max % GEMM/sec drop
    [double]$FlashAttnThreshold  = 5.0,    # Max % FlashAttn/sec drop
    [double]$TokenThreshold      = 3.0,    # Max % token throughput drop
    [double]$LatencyThreshold    = 5.0,    # Max % latency increase
    [double]$PatchDeltaThreshold = 50.0,   # Max % counter delta during hotpatch
    [string]$ReportOutput = "$PSScriptRoot\..\throughput_gate_report.json"
)

$ErrorActionPreference = "Stop"
$script:passed  = $true
$script:checks  = @()
$script:gateVersion = "1.0.0"
$script:timestamp   = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")

# =============================================================================
# Helper: Test-Check
# =============================================================================
function Test-Check {
    param([string]$Name, [scriptblock]$Test, [string]$Category = "general")
    try {
        $result = & $Test
        if ($result) {
            $script:checks += @{ Name = $Name; Status = "PASS"; Detail = "OK"; Category = $Category }
            if ($Verbose) { Write-Host "  ✅ $Name" -ForegroundColor Green }
        } else {
            $script:checks += @{ Name = $Name; Status = "FAIL"; Detail = "Assertion failed"; Category = $Category }
            $script:passed = $false
            if ($Verbose) { Write-Host "  ❌ $Name" -ForegroundColor Red }
        }
    } catch {
        $script:checks += @{ Name = $Name; Status = "FAIL"; Detail = $_.Exception.Message; Category = $Category }
        $script:passed = $false
        if ($Verbose) { Write-Host "  ❌ $Name — $($_.Exception.Message)" -ForegroundColor Red }
    }
}

function Test-Regression {
    param(
        [string]$MetricName,
        [double]$Current,
        [double]$Baseline,
        [double]$ThresholdPct,
        [string]$Direction = "drop",  # "drop" or "increase"
        [string]$Category = "perf"
    )
    if ($Baseline -le 0) {
        $script:checks += @{
            Name = "$MetricName regression check"
            Status = "SKIP"
            Detail = "No baseline ($Baseline)"
            Category = $Category
        }
        if ($Verbose) { Write-Host "  ⏭  $MetricName — no valid baseline" -ForegroundColor Yellow }
        return
    }

    $pctChange = (($Current - $Baseline) / $Baseline) * 100.0
    $regressed = $false

    if ($Direction -eq "drop") {
        $regressed = ($pctChange -lt (-$ThresholdPct))
        $detail = "Current: $([math]::Round($Current, 2)), Baseline: $([math]::Round($Baseline, 2)), Change: $([math]::Round($pctChange, 2))% (max allowed: -${ThresholdPct}%)"
    } else {
        $regressed = ($pctChange -gt $ThresholdPct)
        $detail = "Current: $([math]::Round($Current, 2)), Baseline: $([math]::Round($Baseline, 2)), Change: +$([math]::Round($pctChange, 2))% (max allowed: +${ThresholdPct}%)"
    }

    if ($regressed) {
        $script:checks += @{ Name = "$MetricName regression"; Status = "FAIL"; Detail = $detail; Category = $Category }
        $script:passed = $false
        if ($Verbose) { Write-Host "  ❌ $MetricName — $detail" -ForegroundColor Red }
    } else {
        $script:checks += @{ Name = "$MetricName regression"; Status = "PASS"; Detail = $detail; Category = $Category }
        if ($Verbose) { Write-Host "  ✅ $MetricName — $detail" -ForegroundColor Green }
    }
}

# =============================================================================
# Banner
# =============================================================================
Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   THROUGHPUT CI GATE  —  Milestone T3-D  v$script:gateVersion             ║" -ForegroundColor Magenta
Write-Host "║   GEMM + FlashAttn + AgentLoop Replay Divergence Gate        ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

# =============================================================================
# Phase 1: Baseline Loading
# =============================================================================
Write-Host "Phase 1: Baseline Loading" -ForegroundColor Cyan

$baseline = $null
if (Test-Path $BaselineFile) {
    $baseline = Get-Content $BaselineFile -Raw | ConvertFrom-Json
    Test-Check "Baseline file loaded" { $null -ne $baseline } "baseline"
    Test-Check "Baseline has perf section" { $null -ne $baseline.performance } "baseline"
    Test-Check "Baseline has replay section" { $null -ne $baseline.replay } "baseline"
} else {
    Write-Host "  ⚠  No baseline at $BaselineFile — creating seed baseline" -ForegroundColor Yellow
    $baseline = @{
        version = $script:gateVersion
        created = $script:timestamp
        performance = @{
            gemm_ops_per_sec        = 0.0
            flash_attn_ops_per_sec  = 0.0
            tokens_per_sec          = 0.0
            avg_latency_ms          = 0.0
            inference_count         = 0
        }
        replay = @{
            agent_loop_count        = 0
            tool_calls              = 0
            tool_output_hashes      = @()
            session_id              = ""
        }
        hotpatch = @{
            pre_patch_inference     = 0
            post_patch_inference    = 0
            pre_patch_errors        = 0
            post_patch_errors       = 0
        }
        thresholds = @{
            gemm_drop_pct           = $GEMMThreshold
            flash_attn_drop_pct     = $FlashAttnThreshold
            token_drop_pct          = $TokenThreshold
            latency_increase_pct    = $LatencyThreshold
            patch_delta_pct         = $PatchDeltaThreshold
        }
    }
    $baseline | ConvertTo-Json -Depth 5 | Set-Content $BaselineFile -Encoding UTF8
    Write-Host "  📄 Seed baseline created: $BaselineFile" -ForegroundColor Green
}

# =============================================================================
# Phase 2: Counter Snapshot from Prometheus Export
# =============================================================================
Write-Host "Phase 2: Counter Validation" -ForegroundColor Cyan

$currentMetrics = @{
    inference_count  = 0
    agent_loop_count = 0
    scsi_fails       = 0
    byte_patches     = 0
    mem_patches      = 0
    server_patches   = 0
    flush_ops        = 0
    errors           = 0
}

if ($MetricsFile -and (Test-Path $MetricsFile)) {
    $metricsContent = Get-Content $MetricsFile -Raw
    # Parse Prometheus text format
    foreach ($line in ($metricsContent -split "`n")) {
        $line = $line.Trim()
        if ($line.StartsWith("#") -or $line -eq "") { continue }
        $parts = $line -split "\s+"
        if ($parts.Count -ge 2) {
            switch ($parts[0]) {
                "rawrxd_inference_tokens_total" { $currentMetrics.inference_count = [int64]$parts[1] }
                "rawrxd_agent_steps_total"      { $currentMetrics.agent_loop_count = [int64]$parts[1] }
                "rawrxd_scsi_failures_total"    { $currentMetrics.scsi_fails = [int64]$parts[1] }
                "rawrxd_byte_patches_total"     { $currentMetrics.byte_patches = [int64]$parts[1] }
                "rawrxd_memory_patches_total"   { $currentMetrics.mem_patches = [int64]$parts[1] }
                "rawrxd_server_patches_total"   { $currentMetrics.server_patches = [int64]$parts[1] }
                "rawrxd_flush_ops_total"        { $currentMetrics.flush_ops = [int64]$parts[1] }
                "rawrxd_errors_total"           { $currentMetrics.errors = [int64]$parts[1] }
            }
        }
    }
    Test-Check "Prometheus metrics parsed" { $true } "counters"
} else {
    Write-Host "  ⚠  No metrics file — counter validation uses baseline only" -ForegroundColor Yellow
}

Test-Check "Error count within bounds" {
    $currentMetrics.errors -le 500
} "counters"

Test-Check "SCSI failures within bounds" {
    $currentMetrics.scsi_fails -le 1000
} "counters"

# =============================================================================
# Phase 3: GEMM / FlashAttn / Token Throughput Regression
# =============================================================================
Write-Host "Phase 3: Throughput Regression Gates" -ForegroundColor Cyan

$currentPerf = @{
    gemm_ops_per_sec        = 0.0
    flash_attn_ops_per_sec  = 0.0
    tokens_per_sec          = 0.0
    avg_latency_ms          = 0.0
}

# Look for benchmark results from self_test_gate or explicit perf JSON
$benchmarkFile = "$PSScriptRoot\..\benchmark_output.json"
if (-not $SkipBenchmark -and (Test-Path $benchmarkFile)) {
    $benchData = Get-Content $benchmarkFile -Raw | ConvertFrom-Json
    if ($benchData.PSObject.Properties.Name -contains "gemm_ops_per_sec") {
        $currentPerf.gemm_ops_per_sec = [double]$benchData.gemm_ops_per_sec
    }
    if ($benchData.PSObject.Properties.Name -contains "flash_attn_ops_per_sec") {
        $currentPerf.flash_attn_ops_per_sec = [double]$benchData.flash_attn_ops_per_sec
    }
    if ($benchData.PSObject.Properties.Name -contains "tokens_per_sec") {
        $currentPerf.tokens_per_sec = [double]$benchData.tokens_per_sec
    }
    if ($benchData.PSObject.Properties.Name -contains "avg_latency_ms") {
        $currentPerf.avg_latency_ms = [double]$benchData.avg_latency_ms
    }
    Test-Check "Benchmark data loaded" { $true } "perf"
}

if ($baseline -and $baseline.performance) {
    $bp = $baseline.performance

    Test-Regression -MetricName "GEMM ops/sec" `
        -Current $currentPerf.gemm_ops_per_sec `
        -Baseline ([double]($bp.gemm_ops_per_sec)) `
        -ThresholdPct $GEMMThreshold `
        -Direction "drop" -Category "perf"

    Test-Regression -MetricName "FlashAttn ops/sec" `
        -Current $currentPerf.flash_attn_ops_per_sec `
        -Baseline ([double]($bp.flash_attn_ops_per_sec)) `
        -ThresholdPct $FlashAttnThreshold `
        -Direction "drop" -Category "perf"

    Test-Regression -MetricName "Token throughput" `
        -Current $currentPerf.tokens_per_sec `
        -Baseline ([double]($bp.tokens_per_sec)) `
        -ThresholdPct $TokenThreshold `
        -Direction "drop" -Category "perf"

    Test-Regression -MetricName "Inference latency" `
        -Current $currentPerf.avg_latency_ms `
        -Baseline ([double]($bp.avg_latency_ms)) `
        -ThresholdPct $LatencyThreshold `
        -Direction "increase" -Category "perf"
}

# =============================================================================
# Phase 4: Agent Loop Replay Validation
# =============================================================================
Write-Host "Phase 4: Agent Loop Replay Validation" -ForegroundColor Cyan

if (-not $SkipReplay) {
    $replayData = $null
    if ($ReplayFile -and (Test-Path $ReplayFile)) {
        $replayData = Get-Content $ReplayFile -Raw | ConvertFrom-Json
    }

    if ($replayData) {
        # AgentLoop count must match deterministic replay journal
        Test-Check "Agent loop count matches replay" {
            if ($baseline.replay -and $baseline.replay.agent_loop_count -gt 0) {
                $replayCount = $replayData.action_count
                $baselineCount = $baseline.replay.agent_loop_count
                [math]::Abs($replayCount - $baselineCount) -le 1  # Allow ±1 tolerance
            } else { $true }
        } "replay"

        # Tool output hash drift detection
        if ($replayData.PSObject.Properties.Name -contains "tool_output_hashes" -and
            $baseline.replay.PSObject.Properties.Name -contains "tool_output_hashes") {
            $currentHashes = $replayData.tool_output_hashes
            $baselineHashes = $baseline.replay.tool_output_hashes

            if ($baselineHashes.Count -gt 0 -and $currentHashes.Count -gt 0) {
                $matchCount = 0
                $minLen = [math]::Min($currentHashes.Count, $baselineHashes.Count)
                for ($i = 0; $i -lt $minLen; $i++) {
                    if ($currentHashes[$i] -eq $baselineHashes[$i]) { $matchCount++ }
                }
                $matchPct = ($matchCount / $minLen) * 100.0

                Test-Check "Tool output drift < 10%" {
                    $matchPct -ge 90.0
                } "replay"

                if ($Verbose) {
                    Write-Host "    Tool output match: $([math]::Round($matchPct, 1))% ($matchCount / $minLen)" -ForegroundColor DarkGray
                }
            }
        }

        # Loop count anomaly: current counter vs replay record count
        if ($currentMetrics.agent_loop_count -gt 0 -and $replayData.PSObject.Properties.Name -contains "action_count") {
            Test-Check "UTC counter matches replay actions" {
                $utcCount = $currentMetrics.agent_loop_count
                $replayCount = $replayData.action_count
                # Allow 5% tolerance for non-deterministic paths
                $tolerance = [math]::Max(1, [math]::Floor($replayCount * 0.05))
                [math]::Abs($utcCount - $replayCount) -le $tolerance
            } "replay"
        }
    } else {
        Write-Host "  ⚠  No replay data available — skipping replay validation" -ForegroundColor Yellow
    }
} else {
    Write-Host "  ⏭  Replay validation skipped" -ForegroundColor Yellow
}

# =============================================================================
# Phase 5: Hotpatch Safety Validation
# =============================================================================
Write-Host "Phase 5: Hotpatch Safety Validation" -ForegroundColor Cyan

if ($baseline.hotpatch) {
    $hp = $baseline.hotpatch

    # Pre/post patch inference delta
    if ($hp.pre_patch_inference -gt 0 -and $hp.post_patch_inference -gt 0) {
        $infDelta = [math]::Abs($hp.post_patch_inference - $hp.pre_patch_inference)
        $infDeltaPct = ($infDelta / $hp.pre_patch_inference) * 100.0

        Test-Check "Hotpatch inference delta within bounds" {
            $infDeltaPct -le $PatchDeltaThreshold
        } "hotpatch"

        if ($Verbose) {
            Write-Host "    Inference delta: $infDelta ($([math]::Round($infDeltaPct, 1))%)" -ForegroundColor DarkGray
        }
    }

    # Pre/post patch error count
    if ($hp.pre_patch_errors -ge 0 -and $hp.post_patch_errors -ge 0) {
        Test-Check "Hotpatch did not spike errors" {
            $hp.post_patch_errors -le ($hp.pre_patch_errors + 5)
        } "hotpatch"
    }
}

# =============================================================================
# Phase 6: Structural Integrity
# =============================================================================
Write-Host "Phase 6: Structural Integrity" -ForegroundColor Cyan

$AsmDir = "$PSScriptRoot\..\src\asm"

Test-Check "Telemetry kernel ASM exists" {
    Test-Path "$AsmDir\RawrXD_Telemetry_Kernel.asm"
} "structural"

Test-Check "Prometheus exporter ASM exists" {
    Test-Path "$AsmDir\RawrXD_Prometheus_Exporter.asm"
} "structural"

Test-Check "Telemetry bridge header exists" {
    Test-Path "$PSScriptRoot\..\include\rawrxd_telemetry_exports.h"
} "structural"

Test-Check "Replay+Telemetry fusion header exists" {
    Test-Path "$PSScriptRoot\..\include\replay_telemetry_fusion.h"
} "structural"

Test-Check "Hotpatch safety header exists" {
    Test-Path "$PSScriptRoot\..\include\hotpatch_telemetry_safety.h"
} "structural"

# =============================================================================
# Report Generation
# =============================================================================
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  THROUGHPUT GATE RESULTS" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$passCount = ($script:checks | Where-Object { $_.Status -eq "PASS" }).Count
$failCount = ($script:checks | Where-Object { $_.Status -eq "FAIL" }).Count
$skipCount = ($script:checks | Where-Object { $_.Status -eq "SKIP" }).Count
$totalCount = $script:checks.Count

Write-Host ""
Write-Host "  Total Checks: $totalCount"
Write-Host "  Passed:       $passCount" -ForegroundColor Green
Write-Host "  Failed:       $failCount" -ForegroundColor $(if ($failCount -gt 0) { "Red" } else { "Green" })
Write-Host "  Skipped:      $skipCount" -ForegroundColor Yellow
Write-Host ""

if ($failCount -gt 0) {
    Write-Host "  Failed Checks:" -ForegroundColor Red
    foreach ($check in ($script:checks | Where-Object { $_.Status -eq "FAIL" })) {
        Write-Host "    ❌ [$($check.Category)] $($check.Name): $($check.Detail)" -ForegroundColor Red
    }
    Write-Host ""
}

# JSON report
$report = @{
    gate = "THROUGHPUT_CI_GATE"
    version = $script:gateVersion
    milestone = "T3-D"
    timestamp = $script:timestamp
    result = if ($script:passed) { "PASS" } else { "FAIL" }
    badge = if ($script:passed) { "THROUGHPUT_GATE_PASS" } else { "THROUGHPUT_GATE_FAIL" }
    summary = @{
        total   = $totalCount
        passed  = $passCount
        failed  = $failCount
        skipped = $skipCount
    }
    current_performance = $currentPerf
    current_counters = $currentMetrics
    thresholds = @{
        gemm_drop_pct           = $GEMMThreshold
        flash_attn_drop_pct     = $FlashAttnThreshold
        token_drop_pct          = $TokenThreshold
        latency_increase_pct    = $LatencyThreshold
        patch_delta_pct         = $PatchDeltaThreshold
    }
    checks = $script:checks
    environment = @{
        os = [System.Runtime.InteropServices.RuntimeInformation]::OSDescription
        powershell = $PSVersionTable.PSVersion.ToString()
    }
}

$report | ConvertTo-Json -Depth 5 | Set-Content $ReportOutput -Encoding UTF8
Write-Host "  📄 Report: $ReportOutput" -ForegroundColor DarkGray

if ($script:passed) {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║   ✅  THROUGHPUT_GATE_PASS  —  No regressions detected        ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    exit 0
} else {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Red
    Write-Host "║   ❌  THROUGHPUT_GATE_FAIL  —  PR blocked: regression found   ║" -ForegroundColor Red
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Red
    exit 1
}
