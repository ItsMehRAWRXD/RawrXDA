# Enterprise Auto Model Loader Test Suite
# Comprehensive testing for production readiness

Write-Host "╔════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Enterprise Auto Model Loader Test Suite      ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$testResults = @()
$testsPassed = 0
$testsFailed = 0

function Test-Feature {
    param(
        [string]$Name,
        [scriptblock]$Test,
        [string]$Category = "General"
    )
    
    Write-Host "Testing: $Name" -NoNewline
    try {
        $result = & $Test
        if ($result) {
            Write-Host " ✓" -ForegroundColor Green
            $script:testsPassed++
            $script:testResults += [PSCustomObject]@{
                Category = $Category
                Test = $Name
                Status = "PASSED"
                Message = ""
            }
            return $true
        } else {
            Write-Host " ✗" -ForegroundColor Red
            $script:testsFailed++
            $script:testResults += [PSCustomObject]@{
                Category = $Category
                Test = $Name
                Status = "FAILED"
                Message = "Test returned false"
            }
            return $false
        }
    } catch {
        Write-Host " ✗" -ForegroundColor Red
        $script:testsFailed++
        $script:testResults += [PSCustomObject]@{
            Category = $Category
            Test = $Name
            Status = "FAILED"
            Message = $_.Exception.Message
        }
        return $false
    }
}

# ============================================================================
# Configuration Tests
# ============================================================================
Write-Host "`n[Configuration Tests]" -ForegroundColor Yellow

Test-Feature "Configuration file exists" -Category "Configuration" -Test {
    Test-Path "D:\RawrXD-production-lazy-init\model_loader_config.json"
}

Test-Feature "Configuration is valid JSON" -Category "Configuration" -Test {
    try {
        $config = Get-Content "D:\RawrXD-production-lazy-init\model_loader_config.json" -Raw | ConvertFrom-Json
        return $true
    } catch {
        return $false
    }
}

Test-Feature "Configuration has required fields" -Category "Configuration" -Test {
    $config = Get-Content "D:\RawrXD-production-lazy-init\model_loader_config.json" -Raw | ConvertFrom-Json
    $requiredFields = @("autoLoadEnabled", "maxRetries", "logLevel", "enableMetrics")
    foreach ($field in $requiredFields) {
        if (-not $config.PSObject.Properties.Name.Contains($field)) {
            return $false
        }
    }
    return $true
}

# ============================================================================
# File Structure Tests
# ============================================================================
Write-Host "`n[File Structure Tests]" -ForegroundColor Yellow

Test-Feature "Header file exists" -Category "Structure" -Test {
    Test-Path "D:\RawrXD-production-lazy-init\include\auto_model_loader.h"
}

Test-Feature "Implementation file exists" -Category "Structure" -Test {
    Test-Path "D:\RawrXD-production-lazy-init\src\auto_model_loader.cpp"
}

Test-Feature "Documentation file exists" -Category "Structure" -Test {
    Test-Path "D:\RawrXD-production-lazy-init\docs\AUTO_MODEL_LOADER_ENTERPRISE.md"
}

Test-Feature "Header has enterprise features" -Category "Structure" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    $features = @("CircuitBreaker", "PerformanceMetrics", "LoaderConfig", "ModelMetadata")
    foreach ($feature in $features) {
        if ($header -notmatch $feature) {
            return $false
        }
    }
    return $true
}

# ============================================================================
# Model Discovery Tests
# ============================================================================
Write-Host "`n[Model Discovery Tests]" -ForegroundColor Yellow

Test-Feature "Model directory accessible" -Category "Discovery" -Test {
    Test-Path "D:\OllamaModels"
}

Test-Feature "GGUF models found" -Category "Discovery" -Test {
    $models = Get-ChildItem "D:\OllamaModels" -Filter "*.gguf" -ErrorAction SilentlyContinue
    return ($models.Count -gt 0)
}

Test-Feature "Ollama is installed" -Category "Discovery" -Test {
    try {
        $result = ollama list 2>&1
        return $LASTEXITCODE -eq 0
    } catch {
        return $false
    }
}

Test-Feature "Ollama models available" -Category "Discovery" -Test {
    try {
        $result = ollama list 2>&1 | Out-String
        $lines = $result -split "`n" | Where-Object { $_ -match "^\S+\s+" }
        return ($lines.Count -gt 1)  # Header + at least 1 model
    } catch {
        return $false
    }
}

# ============================================================================
# Integration Tests
# ============================================================================
Write-Host "`n[Integration Tests]" -ForegroundColor Yellow

Test-Feature "Header included in CLI handler" -Category "Integration" -Test {
    $cliHandler = Get-Content "D:\RawrXD-production-lazy-init\src\cli_command_handler.cpp" -Raw -ErrorAction SilentlyContinue
    return ($cliHandler -match '#include\s+"auto_model_loader\.h"')
}

Test-Feature "Header included in Qt IDE" -Category "Integration" -Test {
    $qtIDE = Get-Content "D:\RawrXD-production-lazy-init\src\qtapp\MainWindow_v5.cpp" -Raw -ErrorAction SilentlyContinue
    return ($qtIDE -match '#include\s+"auto_model_loader\.h"')
}

Test-Feature "CMakeLists includes implementation" -Category "Integration" -Test {
    $cmake = Get-Content "D:\RawrXD-production-lazy-init\CMakeLists.txt" -Raw -ErrorAction SilentlyContinue
    return ($cmake -match 'auto_model_loader\.cpp')
}

# ============================================================================
# Feature Validation Tests
# ============================================================================
Write-Host "`n[Feature Validation Tests]" -ForegroundColor Yellow

Test-Feature "Logging system defined" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "enum class LogLevel")
}

Test-Feature "Circuit breaker defined" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "class CircuitBreaker")
}

Test-Feature "Metrics collection defined" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "struct PerformanceMetrics")
}

Test-Feature "Async loading supported" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "loadModelAsync")
}

Test-Feature "Health checks supported" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "performHealthCheck")
}

Test-Feature "Model validation supported" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "validateModel")
}

Test-Feature "SHA256 hashing supported" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "computeModelHash")
}

Test-Feature "Prometheus metrics supported" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "generatePrometheusMetrics")
}

Test-Feature "Cache management supported" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "clearCache")
}

Test-Feature "Model preloading supported" -Category "Features" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "preloadModels")
}

# ============================================================================
# Thread Safety Tests
# ============================================================================
Write-Host "`n[Thread Safety Tests]" -ForegroundColor Yellow

Test-Feature "Singleton mutex defined" -Category "Thread Safety" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "std::mutex.*s_singletonMutex")
}

Test-Feature "Instance mutex defined" -Category "Thread Safety" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "std::mutex.*m_instanceMutex")
}

Test-Feature "Cache mutex defined" -Category "Thread Safety" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "std::mutex.*m_cacheMutex")
}

Test-Feature "Atomic operations used" -Category "Thread Safety" -Test {
    $header = Get-Content "D:\RawrXD-production-lazy-init\include\auto_model_loader.h" -Raw
    return ($header -match "std::atomic")
}

# ============================================================================
# Documentation Tests
# ============================================================================
Write-Host "`n[Documentation Tests]" -ForegroundColor Yellow

Test-Feature "Documentation has overview" -Category "Documentation" -Test {
    $docs = Get-Content "D:\RawrXD-production-lazy-init\docs\AUTO_MODEL_LOADER_ENTERPRISE.md" -Raw
    return ($docs -match "## Overview")
}

Test-Feature "Documentation has architecture" -Category "Documentation" -Test {
    $docs = Get-Content "D:\RawrXD-production-lazy-init\docs\AUTO_MODEL_LOADER_ENTERPRISE.md" -Raw
    return ($docs -match "## Architecture")
}

Test-Feature "Documentation has usage examples" -Category "Documentation" -Test {
    $docs = Get-Content "D:\RawrXD-production-lazy-init\docs\AUTO_MODEL_LOADER_ENTERPRISE.md" -Raw
    return ($docs -match "## Usage")
}

Test-Feature "Documentation has troubleshooting" -Category "Documentation" -Test {
    $docs = Get-Content "D:\RawrXD-production-lazy-init\docs\AUTO_MODEL_LOADER_ENTERPRISE.md" -Raw
    return ($docs -match "## Troubleshooting")
}

Test-Feature "Documentation has deployment guide" -Category "Documentation" -Test {
    $docs = Get-Content "D:\RawrXD-production-lazy-init\docs\AUTO_MODEL_LOADER_ENTERPRISE.md" -Raw
    return ($docs -match "## Deployment")
}

# ============================================================================
# Build System Tests
# ============================================================================
Write-Host "`n[Build System Tests]" -ForegroundColor Yellow

Test-Feature "Build directory exists" -Category "Build" -Test {
    Test-Path "D:\RawrXD-production-lazy-init\build"
}

Test-Feature "CMakeLists.txt exists" -Category "Build" -Test {
    Test-Path "D:\RawrXD-production-lazy-init\CMakeLists.txt"
}

Test-Feature "CMake minimum version specified" -Category "Build" -Test {
    $cmake = Get-Content "D:\RawrXD-production-lazy-init\CMakeLists.txt" -Raw
    return ($cmake -match "cmake_minimum_required")
}

# ============================================================================
# Test Results Summary
# ============================================================================
Write-Host "`n╔════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║            Test Results Summary                ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$totalTests = $testsPassed + $testsFailed
$successRate = [math]::Round(($testsPassed / $totalTests) * 100, 2)

Write-Host "Total Tests: $totalTests" -ForegroundColor White
Write-Host "Passed: $testsPassed" -ForegroundColor Green
Write-Host "Failed: $testsFailed" -ForegroundColor $(if ($testsFailed -eq 0) { "Green" } else { "Red" })
Write-Host "Success Rate: $successRate%" -ForegroundColor $(if ($successRate -ge 95) { "Green" } elseif ($successRate -ge 80) { "Yellow" } else { "Red" })
Write-Host ""

# Group results by category
Write-Host "Results by Category:" -ForegroundColor Yellow
$testResults | Group-Object Category | ForEach-Object {
    $passed = ($_.Group | Where-Object Status -eq "PASSED").Count
    $total = $_.Count
    $rate = [math]::Round(($passed / $total) * 100, 0)
    
    $color = if ($rate -eq 100) { "Green" } elseif ($rate -ge 80) { "Yellow" } else { "Red" }
    Write-Host "  $($_.Name): $passed/$total ($rate%)" -ForegroundColor $color
}

# Show failed tests if any
if ($testsFailed -gt 0) {
    Write-Host "`nFailed Tests:" -ForegroundColor Red
    $testResults | Where-Object Status -eq "FAILED" | ForEach-Object {
        Write-Host "  ✗ $($_.Category): $($_.Test)" -ForegroundColor Red
        if ($_.Message) {
            Write-Host "    Error: $($_.Message)" -ForegroundColor DarkGray
        }
    }
}

Write-Host ""

# Export results to JSON
$resultsFile = "D:\RawrXD-production-lazy-init\test_results.json"
$testResults | ConvertTo-Json -Depth 10 | Out-File $resultsFile -Encoding utf8
Write-Host "Detailed results exported to: $resultsFile" -ForegroundColor Gray

# Exit with appropriate code
if ($testsFailed -eq 0) {
    Write-Host "✓ All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "✗ Some tests failed. Please review and fix." -ForegroundColor Red
    exit 1
}
