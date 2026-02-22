# Parallel Subagent Coordinator - Spawns multiple fixers in parallel
# Each subagent claims a batch of failed files and fixes them
# Swarm mode: -MaxParallel 16 -FilesPerBatch 100 (~16 batches, 1600 files)
param(
    [string]$ManifestPath,
    [int]$MaxParallel = 16,
    [int]$FilesPerBatch = 100
)

$ErrorActionPreference = "Continue"

Write-Host "=== RawrXD Compilation Fixer - Parallel Subagent Orchestration ===" -ForegroundColor Cyan
Write-Host "Manifest Path: $ManifestPath" -ForegroundColor White
Write-Host "Max Parallel Subagents: $MaxParallel" -ForegroundColor White
Write-Host "Files per Batch: $FilesPerBatch`n" -ForegroundColor White

# Step 1: Load failed files from manifest
if ($ManifestPath) {
    if (-not (Test-Path $ManifestPath)) {
        Write-Host "Manifest file not found: $ManifestPath" -ForegroundColor Red
        exit 1
    }
    $failedData = Get-Content $ManifestPath -Raw | ConvertFrom-Json
} else {
    Write-Host "[1/3] Identifying failed files..." -ForegroundColor Cyan
    & "d:\rawrxd\compile_fix_orchestrator.ps1" -DryRun

    if (-not (Test-Path "d:\rawrxd\failed_files.json")) {
        Write-Host "✓ No failed files - all 1610 modules compile successfully!" -ForegroundColor Green
        exit 0
    }

    $failedData = Get-Content "d:\rawrxd\failed_files.json" -Raw | ConvertFrom-Json
}
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
$jobRecords = @()
$runToken = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$runOutDir = Join-Path "d:\rawrxd" ".swarm_runs\$runToken"
if (-not (Test-Path $runOutDir)) {
    New-Item -ItemType Directory -Path $runOutDir -Force | Out-Null
}

for ($batch = 0; $batch -lt $totalBatches; $batch += $MaxParallel) {
    $waveJobs = @()
    for ($i = 0; $i -lt $MaxParallel -and ($batch + $i) -lt $totalBatches; $i++) {
        $batchNum = $batch + $i
        $batchStart = $batchNum * $FilesPerBatch
        $batchEnd = [Math]::Min($batchStart + $FilesPerBatch - 1, [Math]::Max(0, $totalFailed - 1))

        $jobName = "subagent_batch_$batchNum"
        $batchOut = Join-Path $runOutDir "batch_${batchStart}_failed.json"
        Write-Host "  Spawning Subagent #$($batch + $i + 1): Batch $batchNum (files $batchStart-$batchEnd)" -ForegroundColor Magenta

        $job = Start-Job -ScriptBlock {
            param($batchStart, $batchSize, $manifestPath, $outputFailedJson)
            if ($manifestPath) {
                & "d:\rawrxd\subagent_compile_fixer.ps1" -FailedFilesJson $manifestPath -BatchStart $batchStart -BatchSize $batchSize -OutputFailedJson $outputFailedJson
            } else {
                & "d:\rawrxd\subagent_compile_fixer.ps1" -BatchStart $batchStart -BatchSize $batchSize -OutputFailedJson $outputFailedJson
            }
        } -ArgumentList $batchStart, $FilesPerBatch, $ManifestPath, $batchOut -Name $jobName

        $jobs += $job
        $waveJobs += $job
        $jobRecords += [pscustomobject]@{
            Job      = $job
            BatchNum = $batchNum
            BatchOut = $batchOut
        }
    }

    # Wait only current wave to complete before spawning next wave
    $waveJobs | Wait-Job | Out-Null
    $completedCount = ($jobs | Where-Object { $_.State -eq "Completed" }).Count
    Write-Host "  [$completedCount/$($jobs.Count)] subagents completed`n" -ForegroundColor Gray
}

Write-Host "`n[3/3] Collecting results..." -ForegroundColor Cyan

# Collect all results
$totalFixed = 0
$totalStillFailed = 0
$allStillFailed = @()

foreach ($rec in $jobRecords) {
    $job = $rec.Job
    $output = Receive-Job -Job $job -ErrorAction SilentlyContinue
    $parsed = $false
    foreach ($line in $output) {
        if ($line -match "Batch Results:\s*(\d+)\s*fixed,\s*(\d+)\s*still failing") {
            $totalFixed += [int]$matches[1]
            $totalStillFailed += [int]$matches[2]
            $parsed = $true
            break
        }
    }
    Remove-Job -Job $job

    if (Test-Path $rec.BatchOut) {
        try {
            $batchFailed = Get-Content $rec.BatchOut -Raw | ConvertFrom-Json -ErrorAction Stop
            if ($batchFailed -isnot [array]) { $batchFailed = @($batchFailed) }
            $batchFailed = @($batchFailed | Where-Object { $_ })
            $allStillFailed += $batchFailed
            if (-not $parsed) {
                $totalStillFailed += $batchFailed.Count
            }
        } catch {
            Write-Host "WARN: Failed parsing batch output $($rec.BatchOut): $_" -ForegroundColor Yellow
        }
    } elseif (-not $parsed) {
        Write-Host "WARN: Missing expected batch output for batch $($rec.BatchNum): $($rec.BatchOut)" -ForegroundColor Yellow
    }
}

# Final summary + completion log for swarm monitoring
$logPath = "d:\rawrxd\completion_log.txt"
$summary = @"
[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] Swarm complete.
Total Failed (start): $totalFailed
Total Fixed: $totalFixed
Still Failing: $totalStillFailed
Success Rate: $(if ($totalFailed -gt 0) { ($totalFixed / $totalFailed * 100).ToString('F1') } else { '100' })%
"@
Add-Content -Path $logPath -Value $summary -ErrorAction SilentlyContinue

Write-Host "`n`n=== FINAL RESULTS ===" -ForegroundColor Cyan
Write-Host "Total Failed (start): $totalFailed" -ForegroundColor Yellow
Write-Host "Total Fixed: $totalFixed" -ForegroundColor Green
Write-Host "Still Failing: $totalStillFailed" -ForegroundColor Red
Write-Host "Success Rate: $(if ($totalFailed -gt 0) { ($totalFixed / $totalFailed * 100).ToString('F1') } else { '100' })%" -ForegroundColor Yellow
Write-Host "Log: $logPath" -ForegroundColor Gray

if ($totalFailed -eq 0) {
    Write-Host "`n✓ No failed files in manifest." -ForegroundColor Green
    Add-Content -Path $logPath -Value "STATUS: NO FAILURES INPUT" -ErrorAction SilentlyContinue
    exit 0
} elseif ($totalStillFailed -eq 0) {
    Write-Host "`n✓ All files from this manifest batch now compile." -ForegroundColor Green
    Add-Content -Path $logPath -Value "STATUS: ALL FILES COMPILE" -ErrorAction SilentlyContinue
    exit 0
} elseif ($totalFixed -eq 0) {
    Write-Host "`nNo progress in this run (0% fix rate)." -ForegroundColor Red
    Add-Content -Path $logPath -Value "STATUS: NO PROGRESS" -ErrorAction SilentlyContinue
    # Re-emit failed_files.json in orchestrator shape for next pass
    $forJson = $allStillFailed | ForEach-Object {
        if ($_.Path) { $_ } else { [pscustomobject]@{ Path = $null; Type = "unknown"; Error = "Null/invalid manifest entry" } }
    }
    $forJson | ConvertTo-Json -Depth 5 | Out-File "d:\rawrxd\failed_files.json" -Encoding UTF8 -ErrorAction SilentlyContinue
    exit 1
} else {
    Write-Host "`nRemaining failures require manual engineering review:" -ForegroundColor Red
    $allStillFailed | ForEach-Object { $p = if ($_.Path) { $_.Path } else { $_ }; Write-Host "  - $p" -ForegroundColor Red }
    # Re-emit failed_files.json in orchestrator shape (Path, Type, Error) for next pass
    $forJson = $allStillFailed | ForEach-Object {
        if ($_.Path) { $_ } else { [pscustomobject]@{ Path = $_; Type = "cpp"; Error = "Still failing after subagent pass" } }
    }
    $forJson | ConvertTo-Json -Depth 3 | Out-File "d:\rawrxd\failed_files.json" -Encoding UTF8 -ErrorAction SilentlyContinue
    Add-Content -Path $logPath -Value "STATUS: $totalStillFailed REMAINING" -ErrorAction SilentlyContinue
    exit 1
}
