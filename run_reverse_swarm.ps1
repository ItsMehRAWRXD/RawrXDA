# RawrXD Swarm Reverse Controller
# Target: Specific batch files in descending order

$RepoRoot  = "d:\rawrxd"
$BuildDir  = Join-Path $RepoRoot "build"
$FixerPath = Join-Path $RepoRoot "spawn_subagent_fixers.ps1"

# Dynamically find all batch files and sort in descending order
$batchFiles = Get-ChildItem "d:\rawrxd\batch_*_failed.json" | 
    Sort-Object { [int]($_.Name -replace 'batch_(\d+)_failed\.json', '$1') } -Descending |
    Select-Object -ExpandProperty Name

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Yellow
Write-Host "║           RAWXRD SWARM: REVERSE FIXING LOOP               ║" -ForegroundColor Yellow
Write-Host "║           Sequence: Highest -> Lowest (Backwards)         ║" -ForegroundColor Yellow
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Yellow

# Iterate through the batch files
foreach ($manifestName in $batchFiles) {
    $manifestPath = Join-Path $RepoRoot $manifestName

    if (Test-Path $manifestPath) {
        Write-Host ""
        Write-Host ">>> [BATCH $($manifestName -replace 'batch_(\d+)_failed\.json', '$1')] Initiating repair on $manifestName..." -ForegroundColor Cyan

        # Invoke the subagent swarm on this specific manifest        
        & $FixerPath -ManifestPath $manifestPath -MaxParallel 16 -FilesPerBatch 100

        $status = if ($LASTEXITCODE -eq 0) { "SUCCESS" } else { "PARTIAL" }
        Write-Host ">>> [BATCH $($manifestName -replace 'batch_(\d+)_failed\.json', '$1')] Status: $status" -ForegroundColor $(if ($status -eq "SUCCESS") { "Green" } else { "Yellow" })
    } else {
        Write-Host ">>> [BATCH $($manifestName -replace 'batch_(\d+)_failed\.json', '$1')] Skipped: $manifestName not found." -ForegroundColor DarkGray
    }
}

Write-Host ""
Write-Host "✅ Reverse Swarm Sequence Complete." -ForegroundColor Green