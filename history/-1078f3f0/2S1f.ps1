#!/usr/bin/env pwsh
# RawrXD Comprehensive Audit - Direct API Approach
# Starts CLI, waits for model load, then queries via HTTP API

param(
    [string]$ProjectRoot = "D:\RawrXD-production-lazy-init",
    [string]$OutputDir = "D:\Model-Audit-Reports"
)

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$cliPath = Join-Path $ProjectRoot "build\bin-msvc\Release\RawrXD-CLI.exe"

# Comprehensive audit prompts
$auditPrompts = @(
    @{
        Title = "Architecture Analysis"
        Prompt = "Analyze RawrXD's software architecture. What design patterns are used? How are components coupled? Is it scalable? What are architectural strengths and weaknesses?"
    },
    @{
        Title = "Security Analysis - Vulnerabilities"
        Prompt = "Perform a security audit of RawrXD. What are the TOP 5 security vulnerabilities? Where are buffer overflow risks, SQL injection vectors, hardcoded secrets? List specific files and line numbers."
    },
    @{
        Title = "Performance Analysis"
        Prompt = "Analyze RawrXD performance. What threading issues exist? How efficient is memory usage? Are there O(n2) algorithms? How well is GPU compute utilized? What are optimization opportunities?"
    },
    @{
        Title = "Code Quality"
        Prompt = "Review RawrXD code quality. How well is modern C++17 used? Are there memory management issues? What code is duplicated? How comprehensive is error handling? What refactoring is needed?"
    },
    @{
        Title = "Critical Vulnerabilities"
        Prompt = "What are the most critical security issues in RawrXD that require immediate attention? Focus on exploitable vulnerabilities, privilege escalation vectors, and data protection weaknesses."
    },
    @{
        Title = "Build & Dependencies"
        Prompt = "Analyze RawrXD's build system and dependencies. Is CMake configuration clean? Are dependencies well-managed? Any cross-platform issues? Are there unused or vulnerable dependencies?"
    }
)

Write-Host @"

╔══════════════════════════════════════════════════════════╗
║  RawrXD Comprehensive Audit - Direct API Method         ║
║  Full Dimensional Analysis via HTTP                      ║
╚══════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

Write-Host "Step 1: Starting CLI with BigDaddyG-Q2_K-CHEETAH model (23.71 GB)..." -ForegroundColor Yellow
Write-Host "        (This will take 30-60 seconds to load the model)" -ForegroundColor Gray

# Start CLI process
$proc = Start-Process -FilePath $cliPath -PassThru -WindowStyle Minimized
$pid = $proc.Id

Write-Host "        CLI PID: $pid" -ForegroundColor Gray

# Give it time to start and initialize
Start-Sleep -Seconds 10

# Find the API port
Write-Host "Step 2: Detecting API port..." -ForegroundColor Yellow

$port = $null
$portAttempts = 0
$maxAttempts = 30

while (-not $port -and $portAttempts -lt $maxAttempts) {
    for ($testPort = 15000; $testPort -le 25000; $testPort += 100) {
        try {
            $response = Invoke-WebRequest -Uri "http://localhost:$testPort/api/tags" -TimeoutSec 1 -ErrorAction Stop
            $port = $testPort
            Write-Host "        ✓ Found API on port $port" -ForegroundColor Green
            break
        } catch {
            # Continue
        }
    }
    
    if (-not $port) {
        $portAttempts++
        if ($portAttempts -lt $maxAttempts) {
            Start-Sleep -Seconds 2
        }
    }
}

if (-not $port) {
    Write-Host "        [ERROR] Could not find running API server after $maxAttempts attempts" -ForegroundColor Red
    Stop-Process -Id $pid -Force -ErrorAction SilentlyContinue
    exit 1
}

# Wait a bit more to ensure model is loading
Write-Host "Step 3: Waiting for model initialization (this takes several minutes)..." -ForegroundColor Yellow
Start-Sleep -Seconds 30

# Now perform audits via API
Write-Host "Step 4: Running comprehensive audits..." -ForegroundColor Yellow

$allResults = @{}
$resultCount = 0

foreach ($auditDef in $auditPrompts) {
    $resultCount++
    Write-Host "`n[$resultCount/$($auditPrompts.Count)] $($auditDef.Title)..." -ForegroundColor Cyan
    
    try {
        $body = @{
            model = ""
            prompt = $auditDef.Prompt
            stream = $false
            options = @{
                temperature = 0.4
                top_p = 0.95
                num_predict = 3000
            }
        } | ConvertTo-Json -Depth 10
        
        $response = Invoke-RestMethod `
            -Uri "http://localhost:$port/api/generate" `
            -Method Post `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 600 `
            -ErrorAction Stop
        
        $allResults[$auditDef.Title] = $response.response
        
        Write-Host "        ✓ Complete ($(($response.response.Length)/1KB)K output)" -ForegroundColor Green
        
        # Small delay between requests
        Start-Sleep -Seconds 3
        
    } catch {
        Write-Host "        ✗ Failed: $($_.Exception.Message)" -ForegroundColor Red
        $allResults[$auditDef.Title] = "[ERROR] Analysis failed: $($_.Exception.Message)"
    }
}

# Stop CLI
Write-Host "`nStep 5: Stopping CLI..." -ForegroundColor Yellow
Stop-Process -Id $pid -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

# Generate report
$reportPath = Join-Path $OutputDir "RawrXD-Full-Comprehensive-Audit-$(Get-Date -Format 'yyyyMMdd-HHmmss').md"

Write-Host "Step 6: Generating report..." -ForegroundColor Yellow

$report = @"
# RawrXD - Comprehensive AI-Powered Audit Report

**Date**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Model**: BigDaddyG-Q2_K-CHEETAH (23.71 GB)
**Project**: RawrXD AI IDE with Qt GUI, GGUF Model Loading, Vulkan Compute
**Analysis Type**: Full Dimensional Code Review

---

## Table of Contents

1. [Architecture Analysis](#architecture-analysis)
2. [Security Analysis - Vulnerabilities](#security-analysis---vulnerabilities)
3. [Performance Analysis](#performance-analysis)
4. [Code Quality](#code-quality)
5. [Critical Vulnerabilities](#critical-vulnerabilities)
6. [Build & Dependencies](#build--dependencies)

---

## Architecture Analysis

### Model Findings

$($allResults['Architecture Analysis'])

---

## Security Analysis - Vulnerabilities

### Model Findings

$($allResults['Security Analysis - Vulnerabilities'])

---

## Performance Analysis

### Model Findings

$($allResults['Performance Analysis'])

---

## Code Quality

### Model Findings

$($allResults['Code Quality'])

---

## Critical Vulnerabilities

### Model Findings

$($allResults['Critical Vulnerabilities'])

---

## Build & Dependencies

### Model Findings

$($allResults['Build & Dependencies'])

---

## Summary & Recommendations

Based on the comprehensive AI analysis above, the key action items are:

1. **Address critical vulnerabilities** identified in the security analysis
2. **Implement architectural improvements** suggested in the architecture analysis
3. **Optimize performance** bottlenecks identified in the performance analysis
4. **Refactor code quality** issues from the code quality analysis
5. **Review and update dependencies** as recommended in the build analysis

---

**Report Generated**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Total Analysis Sections**: $($auditPrompts.Count)
**Total Content**: Approximately $(($allResults.Values | Measure-Object -Property Length -Sum).Sum / 1KB)K

"@

$report | Out-File -FilePath $reportPath -Encoding UTF8

Write-Host "`n✅ AUDIT COMPLETE" -ForegroundColor Green
Write-Host "`nReport Location: $reportPath" -ForegroundColor Cyan
Write-Host "Report Size: $('{0:N0}' -f ((Get-Item $reportPath).Length / 1KB))KB" -ForegroundColor Cyan

Write-Host "`n" -ForegroundColor Green
$lines = $report -split "`n" | Measure-Object
Write-Host "Report contains $('{0:N0}' -f $lines.Count) lines of analysis and recommendations" -ForegroundColor Green
