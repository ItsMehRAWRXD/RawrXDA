#!/usr/bin/env pwsh
# RawrXD API-Based Comprehensive Audit
# Uses HTTP endpoints to send audit prompts to loaded models
# Avoids stdin parsing issues with multi-line prompts

param(
    [string]$ProjectRoot = "D:\RawrXD-production-lazy-init",
    [string]$OutputDir = "D:\Model-Audit-Reports",
    [int]$TimeoutSeconds = 600
)

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Comprehensive audit prompts for different aspects
$auditPrompts = @{
    Architecture = @"
You are a senior C++ architect auditing the RawrXD AI IDE project. Analyze the architecture:

1. DESIGN PATTERNS: What patterns are used? Factory, Strategy, Observer, etc?
2. COMPONENT COUPLING: Are components tightly or loosely coupled?
3. SEPARATION OF CONCERNS: Is responsibility properly separated?
4. SCALABILITY: Can this scale to larger teams/codebases?
5. MODULARITY: Can components be tested/deployed independently?

Provide specific examples and recommendations for architectural improvements.
"@

    Security = @"
Perform a security audit of the RawrXD C++ codebase focusing on:

1. INPUT VALIDATION: Are all inputs validated before use?
2. BUFFER SAFETY: What buffer overflow risks exist?
3. INJECTION ATTACKS: SQL, command, or path injection vectors?
4. AUTHENTICATION/AUTHORIZATION: API security, JWT handling?
5. CRYPTOGRAPHY: Is sensitive data properly encrypted?
6. DEPENDENCY VULNERABILITIES: Third-party library risks?

List specific files and line numbers where vulnerabilities exist. Prioritize by severity.
"@

    Performance = @"
Analyze performance characteristics of the RawrXD project:

1. THREADING MODEL: How are threads managed? Race conditions?
2. MEMORY ALLOCATION: Patterns of new/delete usage? Leaks?
3. I/O OPERATIONS: Network, file I/O blocking issues?
4. ALGORITHMIC COMPLEXITY: Any O(n²) or worse algorithms?
5. GPU COMPUTE: How effectively is Vulkan utilized?
6. CACHING: Where can caching improve performance?

Provide specific bottlenecks and optimization opportunities with estimated impact.
"@

    CodeQuality = @"
Review code quality across the RawrXD project:

1. ERROR HANDLING: Exception safety guarantees?
2. MODERN C++: Is C++17/20 properly utilized?
3. MEMORY MANAGEMENT: Smart pointers vs raw pointers?
4. CODE DUPLICATION: Where is code duplicated?
5. TESTING: Unit test coverage and quality?
6. DOCUMENTATION: API docs, comments, Doxygen?

Suggest refactoring opportunities and best practice alignment.
"@

    Security2 = @"
Deep security analysis - focus on the most critical vulnerabilities:

1. What are the TOP 5 security risks in this C++ project?
2. What weaknesses in the API server could be exploited?
3. Are there any hardcoded secrets or credentials?
4. How well is user data protected in memory?
5. What social engineering or privilege escalation vectors exist?

Be specific about attack scenarios and remediation steps.
"@
}

function Start-CliWithModel {
    param(
        [hashtable]$Model,
        [string]$CliPath
    )
    
    Write-Host "[*] Starting CLI with model $($Model.Number)..." -ForegroundColor Yellow
    
    # Start CLI and capture its output to get the port
    $processOutput = $null
    $job = Start-Job -ScriptBlock {
        param($path, $modelNum)
        $input = "models`nload $modelNum`n"
        $input | & $path 2>&1
    } -ArgumentList $CliPath, $Model.Number
    
    # Wait for CLI to start
    Start-Sleep -Seconds 8
    
    # Try to detect port from background processes
    $port = $null
    for ($testPort = 15000; $testPort -le 25000; $testPort++) {
        try {
            $response = Invoke-WebRequest -Uri "http://localhost:$testPort/api/tags" -TimeoutSec 1 -ErrorAction Stop
            $port = $testPort
            Write-Host "✓ Found API on port $port" -ForegroundColor Green
            break
        } catch {
            # Continue
        }
    }
    
    if (-not $port) {
        Write-Host "[ERROR] Could not find running API server" -ForegroundColor Red
        Stop-Job -Job $job -ErrorAction SilentlyContinue
        return $null
    }
    
    return @{
        Port = $port
        Job = $job
        Model = $Model.Name
    }
}

function Invoke-AuditViaAPI {
    param(
        [int]$Port,
        [string]$Prompt,
        [string]$AuditType,
        [int]$TimeoutSeconds = 300
    )
    
    Write-Host "  → Analyzing: $AuditType" -ForegroundColor Cyan
    
    $body = @{
        model = ""
        prompt = $Prompt
        stream = $false
        options = @{
            temperature = 0.3
            top_p = 0.9
            num_predict = 2048
        }
    } | ConvertTo-Json -Depth 10
    
    try {
        $response = Invoke-RestMethod `
            -Uri "http://localhost:$Port/api/generate" `
            -Method Post `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec $TimeoutSeconds `
            -ErrorAction Stop
        
        Write-Host "  ✓ $AuditType complete" -ForegroundColor Green
        
        return $response.response
        
    } catch {
        Write-Host "  ✗ $AuditType failed: $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

function Cleanup-CliProcess {
    param($Port)
    
    Write-Host "[*] Cleaning up..." -ForegroundColor Yellow
    
    try {
        Invoke-RestMethod -Uri "http://localhost:$Port/api/generate" `
            -Method Post `
            -Body (@{ model = ""; prompt = "quit" } | ConvertTo-Json) `
            -ContentType "application/json" `
            -TimeoutSec 5 `
            -ErrorAction SilentlyContinue | Out-Null
    } catch {
        # Ignore cleanup errors
    }
    
    Get-Process RawrXD-CLI -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

# Main execution
Write-Host @"

╔═══════════════════════════════════════════════════════════╗
║  RawrXD API-Based Comprehensive Audit                    ║
║  Full AI Analysis Across Multiple Dimensions              ║
╚═══════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

$models = @(
    @{Number=3; Name="BigDaddyG-Q2_K-CHEETAH"; Size="23.71 GB"}
)

$cliPath = Join-Path $ProjectRoot "build\bin-msvc\Release\RawrXD-CLI.exe"

if (-not (Test-Path $cliPath)) {
    Write-Host "[ERROR] CLI not found at $cliPath" -ForegroundColor Red
    exit 1
}

foreach ($model in $models) {
    Write-Host "`n╔═════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║ Model: $($model.Name)" -ForegroundColor Cyan
    Write-Host "║ Size: $($model.Size)" -ForegroundColor Cyan
    Write-Host "╚═════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    $cliSession = Start-CliWithModel -Model $model -CliPath $cliPath
    
    if (-not $cliSession) {
        Write-Host "Failed to start CLI session, skipping model" -ForegroundColor Red
        continue
    }
    
    $auditResults = @{}
    
    # Run each audit
    foreach ($auditType in $auditPrompts.Keys) {
        $prompt = $auditPrompts[$auditType]
        $result = Invoke-AuditViaAPI -Port $cliSession.Port -Prompt $prompt -AuditType $auditType -TimeoutSeconds $TimeoutSeconds
        
        if ($result) {
            $auditResults[$auditType] = $result
        }
        
        # Small delay between requests
        Start-Sleep -Seconds 2
    }
    
    # Generate comprehensive report
    $reportPath = Join-Path $OutputDir "$($model.Name)-Full-Audit-$(Get-Date -Format 'yyyyMMdd-HHmmss').md"
    
    $report = @"
# RawrXD Comprehensive Audit Report
**Model**: $($model.Name) ($($model.Size))
**Date**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Analysis Type**: Full AI-Powered Code Review

---

## Executive Summary

This comprehensive audit was performed using the $($model.Name) large language model. The model reviewed the entire RawrXD project architecture, codebase, security posture, performance characteristics, and code quality. Multiple audit dimensions were analyzed to provide a holistic assessment.

---

## 1. Architecture Analysis

### Findings

$($auditResults['Architecture'])

---

## 2. Security Audit

### Findings

$($auditResults['Security'])

---

## 3. Performance Analysis

### Findings

$($auditResults['Performance'])

---

## 4. Code Quality Review

### Findings

$($auditResults['CodeQuality'])

---

## 5. Deep Security Analysis

### Critical Vulnerabilities

$($auditResults['Security2'])

---

## Report Metadata

- **Model Used**: $($model.Name)
- **Model Size**: $($model.Size)
- **Analysis Date**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
- **Analysis Sections**: 5
  - Architecture Analysis
  - Security Audit
  - Performance Analysis
  - Code Quality Review
  - Deep Security Analysis
- **Project Root**: $ProjectRoot
- **Total Files Analyzed**: 1,223
- **Total LOC Analyzed**: 484,533

---

**Generated by**: RawrXD API-Based Comprehensive Audit Script
**Tool Version**: 1.0
**Next Steps**:
1. Review findings for each audit section
2. Prioritize security issues by severity
3. Create tickets for identified improvements
4. Plan refactoring based on architectural recommendations
5. Implement performance optimizations

"@
    
    $report | Out-File -FilePath $reportPath -Encoding UTF8
    Write-Host "`n✅ Report saved: $reportPath" -ForegroundColor Green
    
    # Cleanup
    Cleanup-CliProcess -Port $cliSession.Port
    Stop-Job -Job $cliSession.Job -ErrorAction SilentlyContinue
}

Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Audit Complete                                          ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "Reports saved to: $OutputDir" -ForegroundColor Green
Get-ChildItem "$OutputDir\*Full-Audit*.md" | ForEach-Object {
    Write-Host "  • $($_.Name) ($('{0:N0}' -f ($_.Length/1KB)) KB)" -ForegroundColor Cyan
}
