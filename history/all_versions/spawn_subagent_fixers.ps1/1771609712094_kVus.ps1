# Parallel Subagent Coordinator - Spawns multiple fixers in parallel
# Each subagent claims a batch of failed files and fixes them
param(
    [int]$MaxParallel = 8,
    [int]$FilesPerBatch = 50
)

$ErrorActionPreference = "Continue"

Write-Host "=== RawrXD Compilation Fixer - Parallel Subagent Orchestration ===" -ForegroundColor Cyan
Write-Host "Max Parallel Subagents: $MaxParallel" -ForegroundColor White
Write-Host "Files per Batch: $FilesPerBatch`n" -ForegroundColor White

# Step 1: Run orchestrator to identify failed files
Write-Host "[1/3] Identifying failed files..." -ForegroundColor Cyan
& "d:\rawrxd\compile_fix_orchestrator.ps1" -DryRun

if (-not (Test-Path "d:\rawrxd\failed_files.json")) {
    Write-Host "✓ No failed files - all 1610 modules compile successfully!" -ForegroundColor Green
    exit 0
}

$failedData = Get-Content "d:\rawrxd\failed_files.json" -Raw | ConvertFrom-Json
if ($failedData -isnot [array]) {
    $failedData = @($failedData)
}

$totalFailed = $failedData.Count
$totalBatches = [Math]::Ceiling($totalFailed / $FilesPerBatch)

Write-Host "`n[2/3] Spawning subagents..." -ForegroundColor Cyan
Write-Host "Total failed files: $totalFailed" -ForegroundColor Yellow
Write-Host "Batches needed: $totalBatches" -ForegroundColor Yellow
Write-Host "Processing with $MaxParallel parallel subagents`n" -ForegroundColor White

# Step 2: Launch subagents in parallel batches
$jobs = @()
for ($batch = 0; $batch -lt $totalBatches; $batch += $MaxParallel) {
    for ($i = 0; $i -lt $MaxParallel -and ($batch + $i) -lt $totalBatches; $i++) {
        $batchNum = $batch + $i
        $batchStart = $batchNum * $FilesPerBatch
        
        $jobName = "subagent_batch_$batchNum"
        Write-Host "  Spawning Subagent #$($batch + $i + 1): Batch $batchNum (files $batchStart-$($batchStart + $FilesPerBatch - 1))" -ForegroundColor Magenta
        
        $job = Start-Job -ScriptBlock {
            param($batchStart, $batchSize)
            & "d:\rawrxd\subagent_compile_fixer.ps1" -BatchStart $batchStart -BatchSize $batchSize
        } -ArgumentList $batchStart, $FilesPerBatch -Name $jobName
        
        $jobs += $job
    }
    
    # Wait for batch to complete before spawning next batch
    $jobs | Wait-Job | Out-Null
    $completedCount = ($jobs | Where-Object { $_.State -eq "Completed" }).Count
    Write-Host "  [$completedCount/$($jobs.Count)] subagents completed`n" -ForegroundColor Gray
}

Write-Host "`n[3/3] Collecting results..." -ForegroundColor Cyan

# Collect all results
$totalFixed = 0
$totalStillFailed = 0
$allStillFailed = @()

foreach ($job in $jobs) {
    $output = Receive-Job -Job $job
    foreach ($line in $output) {
        if ($line -like "*fixed*") {
            # Parse "X fixed, Y still failing"
            if ($line -match "(\d+) fixed.*(\d+) still failing") {
                $totalFixed += [int]$matches[1]
                $totalStillFailed += [int]$matches[2]
            }
        }
    }
    Remove-Job -Job $job
}

# Collect remaining failed files from batch outputs
$batchFailures = @(Get-ChildItem "d:\rawrxd\batch_*_failed.json" -ErrorAction SilentlyContinue)
foreach ($failFile in $batchFailures) {
    $batchFailed = Get-Content $failFile.FullName -Raw | ConvertFrom-Json
    if ($batchFailed -isnot [array]) { $batchFailed = @($batchFailed) }
    $allStillFailed += $batchFailed
}

# Final summary
Write-Host "`n`n=== FINAL RESULTS ===" -ForegroundColor Cyan
Write-Host "Total Failed (start): $totalFailed" -ForegroundColor Yellow
Write-Host "Total Fixed: $totalFixed" -ForegroundColor Green
Write-Host "Still Failing: $totalStillFailed" -ForegroundColor Red
Write-Host "Success Rate: $(if ($totalFailed -gt 0) { ($totalFixed / $totalFailed * 100).ToString('F1') } else { '100' })%" -ForegroundColor Yellow

if ($totalStillFailed -eq 0) {
    Write-Host "`n✓ ALL 1610 FILES COMPILE SUCCESSFULLY!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`nRemaining failures require manual engineering review:" -ForegroundColor Red
    $allStillFailed | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}
