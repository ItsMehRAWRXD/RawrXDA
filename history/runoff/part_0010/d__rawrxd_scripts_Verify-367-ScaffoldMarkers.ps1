#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Verify which of the 367 scaffold markers are present in source code.
.DESCRIPTION
    Scans all source files for SCAFFOLD_NNN markers and generates a report
    showing which markers are applied, missing, or duplicated.
#>

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$srcRoot = Join-Path $repoRoot "src"

Write-Host "=== Scaffold Marker Verification Report ===" -ForegroundColor Cyan
Write-Host "Scanning: $srcRoot" -ForegroundColor Gray

# Find all markers in source code
$foundMarkers = @{}
$markerPattern = '(?://|;)\s*SCAFFOLD_(\d{3})'

Get-ChildItem -Path $srcRoot -Recurse -Include *.cpp,*.h,*.hpp,*.asm -File -ErrorAction SilentlyContinue | ForEach-Object {
    $filePath = $_.FullName
    $matches = Select-String -Path $filePath -Pattern $markerPattern -AllMatches
    
    foreach ($match in $matches) {
        if ($match.Matches.Groups.Count -ge 2) {
            $markerNum = [int]$match.Matches.Groups[1].Value
            $markerID = "SCAFFOLD_$($match.Matches.Groups[1].Value)"
            
            if (-not $foundMarkers.ContainsKey($markerID)) {
                $foundMarkers[$markerID] = @()
            }
            $foundMarkers[$markerID] += $filePath.Replace($repoRoot + "\", "")
        }
    }
}

# Check against expected 1-367
$expectedMarkers = 1..367 | ForEach-Object { "SCAFFOLD_{0:D3}" -f $_ }
$foundIDs = $foundMarkers.Keys | Sort-Object
$missingMarkers = $expectedMarkers | Where-Object { $_ -notin $foundIDs }
$duplicatedMarkers = $foundMarkers.GetEnumerator() | Where-Object { $_.Value.Count -gt 1 }

Write-Host "`n=== Summary ===" -ForegroundColor Cyan
Write-Host "Total unique markers found: $($foundIDs.Count)" -ForegroundColor Green
Write-Host "Total marker references: $($foundMarkers.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum" -ForegroundColor White
Write-Host "Expected markers: 367" -ForegroundColor White
Write-Host "Missing markers: $($missingMarkers.Count)" -ForegroundColor $(if ($missingMarkers.Count -eq 0) { 'Green' } elseif ($missingMarkers.Count -lt 50) { 'Yellow' } else { 'Red' })
Write-Host "Duplicated markers: $($duplicatedMarkers.Count)" -ForegroundColor $(if ($duplicatedMarkers.Count -eq 0) { 'Green' } else { 'Yellow' })

if ($missingMarkers.Count -gt 0 -and $missingMarkers.Count -le 20) {
    Write-Host "`n=== Missing Markers ===" -ForegroundColor Yellow
    $missingMarkers | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
} elseif ($missingMarkers.Count -gt 20) {
    Write-Host "`n=== Missing Markers (first 20) ===" -ForegroundColor Red
    $missingMarkers | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    Write-Host "  ... and $($missingMarkers.Count - 20) more" -ForegroundColor DarkGray
}

if ($duplicatedMarkers.Count -gt 0 -and $duplicatedMarkers.Count -le 10) {
    Write-Host "`n=== Duplicated Markers ===" -ForegroundColor Yellow
    $duplicatedMarkers | ForEach-Object {
        Write-Host "  $($_.Key): $($_.Value.Count) occurrences" -ForegroundColor Gray
    }
} elseif ($duplicatedMarkers.Count -gt 10) {
    Write-Host "`n=== Duplicated Markers (first 10) ===" -ForegroundColor Yellow
    $duplicatedMarkers | Select-Object -First 10 | ForEach-Object {
        Write-Host "  $($_.Key): $($_.Value.Count) occurrences" -ForegroundColor Gray
    }
    Write-Host "  ... and $($duplicatedMarkers.Count - 10) more" -ForegroundColor DarkGray
}

# Export detailed report
$reportPath = Join-Path $repoRoot "reports\scaffold-marker-verification.json"
$reportDir = Split-Path -Parent $reportPath
if (-not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
}

$report = @{
    timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    totalFound = $foundIDs.Count
    totalReferences = ($foundMarkers.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
    expected = 367
    missing = $missingMarkers
    found = @{}
}

foreach ($entry in $foundMarkers.GetEnumerator()) {
    $report.found[$entry.Key] = $entry.Value
}

$report | ConvertTo-Json -Depth 10 | Set-Content -Path $reportPath -Encoding UTF8
Write-Host "`nDetailed report saved: $reportPath" -ForegroundColor Green

# Return status code
if ($missingMarkers.Count -eq 0) {
    Write-Host "`n✓ All 367 scaffold markers are present!" -ForegroundColor Green
    exit 0
} elseif ($foundIDs.Count -ge 300) {
    Write-Host "`n⚠ Most markers applied ($(([math]::Round(($foundIDs.Count/367)*100, 1)))% coverage)" -ForegroundColor Yellow
    exit 0
} else {
    Write-Host "`n❌ Significant markers missing ($(([math]::Round(($foundIDs.Count/367)*100, 1)))% coverage)" -ForegroundColor Red
    exit 1
}
