# Split failed_files.json into batch files for reverse swarm processing
param(
    [int]$FilesPerBatch = 50
)

$ErrorActionPreference = "Stop"

Write-Host "Splitting failed_files.json into batches of $FilesPerBatch files..." -ForegroundColor Cyan

if (-not (Test-Path "d:\rawrxd\failed_files.json")) {
    Write-Host "failed_files.json not found!" -ForegroundColor Red
    exit 1
}

$failedData = Get-Content "d:\rawrxd\failed_files.json" -Raw | ConvertFrom-Json
if ($failedData -isnot [array]) {
    $failedData = @($failedData)
}

$totalFailed = $failedData.Count
$totalBatches = [Math]::Ceiling($totalFailed / $FilesPerBatch)

Write-Host "Total failed files: $totalFailed" -ForegroundColor Yellow
Write-Host "Creating $totalBatches batch files..." -ForegroundColor Yellow

for ($batch = 0; $batch -lt $totalBatches; $batch++) {
    $startIndex = $batch * $FilesPerBatch
    $endIndex = [Math]::Min($startIndex + $FilesPerBatch - 1, $totalFailed - 1)
    $batchData = $failedData[$startIndex..$endIndex]
    
    $batchFile = "d:\rawrxd\batch_${batch}_failed.json"
    $batchData | ConvertTo-Json | Set-Content $batchFile -Encoding UTF8
    
    Write-Host "  Created batch_${batch}_failed.json with $($batchData.Count) files" -ForegroundColor Gray
}

Write-Host "Batch files created successfully!" -ForegroundColor Green