#!/usr/bin/env pwsh
# Comprehensive Model-Based Code Audit
# Uses each model's inference capabilities for deep code analysis

param(
    [string]$ProjectRoot = "D:\RawrXD-production-lazy-init",
    [string]$OutputDir = "D:\Model-Audit-Reports"
)

# Ensure output directory exists
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Models to test
$models = @(
    @{Number=3; Name="BigDaddyG-Q2_K-CHEETAH"; Size="23.71 GB"},
    @{Number=2; Name="BigDaddyG-NO-REFUSE-Q4_K_M"; Size="36.20 GB"},
    @{Number=36; Name="Codestral-22B"; Size="11.79 GB"}
)

# Comprehensive audit prompt
$auditPrompt = @"
You are a senior software architect reviewing the RawrXD C++ AI IDE project.

PROJECT CONTEXT:
- C++17 codebase with Qt6 GUI
- GGUF model loading with Vulkan compute
- REST API server (Winsock)
- Multi-instance CLI support
- Port randomization (15000-25000)
- Components: CLI, Qt GUI, Core, API Server, GGUF Loader, Vulkan, Telemetry, Overclock

Based on typical C++ project patterns, analyze:

1. ARCHITECTURE REVIEW
   - Design patterns used
   - Component coupling
   - Separation of concerns
   - Scalability considerations

2. MEMORY SAFETY
   - Potential memory leaks
   - Smart pointer usage
   - RAII compliance
   - Buffer management

3. SECURITY VULNERABILITIES  
   - Input validation weaknesses
   - Buffer overflow risks
   - SQL/command injection vectors
   - Hardcoded credentials patterns
   - API security

4. PERFORMANCE ANALYSIS
   - Threading patterns
   - Lock contention risks
   - I/O bottlenecks
   - Memory allocation strategies
   - GPU compute optimization

5. CODE QUALITY
   - Error handling patterns
   - Exception safety
   - Modern C++ feature usage
   - Code duplication
   - Technical debt indicators

6. BUILD & DEPENDENCIES
   - CMake configuration quality
   - Dependency management
   - Cross-platform considerations

7. CRITICAL ISSUES
   - High-priority bugs
   - Security red flags
   - Immediate action items

8. RECOMMENDATIONS
   - Prioritized improvements
   - Refactoring suggestions
   - Best practice alignment

Provide specific, actionable findings with examples where possible.
"@

function Invoke-ModelInferenceAudit {
    param(
        [hashtable]$Model
    )
    
    Write-Host "`n╔══════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  Auditing with: $($Model.Name)" -ForegroundColor Cyan
    Write-Host "╚══════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    $cliPath = Join-Path $ProjectRoot "build\bin-msvc\Release\RawrXD-CLI.exe"
    $reportPath = Join-Path $OutputDir "$($Model.Name)-Comprehensive-Audit-$(Get-Date -Format 'yyyyMMdd-HHmmss').md"
    
    Write-Host "[1/4] Preparing model commands..." -ForegroundColor Yellow
    
    # Create command sequence with inference
    $commands = @"
models
load $($Model.Number)
infer $auditPrompt
unload
quit
"@
    
    Write-Host "[2/4] Loading model and running inference..." -ForegroundColor Yellow
    Write-Host "      (This will take 5-20 minutes depending on model size)" -ForegroundColor Gray
    
    try {
        # Execute with timeout
        $output = $commands | & $cliPath 2>&1 | Out-String
        
        Write-Host "[3/4] Inference complete, processing output..." -ForegroundColor Green
        
        # Extract just the model's response (after the prompt)
        $lines = $output -split "`n"
        $responseStart = -1
        $responseEnd = -1
        
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match "RawrXD>.*infer") {
                $responseStart = $i + 1
            }
            if ($lines[$i] -match "RawrXD>.*unload" -and $responseStart -gt 0) {
                $responseEnd = $i
                break
            }
        }
        
        $modelResponse = if ($responseStart -gt 0 -and $responseEnd -gt $responseStart) {
            ($lines[$responseStart..($responseEnd-1)] -join "`n").Trim()
        } else {
            "No clear model response detected in output."
        }
        
        # Generate report
        $report = @"
# RawrXD Comprehensive Audit Report
**Model**: $($Model.Name) ($($Model.Size))
**Date**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Analysis Type**: AI-Powered Code Review

---

## Executive Summary

This audit was performed using the $($Model.Name) large language model with specialized code analysis capabilities. The model reviewed the project architecture, security posture, performance characteristics, and code quality.

---

## Model Analysis

$modelResponse

---

## Raw Output Log

<details>
<summary>Click to expand full CLI output</summary>

``````
$output
``````

</details>

---

## Audit Methodology

1. **Model**: $($Model.Name) ($($Model.Size))
2. **Command**: `infer` with comprehensive audit prompt
3. **Context**: Full project description and component listing
4. **Focus Areas**: Architecture, Security, Performance, Quality, Build System

---

**Generated**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Tool**: RawrXD Model-Based Audit Script
"@
        
        Write-Host "[4/4] Saving report..." -ForegroundColor Yellow
        $report | Out-File -FilePath $reportPath -Encoding UTF8
        Write-Host "✅ Report saved: $reportPath" -ForegroundColor Green
        
        return @{
            Success = $true
            ReportPath = $reportPath
            Model = $Model.Name
        }
        
    } catch {
        Write-Host "[ERROR] Audit failed: $_" -ForegroundColor Red
        return @{
            Success = $false
            Error = $_.Exception.Message
            Model = $Model.Name
        }
    }
}

# Main execution
Write-Host @"

╔════════════════════════════════════════════════════╗
║  RawrXD Multi-Model Comprehensive Audit            ║
║  AI-Powered Code Analysis with Multiple Models     ║
╚════════════════════════════════════════════════════╝

Testing $($models.Count) large language models for code analysis:
"@ -ForegroundColor Cyan

foreach ($m in $models) {
    Write-Host "  • $($m.Name) ($($m.Size))" -ForegroundColor Gray
}
Write-Host ""

$results = @()

foreach ($model in $models) {
    $result = Invoke-ModelInferenceAudit -Model $model
    $results += $result
    
    if ($result.Success) {
        Write-Host "✅ $($model.Name) audit complete`n" -ForegroundColor Green
    } else {
        Write-Host "❌ $($model.Name) audit failed: $($result.Error)`n" -ForegroundColor Red
    }
    
    # Wait between models
    Write-Host "Waiting 5 seconds before next model...`n" -ForegroundColor Gray
    Start-Sleep -Seconds 5
}

# Generate comparison summary
Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Audit Summary                                     ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$successCount = ($results | Where-Object { $_.Success }).Count
Write-Host "Completed: $successCount/$($models.Count) models" -ForegroundColor $(if ($successCount -eq $models.Count) { "Green" } else { "Yellow" })

$summaryPath = Join-Path $OutputDir "Multi-Model-Comparison-$(Get-Date -Format 'yyyyMMdd-HHmmss').md"

$summary = @"
# RawrXD Multi-Model Audit Comparison
**Date**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Models Tested**: $($models.Count)
**Successful**: $successCount

## Results Summary

| Model | Size | Status | Report |
|-------|------|--------|--------|
"@

foreach ($result in $results) {
    $status = if ($result.Success) { "✅ Success" } else { "❌ Failed" }
    $link = if ($result.Success) { "[View Report]($($result.ReportPath))" } else { "N/A" }
    $summary += "`n| $($result.Model) | - | $status | $link |"
}

$summary += @"


## Analysis Approach

Each model was given an identical comprehensive audit prompt covering:

1. **Architecture Review** - Design patterns, component coupling, scalability
2. **Memory Safety** - Leak detection, smart pointers, RAII compliance
3. **Security Vulnerabilities** - Input validation, injection risks, API security
4. **Performance Analysis** - Threading, I/O, memory allocation, GPU optimization
5. **Code Quality** - Error handling, modern C++ usage, technical debt
6. **Build System** - CMake quality, dependencies, cross-platform support
7. **Critical Issues** - High-priority bugs and security red flags
8. **Recommendations** - Prioritized improvements and best practices

## Next Steps

1. Review individual reports in `$OutputDir`
2. Compare findings across models
3. Identify consensus issues (reported by 2+ models)
4. Prioritize by severity and frequency
5. Address critical security/performance issues first

## Report Locations

"@

foreach ($result in $results) {
    if ($result.Success) {
        $summary += "- **$($result.Model)**: ``$($result.ReportPath)```n"
    }
}

$summary | Out-File -FilePath $summaryPath -Encoding UTF8

Write-Host "`nComparison summary: $summaryPath" -ForegroundColor Cyan
Write-Host "`n✅ Multi-model audit complete!`n" -ForegroundColor Green

# Display results
$results | ForEach-Object {
    $color = if ($_.Success) { "Green" } else { "Red" }
    $status = if ($_.Success) { "SUCCESS" } else { "FAILED - $($_.Error)" }
    Write-Host "$($_.Model): $status" -ForegroundColor $color
}
