# Full Pipeline Orchestrator
# 1. Smoke test all 1610 files (JSON output)
# 2. Spawn subagents to fix failures in parallel
# 3. Iterate until all files compile or manual fixes needed

param(
    [int]$MaxSubagents = 8,
    [int]$MaxIterations = 5
)

$ErrorActionPreference = "Continue"

$iteration = 0
$totalStarted = 0
$totalFixed = 0

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD Compilation Pipeline - Full End-to-End Orchestration ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

while ($iteration -lt $MaxIterations) {
    $iteration++
    Write-Host "╭─ ITERATION $iteration ─────────────────────────────────────────────────╮" -ForegroundColor Magenta
    
    # Step 1: Run smoke test
    Write-Host "`n[1/3] Running smoke test..." -ForegroundColor Cyan
    & "d:\rawrxd\smoke_test_json_output.ps1" -OutputFile "d:\rawrxd\compilation_results.json"
    
    $results = Get-Content "d:\rawrxd\compilation_results.json" -Raw | ConvertFrom-Json
    $failures = @($results.files | Where-Object { $_.compilation_status -eq "FAIL" })
    
    Write-Host "`nSmoke Test Results:" -ForegroundColor White
    Write-Host "  Passed: $($results.passed)/$($results.total_files)" -ForegroundColor Green
    Write-Host "  Failed: $($results.failed)/$($results.total_files)" -ForegroundColor Red
    Write-Host "  Success: $(($results.passed / $results.total_files * 100).ToString('F1'))%" -ForegroundColor Yellow
    
    if ($results.failed -eq 0) {
        Write-Host "`n✓ ALL 1610 FILES COMPILE SUCCESSFULLY!" -ForegroundColor Green
        Write-Host "╰────────────────────────────────────────────────────────────────────╯" -ForegroundColor Green
        exit 0
    }
    
    # Step 2: Spawn subagents to fix failures
    Write-Host "`n[2/3] Spawning $MaxSubagents subagents to fix $($results.failed) failures..." -ForegroundColor Cyan
    
    $jobs = @()
    for ($i = 1; $i -le $MaxSubagents; $i++) {
        $job = Start-Job -ScriptBlock {
            param($id, $max)
            & "d:\rawrxd\subagent_fixer.ps1" -SubagentId $id -MaxSubagents $max
        } -ArgumentList $i, $MaxSubagents -Name "subagent_$i"
        
        $jobs += $job
        Write-Host "  ✓ Subagent $i spawned" -ForegroundColor Magenta
    }
    
    # Step 3: Wait for subagents to complete
    Write-Host "`n[3/3] Waiting for subagents to complete..." -ForegroundColor Cyan
    $jobs | Wait-Job | Out-Null
    
    # Collect results
    $iterationFixed = 0
    foreach ($job in $jobs) {
        $output = Receive-Job -Job $job
        Remove-Job -Job $job
    }
    
    # Check for unfixed files
    $unfixedJsons = @(Get-ChildItem "d:\rawrxd\unfixed_subagent_*.json" -ErrorAction SilentlyContinue)
    
    if ($unfixedJsons.Count -eq 0) {
        Write-Host "`n✓ All failures fixed in this iteration!" -ForegroundColor Green
    } else {
        $totalUnfixed = 0
        foreach ($file in $unfixedJsons) {
            $unfixed = Get-Content $file.FullName -Raw | ConvertFrom-Json
            $totalUnfixed += $unfixed.unfixed_count
            Remove-Item $file.FullName -Force
        }
        Write-Host "`n  Still failing: $totalUnfixed files (need manual review)" -ForegroundColor Yellow
    }
    
    Write-Host "╰────────────────────────────────────────────────────────────────────╯`n" -ForegroundColor Cyan
}

Write-Host "═════════════════════════════════════════════════════════════════════════" -ForegroundColor Red
Write-Host "Maximum iterations ($MaxIterations) reached. Remaining failures require" -ForegroundColor Red
Write-Host "manual engineering review of the compilation errors." -ForegroundColor Red
Write-Host "═════════════════════════════════════════════════════════════════════════`n" -ForegroundColor Red

# Final report
$finalResults = Get-Content "d:\rawrxd\compilation_results.json" -Raw | ConvertFrom-Json
$finalFailures = @($finalResults.files | Where-Object { $_.compilation_status -eq "FAIL" })

Write-Host "FINAL STATUS:" -ForegroundColor Yellow
Write-Host "  Compiled: $($finalResults.passed)/$($finalResults.total_files)" -ForegroundColor Green
Write-Host "  Failed: $($finalFailures.Count)/$($finalResults.total_files)" -ForegroundColor Red

Write-Host "`nRemaining failures (detailed review required):" -ForegroundColor Red
$finalFailures | Select-Object -First 20 | ForEach-Object {
    Write-Host "`n  File: $([System.IO.Path]::GetFileName($_.file_name))" -ForegroundColor White
    Write-Host "  Error: $($_.first_error)" -ForegroundColor Gray
}

if ($finalFailures.Count -gt 20) {
    Write-Host "`n  ... and $(($finalFailures.Count) - 20) more failures" -ForegroundColor Gray
}

exit 1
