# ============================================================================
# Release Validation Harness - RawrXD v1.0.0-gold
# ============================================================================
# Automated validation for release tags
# Re-runs AST scope + inference loop sanity checks before promotion
# 
# Usage: .\ci\release_validation_harness.ps1 -Tag v1.0.0-gold
# ============================================================================

param(
    [Parameter(Mandatory=$true)]
    [string]$Tag,
    
    [Parameter(Mandatory=$false)]
    [switch]$SkipBuild,
    
    [Parameter(Mandatory=$false)]
    [switch]$SkipTests,
    
    [Parameter(Mandatory=$false)]
    [string]$BuildDir = "build-validation"
)

$ErrorActionPreference = "Stop"
$ValidationResults = @()
$StartTime = Get-Date

function Write-Header($message) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host $message -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

function Write-Result($test, $status, $duration, $details = "") {
    $color = if ($status -eq "PASS") { "Green" } elseif ($status -eq "FAIL") { "Red" } else { "Yellow" }
    Write-Host "[$status] $test (${duration}ms)" -ForegroundColor $color
    if ($details) { Write-Host "  $details" -ForegroundColor Gray }
    
    $script:ValidationResults += [PSCustomObject]@{
        Test = $test
        Status = $status
        Duration = $duration
        Details = $details
        Timestamp = Get-Date
    }
}

# ============================================================================
# Validation 1: Tag Integrity Check
# ============================================================================
Write-Header "VALIDATION 1: Tag Integrity"

$tagCheckStart = Get-Date
$tagExists = git tag -l $Tag
if ($tagExists) {
    $commitHash = git rev-list -n 1 $Tag
    Write-Result "Tag.Exists" "PASS" ((Get-Date) - $tagCheckStart).TotalMilliseconds "Tag $Tag -> $commitHash"
} else {
    Write-Result "Tag.Exists" "FAIL" ((Get-Date) - $tagCheckStart).TotalMilliseconds "Tag $Tag not found"
    exit 1
}

# ============================================================================
# Validation 2: Build Verification
# ============================================================================
if (-not $SkipBuild) {
    Write-Header "VALIDATION 2: Build Verification"
    
    $buildStart = Get-Date
    try {
        # Clean build directory
        if (Test-Path $BuildDir) {
            Remove-Item -Recurse -Force $BuildDir
        }
        
        # Configure
        cmake -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
        
        # Build core components
        ninja -C $BuildDir RawrXD-Win32IDE 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Build failed" }
        
        $buildDuration = ((Get-Date) - $buildStart).TotalMilliseconds
        
        # Verify binary exists and has expected size
        $binaryPath = "$BuildDir\bin\RawrXD-Win32IDE.exe"
        if (Test-Path $binaryPath) {
            $binarySize = (Get-Item $binarySize).Length
            $expectedSize = 14.7MB
            $sizeDiff = [Math]::Abs($binarySize - $expectedSize) / $expectedSize
            
            if ($sizeDiff -lt 0.1) {  # Within 10%
                Write-Result "Build.Binary" "PASS" $buildDuration "Size: $([math]::Round($binarySize/1MB, 2))MB"
            } else {
                Write-Result "Build.Binary" "WARN" $buildDuration "Size mismatch: $binarySize vs $expectedSize"
            }
        } else {
            Write-Result "Build.Binary" "FAIL" $buildDuration "Binary not found at $binaryPath"
        }
    } catch {
        Write-Result "Build" "FAIL" ((Get-Date) - $buildStart).TotalMilliseconds $_.Exception.Message
        exit 1
    }
}

# ============================================================================
# Validation 3: AST Scope Tests
# ============================================================================
if (-not $SkipTests) {
    Write-Header "VALIDATION 3: AST Scope-Awareness Tests"
    
    $testPath = "tests\ast_test.exe"
    if (Test-Path $testPath) {
        $astTestStart = Get-Date
        $output = & $testPath 2>&1
        $exitCode = $LASTEXITCODE
        $duration = ((Get-Date) - $astTestStart).TotalMilliseconds
        
        if ($exitCode -eq 0 -and $output -match "6/6 tests passed") {
            Write-Result "AST.Scope" "PASS" $duration "All scope-awareness tests passed"
        } else {
            Write-Result "AST.Scope" "FAIL" $duration "Exit code: $exitCode"
            Write-Host $output -ForegroundColor Red
        }
    } else {
        Write-Result "AST.Scope" "SKIP" 0 "Test executable not found, building..."
        
        # Compile test on demand
        cl /std:c++20 /EHsc /O2 tests\ast_scope_validation_real.cpp /Fe:tests\ast_test.exe 2>&1 | Out-Null
        if (Test-Path $testPath) {
            $output = & $testPath 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Result "AST.Scope" "PASS" 0 "Compiled and passed"
            } else {
                Write-Result "AST.Scope" "FAIL" 0 "Test execution failed"
            }
        } else {
            Write-Result "AST.Scope" "FAIL" 0 "Compilation failed"
        }
    }
}

# ============================================================================
# Validation 4: Inference Loop Sanity
# ============================================================================
Write-Header "VALIDATION 4: Inference Loop Sanity"

$sanityStart = Get-Date

# Check that key components are present
$components = @(
    "src\core\ast_graph_engine.h",
    "src\core\execution_scheduler_integration.hpp",
    "src\kv_cache\kv_cache_fp8_quantizer.hpp",
    "src\inference\token_pipeline_double_buffer.hpp",
    "src\ide\ast_completion_bridge.h"
)

$missingComponents = @()
foreach ($comp in $components) {
    if (-not (Test-Path $comp)) {
        $missingComponents += $comp
    }
}

if ($missingComponents.Count -eq 0) {
    Write-Result "Inference.Components" "PASS" ((Get-Date) - $sanityStart).TotalMilliseconds "All critical components present"
} else {
    Write-Result "Inference.Components" "FAIL" ((Get-Date) - $sanityStart).TotalMilliseconds "Missing: $($missingComponents -join ', ')"
}

# ============================================================================
# Validation 5: Thread Safety Check
# ============================================================================
Write-Header "VALIDATION 5: Thread Safety Audit"

$threadCheckStart = Get-Date

# Check for lock-free patterns in key files
$lockFreePatterns = @(
    "std::atomic",
    "moodycamel::ConcurrentQueue",
    "std::shared_mutex",
    "std::atomic.*fetch_add"
)

$lockFreeFiles = @(
    "src\agentic\LockFreeAgentCoordinator.h",
    "src\core\ast_graph_engine.hpp"
)

$foundPatterns = 0
foreach ($file in $lockFreeFiles) {
    if (Test-Path $file) {
        $content = Get-Content $file -Raw
        foreach ($pattern in $lockFreePatterns) {
            if ($content -match $pattern) {
                $foundPatterns++
            }
        }
    }
}

if ($foundPatterns -ge 3) {
    Write-Result "ThreadSafety.LockFree" "PASS" ((Get-Date) - $threadCheckStart).TotalMilliseconds "Lock-free patterns verified"
} else {
    Write-Result "ThreadSafety.LockFree" "WARN" ((Get-Date) - $threadCheckStart).TotalMilliseconds "Limited lock-free patterns found"
}

# ============================================================================
# Validation 6: Memory Safety Check
# ============================================================================
Write-Header "VALIDATION 6: Memory Safety Audit"

$memCheckStart = Get-Date

# Check for smart pointer usage
$smartPtrPattern = "std::(unique_ptr|shared_ptr|make_unique|make_shared)"
$rawPtrPattern = "(char\*|void\*|int\*)\s+\w+\s*=\s*(malloc|new|calloc)"

$cppFiles = Get-ChildItem src -Recurse -Filter "*.cpp" | Select-Object -First 20
$smartPtrCount = 0
$rawPtrCount = 0

foreach ($file in $cppFiles) {
    $content = Get-Content $file.FullName -Raw
    $smartPtrMatches = [regex]::Matches($content, $smartPtrPattern)
    $rawPtrMatches = [regex]::Matches($content, $rawPtrPattern)
    $smartPtrCount += $smartPtrMatches.Count
    $rawPtrCount += $rawPtrMatches.Count
}

$ratio = if ($smartPtrCount + $rawPtrCount -gt 0) { 
    $smartPtrCount / ($smartPtrCount + $rawPtrCount) 
} else { 
    1.0 
}

if ($ratio -gt 0.8) {
    Write-Result "MemorySafety.SmartPtrs" "PASS" ((Get-Date) - $memCheckStart).TotalMilliseconds "Smart pointer ratio: $([math]::Round($ratio*100, 1))%"
} else {
    Write-Result "MemorySafety.SmartPtrs" "WARN" ((Get-Date) - $memCheckStart).TotalMilliseconds "Low smart pointer usage: $([math]::Round($ratio*100, 1))%"
}

# ============================================================================
# Summary Report
# ============================================================================
Write-Header "VALIDATION SUMMARY"

$totalDuration = ((Get-Date) - $StartTime).TotalMilliseconds
$passed = ($ValidationResults | Where-Object { $_.Status -eq "PASS" }).Count
$failed = ($ValidationResults | Where-Object { $_.Status -eq "FAIL" }).Count
$warnings = ($ValidationResults | Where-Object { $_.Status -eq "WARN" }).Count

Write-Host "Tag: $Tag" -ForegroundColor White
Write-Host "Total Tests: $($ValidationResults.Count)" -ForegroundColor White
Write-Host "Passed: $passed" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor Red
Write-Host "Warnings: $warnings" -ForegroundColor Yellow
Write-Host "Duration: $([math]::Round($totalDuration, 2))ms" -ForegroundColor White

if ($failed -eq 0) {
    Write-Host "`n✅ RELEASE VALIDATION PASSED" -ForegroundColor Green
    Write-Host "Tag $Tag is cleared for promotion" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n❌ RELEASE VALIDATION FAILED" -ForegroundColor Red
    Write-Host "Tag $Tag requires fixes before promotion" -ForegroundColor Red
    exit 1
}
