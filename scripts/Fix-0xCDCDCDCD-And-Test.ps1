# Fix-0xCDCDCDCD-And-Test.ps1
# Rebuild with corruption fixes and validate token generation

param(
    [string]$ModelPath = "F:\OllamaModels\blobs\sha256-e1eaa0f4fffb8880ca14c1c1f9e7d887fb45cc19a5b17f5cc83c3e8d3e85914e",
    [int]$TimeoutSec = 60
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Stage 1: Clean Rebuild" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$ErrorActionPreference = "Continue"
$buildDir = "D:\rawrxd\build"
$binPath = "D:\rawrxd\bin\RawrXD-Win32IDE.exe"

# Touch source files to force recompile
$touchFiles = @(
    "D:\rawrxd\src\cpu_inference_engine.cpp",
    "D:\rawrxd\src\win32app\main_win32.cpp"
)

foreach ($file in $touchFiles) {
    if (Test-Path $file) {
        (Get-Item $file).LastWriteTime = Get-Date
        Write-Host "[TOUCH] $file" -ForegroundColor Yellow
    }
}

# Clean stale locks
Remove-Item -Path "$buildDir\**\*.ilk" -Force -ErrorAction SilentlyContinue
Remove-Item -Path "$buildDir\**\*.pdb" -Force -ErrorAction SilentlyContinue

Write-Host "`n[BUILD] Starting CMake build..." -ForegroundColor Green
Push-Location $buildDir
try {
    $buildOutput = cmake --build . --config Release --target RawrXD-Win32IDE 2>&1
    $buildOutput | Out-File -FilePath "D:\rawrxd\build_fix_0xCD.log" -Encoding utf8
    Write-Host $buildOutput
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[BUILD] FAILED with exit code $LASTEXITCODE" -ForegroundColor Red
        Pop-Location
        exit 1
    }
} catch {
    Write-Host "[BUILD] Exception: $_" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location

Write-Host "[BUILD] SUCCESS" -ForegroundColor Green

# Verify binary timestamp
if (Test-Path $binPath) {
    $binAge = (Get-Date) - (Get-Item $binPath).LastWriteTime
    Write-Host "[BINARY] Age: $([math]::Round($binAge.TotalSeconds, 1)) seconds" -ForegroundColor Cyan
    if ($binAge.TotalMinutes -gt 5) {
        Write-Host "[BINARY] WARNING: Binary is stale (>5min old) - build may not have updated it!" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "[BINARY] NOT FOUND: $binPath" -ForegroundColor Red
    exit 1
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " Stage 2: Inference Validation Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if (-not (Test-Path $ModelPath)) {
    Write-Host "[MODEL] NOT FOUND: $ModelPath" -ForegroundColor Red
    Write-Host "[MODEL] Please provide valid model path with -ModelPath parameter" -ForegroundColor Yellow
    exit 1
}

Write-Host "[TEST] Model: $ModelPath" -ForegroundColor Cyan
Write-Host "[TEST] Timeout: ${TimeoutSec}s" -ForegroundColor Cyan
Write-Host "[TEST] Launching fresh PowerShell..." -ForegroundColor Green

$testScript = @"
`$ErrorActionPreference = 'Continue'
Set-Location 'D:\rawrxd'
Write-Host '[TEST] Running in fresh shell...' -ForegroundColor Green
& '.\bin\RawrXD-Win32IDE.exe' --test-inference-fast --test-model '$ModelPath' 2>&1
Write-Host '[TEST] Exit code: ' `$LASTEXITCODE -ForegroundColor Cyan
exit `$LASTEXITCODE
"@

$testScriptPath = [System.IO.Path]::GetTempFileName() + ".ps1"
$testScript | Out-File -FilePath $testScriptPath -Encoding utf8

try {
    $proc = Start-Process -FilePath "powershell.exe" `
                          -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $testScriptPath `
                          -NoNewWindow `
                          -PassThru `
                          -RedirectStandardOutput "D:\rawrxd\test_output_fix_0xCD.txt" `
                          -RedirectStandardError "D:\rawrxd\test_stderr_fix_0xCD.txt"
    
    $proc | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
    
    if ($proc.HasExited) {
        $exitCode = $proc.ExitCode
        Write-Host "`n[TEST] Completed with exit code: $exitCode" -ForegroundColor Cyan
    } else {
        Write-Host "`n[TEST] TIMEOUT after ${TimeoutSec}s - killing process" -ForegroundColor Red
        $proc | Stop-Process -Force
        $exitCode = 124
    }
} catch {
    Write-Host "[TEST] Exception: $_" -ForegroundColor Red
    $exitCode = 1
} finally {
    Remove-Item -Path $testScriptPath -Force -ErrorAction SilentlyContinue
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " Stage 3: Results Analysis" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$stdoutPath = "D:\rawrxd\test_output_fix_0xCD.txt"
$stderrPath = "D:\rawrxd\test_stderr_fix_0xCD.txt"

if (Test-Path $stdoutPath) {
    $stdout = Get-Content $stdoutPath -Raw
    Write-Host "`n--- STDOUT ---" -ForegroundColor Yellow
    Write-Host $stdout
    
    # Parse for key patterns
    if ($stdout -match 'PASS FAST_GENERATE token=(\d+) time=(\d+)ms') {
        Write-Host "`n[RESULT] ✅ SUCCESS - Token generated: $($Matches[1]), Time: $($Matches[2])ms" -ForegroundColor Green
        $exitCode = 0
    } elseif ($stdout -match 'FAIL FAST_INVALID_STATE bad_layers=(-?\d+)') {
        Write-Host "`n[RESULT] ❌ FAIL - Invalid layers: $($Matches[1]) (0xCDCDCDCD corruption)" -ForegroundColor Red
        Write-Host "[HINT] LoadModel is still seeing uninitialized metadata - check GGUF parser" -ForegroundColor Yellow
    } elseif ($stdout -match 'FAIL FAST_INVALID_STATE bad_embed=(-?\d+)') {
        Write-Host "`n[RESULT] ❌ FAIL - Invalid embed: $($Matches[1]) (0xCDCDCDCD corruption)" -ForegroundColor Red
        Write-Host "[HINT] LoadModel is still seeing uninitialized metadata - check GGUF parser" -ForegroundColor Yellow
    } elseif ($stdout -match 'FAIL FAST_EXCEPTION (.+)') {
        Write-Host "`n[RESULT] ❌ FAIL - Exception: $($Matches[1])" -ForegroundColor Red
        Write-Host "[HINT] Generation threw exception - check engine state validation" -ForegroundColor Yellow
    } elseif ($stdout -match 'FAIL FAST_TIMEOUT time=(\d+)ms') {
        Write-Host "`n[RESULT] ❌ FAIL - Timeout after $($Matches[1])ms" -ForegroundColor Red
        Write-Host "[HINT] Generate() is hanging - add layer instrumentation" -ForegroundColor Yellow
    } elseif ($stdout -match 'FAIL FAST_ENGINE_LOAD') {
        Write-Host "`n[RESULT] ❌ FAIL - LoadModel() returned false" -ForegroundColor Red
        Write-Host "[HINT] Check ENGINE ERROR/FATAL messages in stderr" -ForegroundColor Yellow
    } else {
        Write-Host "`n[RESULT] ⚠️ UNKNOWN - No recognizable pass/fail pattern" -ForegroundColor Magenta
    }
}

if (Test-Path $stderrPath) {
    $stderr = Get-Content $stderrPath -Raw
    Write-Host "`n--- STDERR (Engine Diagnostics) ---" -ForegroundColor Yellow
    Write-Host $stderr
    
    # Extract key diagnostic values
    if ($stderr -match 'Post-clamp: layers=(\d+).*embed=(\d+).*vocab=(\d+)') {
        Write-Host "`n[DIAGNOSTIC] Post-clamp values:" -ForegroundColor Cyan
        Write-Host "  Layers: $($Matches[1])" -ForegroundColor Cyan
        Write-Host "  Embed:  $($Matches[2])" -ForegroundColor Cyan
        Write-Host "  Vocab:  $($Matches[3])" -ForegroundColor Cyan
    }
    
    if ($stderr -match 'FATAL.*corrupted \(0xCDCDCDCD\)') {
        Write-Host "`n[DIAGNOSTIC] ⚠️ Corruption detected and forced to fallback values" -ForegroundColor Yellow
    }
    
    if ($stderr -match 'VALIDATION FAILED: (.+)') {
        Write-Host "`n[DIAGNOSTIC] ❌ Validation failed: $($Matches[1])" -ForegroundColor Red
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build log:  D:\rawrxd\build_fix_0xCD.log" -ForegroundColor Gray
Write-Host "Test stdout: $stdoutPath" -ForegroundColor Gray
Write-Host "Test stderr: $stderrPath" -ForegroundColor Gray
Write-Host "Exit code:  $exitCode" -ForegroundColor Gray

exit $exitCode
