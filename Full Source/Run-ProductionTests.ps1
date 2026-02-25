# RawrXD Production Test Runner
# Comprehensive test suite for CI/CD and production validation

param(
    [string]$BuildDir = "build_universal",
    [string]$Config = "Release",
    [switch]$Quick = $false,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "RawrXD Production Test Suite" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

# Test categories
$testCategories = @{
    "Critical" = @(
        "test_gguf_loader_simple",
        "test_gguf_loader", 
        "test_agentic_file_operations",
        "test_tool_registry",
        "quant_correctness_tests",
        "test_deflate_masm"
    )
    "Integration" = @(
        "test_agent_coordinator_integration",
        "test_chat_streaming",
        "test_kv_cache"
    )
    "Performance" = @(
        "quant_scalar_smoke",
        "quant_engine_test",
        "gpu_inference_benchmark"
    )
    "MASM" = @(
        "test_deflate_masm",
        "deflate_brutal_masm_lib"
    )
}

if ($Quick) {
    Write-Host "⚡ Quick mode: Running only critical tests" -ForegroundColor Yellow
    $testsToRun = $testCategories["Critical"]
} else {
    $testsToRun = $testCategories.Values | ForEach-Object { $_ } | Select-Object -Unique
}

$totalTests = $testsToRun.Count
$passedTests = 0
$failedTests = 0
$results = @()

Write-Host "Found $totalTests tests to execute" -ForegroundColor Green

# Run each test
foreach ($testName in $testsToRun) {
    Write-Host ""
    Write-Host "Running: $testName" -ForegroundColor Cyan
    
    $exePath = "$BuildDir\$Config\$testName.exe"
    $vcxprojPath = "$BuildDir\$testName.vcxproj"
    
    # Check if test exists
    if (!(Test-Path $exePath)) {
        Write-Host "  ⚠️  Test executable not found: $exePath" -ForegroundColor Yellow
        
        # Try to build it
        if (Test-Path $vcxprojPath) {
            Write-Host "  🔨 Attempting to build $testName..." -ForegroundColor Yellow
            try {
                msbuild $vcxprojPath /p:Configuration=$Config /p:Platform=x64 /v:minimal
                if (!(Test-Path $exePath)) {
                    Write-Host "  ❌ Build failed or executable still not found" -ForegroundColor Red
                    $failedTests++
                    $results += [PSCustomObject]@{Name = $testName; Status = "BUILD_FAILED"; Duration = 0}
                    continue
                }
            } catch {
                Write-Host "  ❌ Build error: $_" -ForegroundColor Red
                $failedTests++
                $results += [PSCustomObject]@{Name = $testName; Status = "BUILD_ERROR"; Duration = 0}
                continue
            }
        } else {
            Write-Host "  ❌ Test project not found: $vcxprojPath" -ForegroundColor Red
            $failedTests++
            $results += [PSCustomObject]@{Name = $testName; Status = "NOT_FOUND"; Duration = 0}
            continue
        }
    }
    
    # Run test with timeout
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $processInfo = New-Object System.Diagnostics.ProcessStartInfo
    $processInfo.FileName = $exePath
    $processInfo.RedirectStandardOutput = $true
    $processInfo.RedirectStandardError = $true
    $processInfo.UseShellExecute = $false
    $processInfo.WorkingDirectory = "$BuildDir\$Config"
    
    if ($Verbose) {
        $processInfo.Arguments = "--verbose"
    }
    
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $processInfo
    
    try {
        $process.Start() | Out-Null
        
        # Wait for completion with 5 minute timeout
        if (!$process.WaitForExit(300000)) {
            Write-Host "  ⚠️  Test timed out (5 minutes)" -ForegroundColor Yellow
            $process.Kill()
            $failedTests++
            $results += [PSCustomObject]@{Name = $testName; Status = "TIMEOUT"; Duration = $stopwatch.ElapsedMilliseconds}
            continue
        }
        
        $stopwatch.Stop()
        $exitCode = $process.ExitCode
        $output = $process.StandardOutput.ReadToEnd()
        $error = $process.StandardError.ReadToEnd()
        
        if ($exitCode -eq 0) {
            Write-Host "  ✅ PASSED in $($stopwatch.ElapsedMilliseconds)ms" -ForegroundColor Green
            $passedTests++
            $results += [PSCustomObject]@{Name = $testName; Status = "PASSED"; Duration = $stopwatch.ElapsedMilliseconds}
        } else {
            Write-Host "  ❌ FAILED (exit code: $exitCode) in $($stopwatch.ElapsedMilliseconds)ms" -ForegroundColor Red
            if ($Verbose -and $error) {
                Write-Host "  Error output: $error" -ForegroundColor Red
            }
            $failedTests++
            $results += [PSCustomObject]@{Name = $testName; Status = "FAILED"; Duration = $stopwatch.ElapsedMilliseconds}
        }
        
        if ($Verbose -and $output) {
            Write-Host "  Output: $output" -ForegroundColor Gray
        }
        
    } catch {
        Write-Host "  ❌ EXCEPTION: $_" -ForegroundColor Red
        $failedTests++
        $results += [PSCustomObject]@{Name = $testName; Status = "EXCEPTION"; Duration = $stopwatch.ElapsedMilliseconds}
    } finally {
        if ($process -and !$process.HasExited) {
            $process.Kill()
        }
    }
}

# Summary
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Test Summary" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Total:  $totalTests" -ForegroundColor White
Write-Host "Passed: $passedTests" -ForegroundColor Green
Write-Host "Failed: $failedTests" -ForegroundColor Red

# Show failed tests
if ($failedTests -gt 0) {
    Write-Host ""
    Write-Host "Failed Tests:" -ForegroundColor Red
    $results | Where-Object { $_.Status -ne "PASSED" } | Format-Table Name, Status, Duration -AutoSize
}

# Performance summary
Write-Host ""
Write-Host "Performance Summary:" -ForegroundColor Cyan
$avgDuration = ($results | Where-Object { $_.Status -eq "PASSED" } | Measure-Object -Property Duration -Average).Average
Write-Host "Average test duration: $([math]::Round($avgDuration, 2))ms" -ForegroundColor White

$totalDuration = ($results | Measure-Object -Property Duration -Sum).Sum
Write-Host "Total execution time: $([math]::Round($totalDuration / 1000, 2))s" -ForegroundColor White

# Exit with appropriate code
if ($failedTests -eq 0) {
    Write-Host ""
    Write-Host "✅ All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "❌ Some tests failed" -ForegroundColor Red
    exit 1
}
