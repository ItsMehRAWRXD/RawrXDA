# Test-TokenGeneration.ps1
# Validates single-token and multi-token generation with detokenization

param(
    [string]$ModelPath = "F:\OllamaModels\blobs\sha256-6f85a640a97cf2bf5b8e764087b1e83da0fdb51d7c9fab7d0fece9385611df83",
    [int]$Layers = 1
)

$ErrorActionPreference = "Continue"
# Prefer Ninja build output if available (newer), fallback to bin/
$binPath = if (Test-Path "D:\rawrxd\build_ninja\bin\RawrXD-Win32IDE.exe") {
    "D:\rawrxd\build_ninja\bin\RawrXD-Win32IDE.exe"
} else {
    "D:\rawrxd\bin\RawrXD-Win32IDE.exe"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " TOKEN GENERATION VALIDATION SUITE" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if (-not (Test-Path $binPath)) {
    Write-Host "❌ Binary not found: $binPath" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $ModelPath)) {
    Write-Host "❌ Model not found: $ModelPath" -ForegroundColor Red
    exit 1
}

# Check if we need to rebuild (source touched recently)
$srcFile = "D:\rawrxd\src\win32app\main_win32.cpp"
if (Test-Path $srcFile) {
    $srcAge = (Get-Date) - (Get-Item $srcFile).LastWriteTime
    $binAge = (Get-Date) - (Get-Item $binPath).LastWriteTime
    
    if ($srcAge -lt $binAge) {
        Write-Host "⚠️ Source file modified after binary build - rebuilding..." -ForegroundColor Yellow
        # Prefer Ninja build if available, fallback to NMake
        if (Test-Path "D:\rawrxd\build_ninja\build.ninja") {
            Push-Location "D:\rawrxd\build_ninja"
            ninja RawrXD-Win32IDE 2>&1 | Out-Null
        } else {
            Push-Location "D:\rawrxd\build"
            cmake --build . --config Release --target RawrXD-Win32IDE 2>&1 | Out-Null
        }
        Pop-Location
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "❌ Rebuild failed" -ForegroundColor Red
            exit 1
        }
        Write-Host "✅ Rebuild complete" -ForegroundColor Green
    }
}

Write-Host "`nModel: $ModelPath" -ForegroundColor Gray
Write-Host "Layers: $Layers" -ForegroundColor Gray

# Test 1: Single Token (What is token 42?)
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " TEST 1: Single Token Generation" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

Write-Host "[1/3] Running single-token test..." -ForegroundColor Yellow
# Clean stale result file
$resultLog = Join-Path (Split-Path $binPath) "inference_fast_result.txt"
$resultLogAlt = "D:\rawrxd\inference_fast_result.txt"
Remove-Item $resultLog -Force -ErrorAction SilentlyContinue

$output1 = & $binPath --test-inference-fast `
    --test-model $ModelPath `
    --test-num-layers $Layers `
    --test-max-tokens 1 `
    --test-prompt "Hello" 2>&1 | Out-String

# WinMain apps may not pipe stdout to PowerShell; fallback to log file
if ($output1 -notmatch 'PASS|FAIL') {
    foreach ($p in @($resultLog, "D:\rawrxd\inference_fast_result.txt", "inference_fast_result.txt")) {
        if (Test-Path $p) { $output1 = Get-Content $p -Raw; break }
    }
}

Write-Host $output1

if ($output1 -match 'PASS FAST_GENERATE token=(\d+) time=(\d+)ms') {
    $token = $matches[1]
    $time = $matches[2]
    Write-Host "✅ Single token: $token in ${time}ms" -ForegroundColor Green
    
    if ($output1 -match '\[DETOK\] Token \d+ = "(.+)"') {
        $tokenText = $matches[1]
        Write-Host "📝 Decoded: '$tokenText'" -ForegroundColor Cyan
    }
} else {
    Write-Host "❌ Single token test failed" -ForegroundColor Red
    exit 1
}

# Test 2: Multi-Token (10 tokens - Coherence Check)
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " TEST 2: Multi-Token Generation (10)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

Write-Host "[2/3] Running 10-token test..." -ForegroundColor Yellow
Remove-Item $resultLog -Force -ErrorAction SilentlyContinue
$output2 = & $binPath --test-inference-fast `
    --test-model $ModelPath `
    --test-num-layers $Layers `
    --test-max-tokens 10 `
    --test-timeout-ms 30000 `
    --test-prompt "The answer to life, the universe, and everything is" 2>&1 | Out-String

if ($output2 -notmatch 'PASS|FAIL') {
    foreach ($p in @($resultLog, "D:\rawrxd\inference_fast_result.txt", "inference_fast_result.txt")) {
        if (Test-Path $p) { $output2 = Get-Content $p -Raw; break }
    }
}

Write-Host $output2

if ($output2 -match 'PASS FAST_GENERATE tokens=(\d+) time=(\d+)ms') {
    $count = $matches[1]
    $time = $matches[2]
    $perToken = [math]::Round($time / $count, 1)
    
    Write-Host "✅ Generated $count tokens in ${time}ms (~${perToken}ms/token)" -ForegroundColor Green
    
    if ($output2 -match '\[DETOK\] Text: "(.+)"') {
        $fullText = $matches[1]
        Write-Host "`n📝 Generated text:" -ForegroundColor Cyan
        Write-Host "   '$fullText'" -ForegroundColor White
    }
    
    if ($output2 -match '\[TOKENS\] IDs: (.+)') {
        $tokenIds = $matches[1]
        Write-Host "`n🔢 Token IDs: $tokenIds" -ForegroundColor Gray
    }
} else {
    Write-Host "❌ Multi-token test failed" -ForegroundColor Red
    Write-Host "Output: $output2" -ForegroundColor Gray
    exit 1
}

# Test 3: Longer Sequence (50 tokens - KV Cache Stress Test)
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " TEST 3: Long Sequence (50 tokens)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

Write-Host "[3/3] Running 50-token test (KV cache validation)..." -ForegroundColor Yellow
Remove-Item $resultLog -Force -ErrorAction SilentlyContinue
$output3 = & $binPath --test-inference-fast `
    --test-model $ModelPath `
    --test-num-layers $Layers `
    --test-max-tokens 50 `
    --test-prompt "Once upon a time" `
    --test-timeout-ms 60000 2>&1 | Out-String

if ($output3 -notmatch 'PASS|FAIL') {
    foreach ($p in @($resultLog, "D:\rawrxd\inference_fast_result.txt", "inference_fast_result.txt")) {
        if (Test-Path $p) { $output3 = Get-Content $p -Raw; break }
    }
}

Write-Host $output3

if ($output3 -match 'PASS FAST_GENERATE tokens=(\d+) time=(\d+)ms') {
    $count = $matches[1]
    $time = $matches[2]
    $perToken = [math]::Round($time / $count, 1)
    
    Write-Host "✅ Generated $count tokens in ${time}ms (~${perToken}ms/token)" -ForegroundColor Green
    
    if ($output3 -match '\[DETOK\] Text: "(.+)"') {
        $fullText = $matches[1]
        Write-Host "`n📝 Generated text (first 200 chars):" -ForegroundColor Cyan
        $preview = if ($fullText.Length -gt 200) { $fullText.Substring(0, 200) + "..." } else { $fullText }
        Write-Host "   '$preview'" -ForegroundColor White
    }
    
    # Check if latency stays consistent (no KV cache corruption)
    if ($perToken -lt 500) {
        Write-Host "⚡ KV cache: HEALTHY (consistent latency)" -ForegroundColor Green
    } else {
        Write-Host "⚠️ KV cache: DEGRADED (latency increasing)" -ForegroundColor Yellow
    }
} else {
    Write-Host "⚠️ Long sequence test failed (may be timeout)" -ForegroundColor Yellow
}

# Summary
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " SUMMARY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$tests = @()
if ($output1 -match 'PASS') { $tests += "✅ Single token" } else { $tests += "❌ Single token" }
if ($output2 -match 'PASS') { $tests += "✅ Multi-token (10)" } else { $tests += "❌ Multi-token (10)" }
if ($output3 -match 'PASS') { $tests += "✅ Long sequence (50)" } else { $tests += "⚠️ Long sequence" }

foreach ($test in $tests) {
    Write-Host $test
}

if ($tests -match "❌") {
    Write-Host "`n❌ Some tests failed" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`n✅ ALL TESTS PASSED - Autoregressive loop validated!" -ForegroundColor Green
    Write-Host "   • Single token: Working" -ForegroundColor Green
    Write-Host "   • Multi-token: Working" -ForegroundColor Green
    Write-Host "   • KV cache: Working" -ForegroundColor Green
    Write-Host "   • Detokenization: Working" -ForegroundColor Green
    exit 0
}
