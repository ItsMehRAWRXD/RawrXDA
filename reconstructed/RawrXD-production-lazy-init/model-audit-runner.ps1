#!/usr/bin/env pwsh
# Model Audit Runner - Sequential model loading and project analysis
# Loads each large model one at a time, performs audit, unloads, then proceeds to next

param(
    [string]$ProjectRoot = "D:\RawrXD-production-lazy-init",
    [string]$OutputDir = "D:\Model-Audit-Reports"
)

# Ensure output directory exists
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Model configurations to test
$models = @(
    @{Number=3; Name="BigDaddyG-Q2_K-CHEETAH"; Size="23.71 GB"; Description="Cheetah variant - speed optimized"},
    @{Number=2; Name="BigDaddyG-NO-REFUSE-Q4_K_M"; Size="36.20 GB"; Description="No-refuse variant - uncensored"},
    @{Number=36; Name="Codestral-22B-v0.1-hf"; Size="11.79 GB"; Description="Code-specialized model"}
)

# Audit prompt template
$auditPrompt = @"
You are an expert code auditor analyzing the RawrXD project - a C++ AI IDE with Qt GUI, API server, GGUF model loading, Vulkan compute, and CLI interface.

PROJECT LOCATION: $ProjectRoot

Please perform a comprehensive audit covering:

1. ARCHITECTURE ANALYSIS
   - Overall design patterns and code organization
   - Component coupling and cohesion
   - Modularity and separation of concerns
   - Key architectural strengths and weaknesses

2. CODE QUALITY
   - Memory management (leaks, smart pointers, RAII)
   - Exception handling patterns
   - Error propagation and recovery
   - Resource cleanup and RAII compliance
   - Modern C++ usage (C++17/20 features)

3. SECURITY VULNERABILITIES
   - Input validation and sanitization
   - Buffer overflow risks
   - SQL injection potential
   - Command injection vectors
   - Hardcoded credentials or secrets
   - Path traversal vulnerabilities
   - Unsafe API usage

4. PERFORMANCE CONCERNS
   - Threading and concurrency issues
   - Memory allocation patterns
   - Algorithmic complexity
   - I/O operations
   - GPU compute optimization

5. BUILD SYSTEM
   - CMake configuration quality
   - Dependency management
   - Platform compatibility
   - Build optimization

6. TESTING & DOCUMENTATION
   - Test coverage assessment
   - Documentation completeness
   - API documentation quality
   - Code comments

7. CRITICAL ISSUES
   - High-priority bugs or vulnerabilities
   - Immediate action items
   - Risk assessment

8. RECOMMENDATIONS
   - Prioritized improvement suggestions
   - Refactoring opportunities
   - Technical debt reduction strategies

Please provide specific file names, line numbers (if possible), and code examples where relevant. Be thorough but concise.
"@

function Invoke-ModelAudit {
    param(
        [hashtable]$Model,
        [string]$Prompt,
        [string]$OutputPath
    )
    
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "AUDITING WITH: $($Model.Name) ($($Model.Size))" -ForegroundColor Cyan
    Write-Host "Description: $($Model.Description)" -ForegroundColor Gray
    Write-Host "========================================`n" -ForegroundColor Cyan
    
    $cliPath = Join-Path $ProjectRoot "build\bin-msvc\Release\RawrXD-CLI.exe"
    $reportPath = Join-Path $OutputPath "$($Model.Name)-Audit-$(Get-Date -Format 'yyyyMMdd-HHmmss').md"
    $tempOutput = Join-Path $OutputPath "temp_$($Model.Number).txt"
    
    # Create command sequence
    # 1. Cache models
    # 2. Load specific model
    # 3. Use analyzeproject command to audit the codebase
    # 4. Unload and quit
    
    Write-Host "[1/5] Caching models list..." -ForegroundColor Yellow
    Write-Host "[2/5] Loading model #$($Model.Number): $($Model.Name)..." -ForegroundColor Yellow
    Write-Host "[3/5] Analyzing project (this may take 5-15 minutes for large models)..." -ForegroundColor Yellow
    
    $commands = @"
models
load $($Model.Number)
analyzeproject $ProjectRoot
unload
quit
"@
    
    try {
        # Run CLI with piped commands
        $output = $commands | & $cliPath 2>&1 | Out-String
        
        # Save raw output
        $output | Out-File -FilePath $tempOutput -Encoding UTF8
        
        Write-Host "[4/5] Model execution complete" -ForegroundColor Green
        Write-Host "[5/5] Saving report..." -ForegroundColor Yellow
        
        # Create formatted report
        $report = @"
# RawrXD Project Audit Report
**Model**: $($Model.Name) ($($Model.Size))
**Description**: $($Model.Description)
**Date**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Model Number**: $($Model.Number)

---

## Analysis Command

``````bash
analyzeproject $ProjectRoot
``````

---

## Model Analysis Output

``````
$output
``````

---

**Note**: This report contains the model's comprehensive project analysis including architecture, security, performance, and code quality findings.

"@
        
        $report | Out-File -FilePath $reportPath -Encoding UTF8
        Write-Host "✅ Report saved to: $reportPath" -ForegroundColor Green
        
        # Clean up temp file
        Remove-Item -Path $tempOutput -Force -ErrorAction SilentlyContinue
        
        return $reportPath
        
    } catch {
        Write-Host "[ERROR] Model execution failed: $_" -ForegroundColor Red
        return $null
    }
}

# Main execution
Write-Host @"

╔═══════════════════════════════════════════════════╗
║   RawrXD Multi-Model Audit Runner                ║
║   Sequential Model Testing and Comparison         ║
╚═══════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

$results = @()

foreach ($model in $models) {
    $reportPath = Invoke-ModelAudit -Model $model -Prompt $auditPrompt -OutputPath $OutputDir
    
    if ($reportPath) {
        $results += @{
            Model = $model.Name
            Size = $model.Size
            ReportPath = $reportPath
            Success = $true
        }
    } else {
        $results += @{
            Model = $model.Name
            Size = $model.Size
            ReportPath = $null
            Success = $false
        }
    }
    
    # Wait between models to ensure clean shutdown
    Write-Host "`nWaiting 5 seconds before next model...`n" -ForegroundColor Gray
    Start-Sleep -Seconds 5
}

# Generate comparison summary
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "AUDIT SUMMARY" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

$summaryPath = Join-Path $OutputDir "Audit-Comparison-Summary-$(Get-Date -Format 'yyyyMMdd-HHmmss').md"

$summary = @"
# RawrXD Multi-Model Audit Comparison
**Date**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Models Tested**: $($models.Count)

## Test Results

| Model | Size | Status | Report |
|-------|------|--------|--------|
"@

foreach ($result in $results) {
    $status = if ($result.Success) { "✅ Success" } else { "❌ Failed" }
    $reportLink = if ($result.Success) { "[View Report]($($result.ReportPath))" } else { "N/A" }
    $summary += "`n| $($result.Model) | $($result.Size) | $status | $reportLink |"
}

$summary += @"


## Next Steps

1. Review individual model reports in: `$OutputDir`
2. Compare findings across models
3. Identify consensus issues (reported by multiple models)
4. Prioritize fixes based on severity and frequency
5. Address critical security and performance issues first

## Report Locations

"@

foreach ($result in $results) {
    if ($result.Success) {
        $summary += "- **$($result.Model)**: ``$($result.ReportPath)```n"
    }
}

$summary | Out-File -FilePath $summaryPath -Encoding UTF8
Write-Host "Comparison summary saved to: $summaryPath" -ForegroundColor Green

# Display results table
$results | ForEach-Object {
    $color = if ($_.Success) { "Green" } else { "Red" }
    $status = if ($_.Success) { "SUCCESS" } else { "FAILED" }
    Write-Host "$($_.Model): $status" -ForegroundColor $color
}

Write-Host "`n✅ All audits complete!`n" -ForegroundColor Cyan
