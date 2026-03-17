#!/usr/bin/env pwsh
# RawrXD Final Comprehensive Audit - Direct Method

$OutputDir = "D:\Model-Audit-Reports"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$cliPath = "D:\RawrXD-production-lazy-init\build\bin-msvc\Release\RawrXD-CLI.exe"

Write-Host @"

╔═══════════════════════════════════════════════════════╗
║  RawrXD COMPREHENSIVE AUDIT                          ║
║  AI-Powered Analysis with BigDaddyG-Q2_K-CHEETAH     ║
╚═══════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

# Step 1: Start CLI
Write-Host "STEP 1: Starting CLI and loading model (23.71 GB)..." -ForegroundColor Yellow
Write-Host "        This will take 30-90 seconds..." -ForegroundColor Gray

$commands = @"
models
load 3
"@

$output = $commands | & $cliPath 2>&1 | Select-Object -First 50
$portMatch = $output | Where-Object { $_ -match '\[Port: (\d+)\]' } | Select-Object -First 1
$port = if ($portMatch -match '\[Port: (\d+)\]') { $matches[1] } else { $null }

if (-not $port) {
    Write-Host "[ERROR] Could not start CLI or find port" -ForegroundColor Red
    exit 1
}

Write-Host "✓ CLI running on port $port" -ForegroundColor Green
Write-Host "✓ Model #3 (BigDaddyG-Q2_K-CHEETAH, 23.71 GB) loading..." -ForegroundColor Green

# Give model time to load
Write-Host "`nWaiting for model to fully load..." -ForegroundColor Yellow
Start-Sleep -Seconds 60

# Step 2: Run audits via API
Write-Host "`nSTEP 2: Performing comprehensive audits..." -ForegroundColor Yellow

$audits = @(
    @{
        Name = "ARCHITECTURE"
        Prompt = "Analyze RawrXD's C++ architecture. Identify design patterns used. How are components coupled? Evaluate scalability, modularity, and separation of concerns. What are the main architectural strengths and weaknesses? Provide specific recommendations."
    },
    @{
        Name = "SECURITY VULNERABILITIES"
        Prompt = "Perform a comprehensive security audit of RawrXD. Identify the TOP 5 most critical security vulnerabilities. Focus on: buffer overflows, SQL injection, command injection, hardcoded secrets, API security, authentication/authorization issues. List specific files and issues."
    },
    @{
        Name = "PERFORMANCE"
        Prompt = "Analyze RawrXD's performance characteristics. What are the main bottlenecks? How is threading managed? Are there memory leaks or inefficient allocation patterns? What O(n2) or worse algorithms exist? How well is GPU/Vulkan compute utilized? Suggest optimizations."
    },
    @{
        Name = "CODE QUALITY"
        Prompt = "Review RawrXD's code quality. How well is modern C++17 utilized? What memory management issues exist (smart pointers vs raw pointers)? Where is code duplicated? How comprehensive is error handling? What refactoring would improve code quality?"
    },
    @{
        Name = "CRITICAL ISSUES"
        Prompt = "Identify the most critical issues in RawrXD that require immediate attention. What are the highest priority bugs? What security issues pose the greatest risk? What technical debt is most urgent? Prioritize by severity and business impact."
    }
)

$reportContent = @"
# RawrXD - Comprehensive AI Audit Report

**Date**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Model**: BigDaddyG-Q2_K-CHEETAH (23.71 GB)
**Analysis Method**: Direct API via HTTP `/api/generate` endpoint
**Total Audits**: $($audits.Count)

---

## Overview

This report contains comprehensive AI-powered analysis of the RawrXD project across multiple dimensions:
- Architecture and design
- Security posture and vulnerabilities
- Performance characteristics
- Code quality metrics
- Critical issues and priorities

---

"@

foreach ($audit in $audits) {
    Write-Host "`n  → $($audit.Name)..." -ForegroundColor Cyan
    
    $body = @{
        model = ""
        prompt = $audit.Prompt
        stream = $false
        options = @{
            temperature = 0.4
            num_predict = 3000
        }
    } | ConvertTo-Json -Depth 10
    
    try {
        $response = Invoke-RestMethod `
            -Uri "http://localhost:$port/api/generate" `
            -Method Post `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 600
        
        $auditResult = $response.response
        
        $reportContent += @"
## $($audit.Name)

$auditResult

---

"@
        
        Write-Host "    ✓ Complete ($(($auditResult.Length)/1KB)K)" -ForegroundColor Green
        
        Start-Sleep -Seconds 3
        
    } catch {
        Write-Host "    ✗ Failed: $($_.Exception.Message)" -ForegroundColor Red
        $reportContent += @"
## $($audit.Name)

**ERROR**: Analysis failed - $($_.Exception.Message)

---

"@
    }
}

# Add summary
$reportContent += @"
## Report Summary

**Analysis Sections**: $($audits.Count)
**Total Content**: Approximately $(($reportContent.Length)/1KB)K

**Generated**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Model**: BigDaddyG-Q2_K-CHEETAH
**Status**: Complete

---

### Recommendations

Based on the audit findings above:

1. **Address Security Issues** - Implement fixes for identified vulnerabilities
2. **Refactor Architecture** - Apply architectural improvements
3. **Optimize Performance** - Implement performance optimizations
4. **Improve Code Quality** - Refactor and improve code quality
5. **Resolve Critical Issues** - Address high-priority items immediately

---

*End of Report*
"@

# Save report
$reportPath = Join-Path $OutputDir "RawrXD-FULL-AUDIT-$(Get-Date -Format 'yyyyMMdd-HHmmss').md"
$reportContent | Out-File -FilePath $reportPath -Encoding UTF8

Write-Host "`nSTEP 3: Report generated" -ForegroundColor Yellow
Write-Host "✓ Path: $reportPath" -ForegroundColor Green
Write-Host "✓ Size: $('{0:N0}' -f ((Get-Item $reportPath).Length / 1KB))KB" -ForegroundColor Green

Write-Host "`n" -ForegroundColor Green
Write-Host "╔═══════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║  AUDIT COMPLETE                                      ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════╝" -ForegroundColor Green

# Cleanup
Write-Host "`nCleaning up..." -ForegroundColor Yellow
Get-Process RawrXD-CLI -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

Write-Host "✓ Complete" -ForegroundColor Green
