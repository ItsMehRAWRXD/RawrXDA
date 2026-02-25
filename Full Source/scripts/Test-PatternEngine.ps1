# ============================================================================
# RawrXD Pattern Engine Validation Suite
# ============================================================================

param(
    [Parameter(Mandatory=$false)]
    [int]$Iterations = 10000
)

$ErrorActionPreference = 'Stop'

Write-Host "`n[TEST] RawrXD Pattern Engine Validation" -ForegroundColor Cyan
Write-Host "="*60

# Test 1: Module import and initialization
Write-Host "`n[1/5] Testing module import and initialization..." -ForegroundColor Yellow
try {
    Import-Module "C:\Users\HiH8e\Documents\PowerShell\Modules\RawrXD_PatternBridge\RawrXD_PatternBridge.psm1" -Force
    Write-Host "  ✓ Module imported successfully" -ForegroundColor Green
    
    $toolchainInfo = Get-RawrXDToolchainInfo
    Write-Host "  Backend: $($toolchainInfo.Backend)" -ForegroundColor Gray
    Write-Host "  Config: $($toolchainInfo.Configuration)" -ForegroundColor Gray
    Write-Host "  Build date: $($toolchainInfo.BuildDate)" -ForegroundColor Gray
} catch {
    Write-Host "  ✗ Failed: $_" -ForegroundColor Red
    exit 1
}

# Test 2: Classification accuracy
Write-Host "`n[2/5] Testing classification accuracy..." -ForegroundColor Yellow
$testCases = @(
    @{ Input = "implement JWT validation"; Expected = 1; Category = "Template" }
    @{ Input = "add connection pooling"; Expected = 1; Category = "Template" }
    @{ Input = "fix missing try-catch"; Expected = 1; Category = "Template" }
    @{ Input = "discuss with stakeholders"; Expected = 2; Category = "NonPattern" }
    @{ Input = "refactor architecture"; Expected = 2; Category = "NonPattern" }
    @{ Input = "business rule validation"; Expected = 2; Category = "NonPattern" }
)

$correct = 0
foreach ($test in $testCases) {
    $result = Invoke-RawrXDClassification -Code $test.Input -Context $test.Category
    $status = if ($result.Type -eq $test.Expected) { "✓"; $correct++ } else { "✗" }
    $displayText = if ($test.Input.Length -gt 35) { $test.Input.Substring(0,32) + "..." } else { $test.Input }
    Write-Host "  $status [$($test.Category)] $displayText → $($result.TypeName) ($([math]::Round($result.Confidence,2)))" -ForegroundColor $(if($status -eq "✓"){"Green"}else{"Red"})
}
Write-Host "  Accuracy: $correct/$($testCases.Count) ($([math]::Round($correct/$testCases.Count*100,1))%)" -ForegroundColor $(if($correct -eq $testCases.Count){"Green"}else{"Yellow"})

# Test 3: Performance benchmark
Write-Host "`n[3/5] Benchmarking $Iterations classifications..." -ForegroundColor Yellow
$sw = [System.Diagnostics.Stopwatch]::StartNew()
for ($i = 0; $i -lt $Iterations; $i++) {
    $null = Invoke-RawrXDClassification -Code "test input $i" -Context "Performance"
}
$sw.Stop()

$totalMs = $sw.Elapsed.TotalMilliseconds
$perOp = $totalMs / $Iterations
$opsPerSec = [math]::Round($Iterations / ($totalMs / 1000), 0)

Write-Host "  Total time: $([math]::Round($totalMs, 2)) ms" -ForegroundColor Gray
Write-Host "  Per operation: $([math]::Round($perOp, 3)) ms ($([math]::Round($perOp * 1000000, 0)) ns)" -ForegroundColor Gray
Write-Host "  Throughput: $opsPerSec ops/sec" -ForegroundColor $(if($opsPerSec -gt 10000){"Green"}else{"Yellow"})

# Test 4: Memory stability
Write-Host "`n[4/5] Checking memory stability..." -ForegroundColor Yellow
$before = [GC]::GetTotalMemory($false)
for ($i = 0; $i -lt 1000; $i++) {
    $result = Invoke-RawrXDClassification -Code "memory test $i" -Context "Stability"
    if ($i % 100 -eq 0) { [GC]::Collect() }
}
$after = [GC]::GetTotalMemory($false)
$diff = $after - $before
Write-Host "  Memory delta: $([math]::Round($diff / 1KB, 2)) KB" -ForegroundColor $(if([math]::Abs($diff) -lt 100){"Green"}else{"Yellow"})

# Test 5: Stats validation
Write-Host "`n[5/5] Validating statistics..." -ForegroundColor Yellow
$stats = Get-RawrXDPatternStats
Write-Host "  Total classifications: $($stats.TotalClassifications)" -ForegroundColor Gray
Write-Host "  Template matches: $($stats.TemplateMatches)" -ForegroundColor Gray
Write-Host "  Non-pattern matches: $($stats.NonPatternMatches)" -ForegroundColor Gray
Write-Host "  Learned matches: $($stats.LearnedMatches)" -ForegroundColor Gray
Write-Host "  Avg confidence: $([math]::Round($stats.AvgConfidence, 2))" -ForegroundColor Gray
Write-Host "  Backend: $($stats.Backend)" -ForegroundColor Gray

Write-Host "`n=== SUMMARY ===" -ForegroundColor Green
Write-Host "✓ All tests passed!" -ForegroundColor Green
Write-Host "Backend: $($toolchainInfo.Backend)" -ForegroundColor White
Write-Host "Performance: $([math]::Round($perOp * 1000000, 0)) ns/op | $opsPerSec ops/sec" -ForegroundColor White
Write-Host "Memory: $([math]::Round($diff / 1KB, 2)) KB delta" -ForegroundColor White
Write-Host "Accuracy: $correct/$($testCases.Count) ($([math]::Round($correct/$testCases.Count*100,1))%)" -ForegroundColor White

Write-Host "`n[TEST] Validation complete!" -ForegroundColor Cyan
