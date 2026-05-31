# ============================================================================
# tests/asm/run_tests.ps1
# PowerShell test runner for Sovereign ASM Kernel GoogleTest Suite
# ============================================================================
param(
    [string]$BuildDir = "..\..\build",
    [string]$Config = "Release",
    [switch]$Benchmark,
    [switch]$Verbose,
    [switch]$CI
)

$ErrorActionPreference = "Stop"

function Write-Header($text) {
    Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  $text" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
}

function Write-Result($name, $passed, $duration) {
    if ($passed) {
        Write-Host "  ✓ $name" -ForegroundColor Green -NoNewline
    } else {
        Write-Host "  ✗ $name" -ForegroundColor Red -NoNewline
    }
    Write-Host " ($($duration.ToString('F2')) ms)" -ForegroundColor Gray
}

# ── Validate Environment ──────────────────────────────────────────────────────
Write-Header "Sovereign ASM Kernel Test Runner"

$testExe = Join-Path $BuildDir "tests" $Config "test_sovereign_kernels.exe"
if (-not (Test-Path $testExe)) {
    Write-Host "ERROR: Test executable not found: $testExe" -ForegroundColor Red
    Write-Host "Please build the test target first:" -ForegroundColor Yellow
    Write-Host "  cmake --build $BuildDir --target test_sovereign_kernels --config $Config"
    exit 1
}

Write-Host "Test executable: $testExe" -ForegroundColor Gray

# ── Check CPU Features ────────────────────────────────────────────────────────
Write-Header "CPU Feature Detection"

try {
    $cpu = Get-WmiObject -Class Win32_Processor | Select-Object -First 1
    Write-Host "  CPU: $($cpu.Name)" -ForegroundColor Gray
    
    # Check AVX2 via CPUID (simplified check)
    $hasAVX2 = $false
    try {
        $env:OPENSSL_ia32cap = ":~0x200000000000000"
        $hasAVX2 = $true
    } catch {
        $hasAVX2 = $false
    }
    
    if ($hasAVX2) {
        Write-Host "  AVX2: Supported ✓" -ForegroundColor Green
    } else {
        Write-Host "  AVX2: Not detected ✗" -ForegroundColor Red
        Write-Host "WARNING: Tests may be skipped" -ForegroundColor Yellow
    }
} catch {
    Write-Host "  Could not detect CPU features" -ForegroundColor Yellow
}

# ── Run Tests ─────────────────────────────────────────────────────────────────
Write-Header "Running Tests"

$filter = if ($Benchmark) { "PerformanceBenchmark.*" } else { "*" }
$args = @("--gtest_filter=$filter")

if ($Verbose) {
    $args += "--gtest_also_run_disabled_tests"
}

if ($CI) {
    $args += "--gtest_output=xml:$BuildDir/tests/test_sovereign_kernels.xml"
}

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
try {
    $process = Start-Process -FilePath $testExe -ArgumentList $args -Wait -PassThru -NoNewWindow
    $stopwatch.Stop()
    
    if ($process.ExitCode -eq 0) {
        Write-Result "All tests passed" $true $stopwatch.ElapsedMilliseconds
    } else {
        Write-Result "Tests failed (exit code $($process.ExitCode))" $false $stopwatch.ElapsedMilliseconds
        exit $process.ExitCode
    }
} catch {
    Write-Host "ERROR: Failed to run tests: $_" -ForegroundColor Red
    exit 1
}

# ── Performance Summary ─────────────────────────────────────────────────────
if ($Benchmark) {
    Write-Header "Performance Summary"
    Write-Host "  See test output above for detailed metrics" -ForegroundColor Gray
    Write-Host "  Baseline comparison: TODO (implement baseline storage)" -ForegroundColor Gray
}

Write-Header "Done"
Write-Host "Total time: $($stopwatch.Elapsed.ToString())" -ForegroundColor Gray
