# Titan 70B Stress Test Runner
# Validates GPU batching, lock-free coordinator, zone fallback, KV flushing

param(
    [int]$Turns = 100,
    [int]$Warmup = 10,
    [switch]$Verbose,
    [string]$OutputPath = "titan_70b_stress_report.md"
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    TITAN 70B STRESS TEST RUNNER                           ║" -ForegroundColor Cyan
Write-Host "║         GPU Batching | Lock-Free Coord | Zone Fallback | KV Flush         ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check if executable exists
$exePath = "..\build\bin\titan_70b_stress_test.exe"
if (-not (Test-Path $exePath)) {
    Write-Host "[Build] Titan stress test executable not found. Building..." -ForegroundColor Yellow
    
    # Try to build
    if (Test-Path "..\build") {
        cmake --build ..\build --config Release --target titan_70b_stress_test 2>&1 | Out-Null
    }
    
    if (-not (Test-Path $exePath)) {
        Write-Host "[Error] Failed to build titan_70b_stress_test.exe" -ForegroundColor Red
        exit 1
    }
}

# Run the test
Write-Host "[Run] Starting Titan 70B stress test..." -ForegroundColor Green
Write-Host "      Turns: $Turns | Warmup: $Warmup" -ForegroundColor Gray
Write-Host ""

$env:RAWRXD_TITAN_TURNS = $Turns
$env:RAWRXD_TITAN_WARMUP = $Warmup
$env:RAWRXD_TITAN_VERBOSE = $Verbose
$env:RAWRXD_TITAN_OUTPUT = $OutputPath

$startTime = Get-Date
try {
    & $exePath 2>&1 | ForEach-Object {
        if ($_ -match "VIOLATED|FAIL|ERROR") {
            Write-Host $_ -ForegroundColor Red
        } elseif ($_ -match "PASSED|SUCCESS|✅") {
            Write-Host $_ -ForegroundColor Green
        } elseif ($_ -match "TPS:|GPU Batches:|Zone Fallbacks:") {
            Write-Host $_ -ForegroundColor Cyan
        } else {
            Write-Host $_
        }
    }
    $exitCode = $LASTEXITCODE
} catch {
    Write-Host "[Error] Test execution failed: $_" -ForegroundColor Red
    exit 1
}
$endTime = Get-Date
$duration = ($endTime - $startTime).TotalSeconds

Write-Host ""
Write-Host "Test completed in $([math]::Round($duration, 2)) seconds" -ForegroundColor Gray

# Check for report
if (Test-Path $OutputPath) {
    Write-Host "[Report] Generated: $OutputPath" -ForegroundColor Green
    
    # Show summary from report
    $report = Get-Content $OutputPath -Raw
    if ($report -match "Average TPS \| ([\d.]+) \| ([^|]+)") {
        $avgTps = $matches[1]
        $status = $matches[2].Trim()
        Write-Host "[Result] Average TPS: $avgTps | Status: $status" -ForegroundColor $(if ($status -match "PASS") { "Green" } else { "Yellow" })
    }
    
    if ($report -match "Contract Violations \| (\d+)/(\d+) \| ([^|]+)") {
        $violations = $matches[1]
        $total = $matches[2]
        $vstatus = $matches[3].Trim()
        Write-Host "[Result] Contract Violations: $violations/$total | Status: $vstatus" -ForegroundColor $(if ($vstatus -match "PASS") { "Green" } else { "Red" })
    }
} else {
    Write-Host "[Warning] Report file not generated" -ForegroundColor Yellow
}

exit $exitCode
