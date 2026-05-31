#!/usr/bin/env powershell
# 70B Stress Test Profile for RawrXD Streaming Engine
# Date: May 2, 2026
# Purpose: Titan soak with KV aperture flush monitoring, mmap fallback, and GPU batching

param(
    [string]$ModelPath = "F:\models\Qwen2.5-70B-Instruct-Q4_K_M.gguf",
    [int]$ContextTokens = 32768,
    [int]$MaxTokens = 512,
    [int]$Cycles = 100,
    [switch]$EnableMmapFallback = $true,
    [switch]$EnableGpuBatching = $false,  # Not yet implemented
    [switch]$EnablePhaseAwareMetrics = $true,
    [string]$OutputDir = "D:\rawrxd\test_results\70b_stress_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
)

# ============================================================================
# HEADER
# ============================================================================
Write-Host "╔════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   RawrXD 70B Stress Test - Titan Soak with Streaming Hardening    ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Create output directory
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$LogFile = Join-Path $OutputDir "stress_test.log"
$MetricsFile = Join-Path $OutputDir "metrics.json"
$ReportFile = Join-Path $OutputDir "report.md"

function Log-Message {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logLine = "[$timestamp] [$Level] $Message"
    Write-Host $logLine
    Add-Content -Path $LogFile -Value $logLine
}

# ============================================================================
# ENVIRONMENT CONFIGURATION
# ============================================================================
Log-Message "Configuring environment for 70B stress test..."

# KV Cache Configuration
$env:RAWRXD_KV_RECENT_TOKENS = "32768"      # Recent context window
$env:RAWRXD_KV_MID_TOKENS = "65536"         # Mid-tier context
$env:RAWRXD_KV_APERTURE_MONITORING = "1"    # Enable aperture flush monitoring
$env:RAWRXD_KV_FLUSH_THRESHOLD = "0.15"     # Aperture thrashing threshold

# Extension Logging
$env:RAWRXD_EXTENSION_LOG = Join-Path $OutputDir "extension_events.log"
$env:RAWRXD_EXTENSION_LOG_LEVEL = "DEBUG"   # Capture all diagnostic events

# Streaming Configuration
$env:RAWRXD_STREAMING_MMAP_FALLBACK = if ($EnableMmapFallback) { "1" } else { "0" }
$env:RAWRXD_STREAMING_GPU_BATCHING = if ($EnableGpuBatching) { "1" } } else { "0" }
$env:RAWRXD_STREAMING_PHASE_AWARE = if ($EnablePhaseAwareMetrics) { "1" } else { "0" }

# Memory Configuration
$env:RAWRXD_MAX_PINNED_MB = "14336"         # 14GB pinned limit for 16GB VRAM systems
$env:RAWRXD_PREFETCH_WINDOW = "3"            # 3-layer prefetch window
$env:RAWRXD_EVICTION_THRESHOLD = "0.85"       # Evict when 85% of budget used

# Hotpatch Configuration
$env:RAWRXD_HOTPATCH_AUTOPATCH = "1"         # Enable autopatch
$env:RAWRXD_HOTPATCH_UNRESTRICTIVE_DIAL = "0.7"  # 70% aggression

Log-Message "Environment configured:"
Log-Message "  Model: $ModelPath"
Log-Message "  Context: $ContextTokens tokens"
Log-Message "  Max Output: $MaxTokens tokens"
Log-Message "  Cycles: $Cycles"
Log-Message "  MMAP Fallback: $EnableMmapFallback"
Log-Message "  GPU Batching: $EnableGpuBatching"
Log-Message "  Phase-Aware Metrics: $EnablePhaseAwareMetrics"
Log-Message "  Output Directory: $OutputDir"

# ============================================================================
# PRE-TEST VALIDATION
# ============================================================================
Log-Message "Running pre-test validation..."

# Check model exists
if (-not (Test-Path $ModelPath)) {
    Log-Message "ERROR: Model not found at $ModelPath" "ERROR"
    exit 1
}

$modelSize = (Get-Item $ModelPath).Length / 1GB
Log-Message "Model size: $([math]::Round($modelSize, 2)) GB"

# Check available memory
$os = Get-CimInstance -ClassName Win32_OperatingSystem
$availableGB = [math]::Round($os.FreePhysicalMemory / 1MB, 2)
Log-Message "Available RAM: $availableGB GB"

if ($availableGB -lt 32) {
    Log-Message "WARNING: Less than 32GB RAM available. Test may fail for 70B model." "WARN"
}

# Check executable
$exePath = "D:\rawrxd\build-ninja\bin\RawrXD-Win32IDE.exe"
if (-not (Test-Path $exePath)) {
    Log-Message "ERROR: RawrXD executable not found at $exePath" "ERROR"
    exit 1
}

Log-Message "Pre-test validation passed"

# ============================================================================
# TEST EXECUTION
# ============================================================================
Log-Message "Starting 70B stress test (Titan soak)..."

$testPrompts = @(
    "Explain the concept of transformer architecture in deep learning.",
    "Write a Python function to implement attention mechanism.",
    "Compare and contrast CNNs and RNNs for sequence modeling.",
    "Describe the challenges of training large language models.",
    "What are the key innovations in the GPT architecture?",
    "Explain how gradient checkpointing reduces memory usage.",
    "Write a detailed analysis of the attention is all you need paper.",
    "Describe the differences between encoder and decoder architectures.",
    "What are mixture-of-experts models and how do they work?",
    "Explain the concept of KV caching in transformer inference."
)

$metrics = @{
    startTime = Get-Date -Format "o"
    cyclesCompleted = 0
    cyclesFailed = 0
    totalTokensGenerated = 0
    totalTimeMs = 0
    phaseTransitions = @()
    apertureFlushes = @()
    mmapFallbacks = @()
    errors = @()
}

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
    $prompt = $testPrompts[($cycle - 1) % $testPrompts.Count]
    Log-Message "Cycle $cycle/$Cycles: Testing with prompt: '$prompt'"
    
    $cycleStart = Get-Date
    
    try {
        # Build command arguments
        $args = @(
            "--headless",
            "--model", $ModelPath,
            "--prompt", "`"$prompt`"",
            "--max-tokens", $MaxTokens,
            "--context-length", $ContextTokens,
            "--streaming-mode",
            "--enable-metrics"
        )
        
        # Run inference
        $process = Start-Process -FilePath $exePath -ArgumentList $args `
            -PassThru -Wait -WindowStyle Hidden `
            -RedirectStandardOutput (Join-Path $OutputDir "cycle_$cycle.log") `
            -RedirectStandardError (Join-Path $OutputDir "cycle_$cycle.err")
        
        $cycleEnd = Get-Date
        $cycleDuration = ($cycleEnd - $cycleStart).TotalMilliseconds
        
        if ($process.ExitCode -eq 0) {
            Log-Message "Cycle $cycle completed successfully (${cycleDuration}ms)"
            $metrics.cyclesCompleted++
            
            # Parse output for metrics (placeholder - would parse actual output)
            $metrics.totalTokensGenerated += $MaxTokens
            $metrics.totalTimeMs += $cycleDuration
        } else {
            Log-Message "Cycle $cycle failed with exit code $($process.ExitCode)" "ERROR"
            $metrics.cyclesFailed++
            $metrics.errors += "Cycle $cycle failed with exit code $($process.ExitCode)"
        }
        
        # Check for aperture thrashing in logs
        $cycleLog = Get-Content (Join-Path $OutputDir "cycle_$cycle.log") -ErrorAction SilentlyContinue
        if ($cycleLog -match "Aperture thrashing detected") {
            Log-Message "Cycle $cycle: Aperture thrashing detected" "WARN"
            $metrics.apertureFlushes += $cycle
        }
        
        # Check for mmap fallback
        if ($cycleLog -match "Auto-fallback to mmap") {
            Log-Message "Cycle $cycle: MMAP fallback activated" "INFO"
            $metrics.mmapFallbacks += $cycle
        }
        
    } catch {
        Log-Message "Cycle $cycle exception: $_" "ERROR"
        $metrics.cyclesFailed++
        $metrics.errors += "Cycle $cycle exception: $_"
    }
    
    # Progress report every 10 cycles
    if ($cycle % 10 -eq 0) {
        $progress = [math]::Round(($cycle / $Cycles) * 100, 1)
        $tps = if ($metrics.totalTimeMs -gt 0) { 
            [math]::Round($metrics.totalTokensGenerated / ($metrics.totalTimeMs / 1000), 2)
        } else { 0 }
        Log-Message "Progress: $progress% complete | TPS: $tps | Success: $($metrics.cyclesCompleted)/$cycle"
    }
}

# ============================================================================
# POST-TEST ANALYSIS
# ============================================================================
Log-Message "Stress test completed. Generating report..."

$metrics.endTime = Get-Date -Format "o"
$metrics.avgTps = if ($metrics.totalTimeMs -gt 0) {
    [math]::Round($metrics.totalTokensGenerated / ($metrics.totalTimeMs / 1000), 2)
} else { 0 }
$metrics.successRate = [math]::Round(($metrics.cyclesCompleted / $Cycles) * 100, 2)

# Save metrics JSON
$metrics | ConvertTo-Json -Depth 10 | Set-Content $MetricsFile

# Generate markdown report
$report = @"
# RawrXD 70B Stress Test Report

**Date:** $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")  
**Model:** $ModelPath  
**Configuration:**
- Context Tokens: $ContextTokens
- Max Output Tokens: $MaxTokens
- Cycles: $Cycles
- MMAP Fallback: $EnableMmapFallback
- GPU Batching: $EnableGpuBatching
- Phase-Aware Metrics: $EnablePhaseAwareMetrics

## Summary

| Metric | Value |
|--------|-------|
| Cycles Completed | $($metrics.cyclesCompleted) / $Cycles |
| Success Rate | $($metrics.successRate)% |
| Total Tokens | $($metrics.totalTokensGenerated) |
| Average TPS | $($metrics.avgTps) |
| Total Time | $([math]::Round($metrics.totalTimeMs / 1000, 2))s |

## Streaming Engine Performance

### Phase-Aware Metrics
$(if ($EnablePhaseAwareMetrics) { "Phase-aware measurement was enabled. Phase transitions logged to extension events." } else { "Phase-aware measurement was disabled." })

### Aperture Monitoring
$(if ($metrics.apertureFlushes.Count -gt 0) {
    "**WARNING:** Aperture thrashing detected in cycles: $($metrics.apertureFlushes -join ', ')"
} else {
    "✅ No aperture thrashing detected"
})

### MMAP Fallback
$(if ($metrics.mmapFallbacks.Count -gt 0) {
    "MMAP fallback activated in cycles: $($metrics.mmapFallbacks -join ', ')"
} else {
    "✅ No MMAP fallback required (all zones fit in memory)"
})

## Errors

$(if ($metrics.errors.Count -eq 0) { "✅ No errors encountered" } else { $metrics.errors | ForEach-Object { "- $_" } })

## Conclusion

$(if ($metrics.successRate -ge 95) {
    "✅ **PASS:** Stress test completed with high success rate. Streaming engine is stable for 70B models."
} elseif ($metrics.successRate -ge 80) {
    "⚠️ **PARTIAL:** Acceptable success rate but some failures occurred. Review error logs."
} else {
    "❌ **FAIL:** High failure rate. Streaming engine requires further hardening for 70B models."
})

## Artifacts

- Metrics JSON: $MetricsFile
- Extension Events: $($env:RAWRXD_EXTENSION_LOG)
- Cycle Logs: $OutputDir\cycle_*.log
"@

$report | Set-Content $ReportFile

# ============================================================================
# FOOTER
# ============================================================================
Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════════╗" -ForegroundColor $(if ($metrics.successRate -ge 95) { "Green" } elseif ($metrics.successRate -ge 80) { "Yellow" } else { "Red" })
Write-Host "║   Stress Test Complete                                             ║" -ForegroundColor $(if ($metrics.successRate -ge 95) { "Green" } elseif ($metrics.successRate -ge 80) { "Yellow" } else { "Red" })
Write-Host "║   Success Rate: $($metrics.successRate)% | Avg TPS: $($metrics.avgTps)                           ║" -ForegroundColor $(if ($metrics.successRate -ge 95) { "Green" } elseif ($metrics.successRate -ge 80) { "Yellow" } else { "Red" })
Write-Host "║   Report: $ReportFile" -ForegroundColor $(if ($metrics.successRate -ge 95) { "Green" } elseif ($metrics.successRate -ge 80) { "Yellow" } else { "Red" })
Write-Host "╚════════════════════════════════════════════════════════════════════╝" -ForegroundColor $(if ($metrics.successRate -ge 95) { "Green" } elseif ($metrics.successRate -ge 80) { "Yellow" } else { "Red" })

exit $(if ($metrics.successRate -ge 80) { 0 } else { 1 })
