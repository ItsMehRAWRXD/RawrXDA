#!/usr/bin/env pwsh
param([string]$CLI = "D:\RawrXD-production-lazy-init\build\bin-msvc\Release\RawrXD-CLI.exe")

Write-Host "`n=== Testing Enhanced Models Command ===" -ForegroundColor Cyan

# Test models command
$commands = @"
models
quit
"@

Write-Host "Running 'models' command..." -ForegroundColor Yellow
$output = $commands | & $CLI 2>&1 | Out-String

# Check for expected output
$hasScanning = $output -match "Scanning for GGUF models"
$hasDiscovered = $output -match "DISCOVERED MODELS"
$hasNumbering = $output -match "\[\d+\]"
$hasSize = $output -match "\d+\.?\d* (GB|MB)"
$hasLoadHint = $output -match "load <number>"

Write-Host "`nResults:" -ForegroundColor Yellow
Write-Host "  ✓ Scanning message: $hasScanning" -ForegroundColor $(if($hasScanning){'Green'}else{'Red'})
Write-Host "  ✓ Discovered header: $hasDiscovered" -ForegroundColor $(if($hasDiscovered){'Green'}else{'Red'})
Write-Host "  ✓ Numbered models: $hasNumbering" -ForegroundColor $(if($hasNumbering){'Green'}else{'Red'})
Write-Host "  ✓ File sizes shown: $hasSize" -ForegroundColor $(if($hasSize){'Green'}else{'Red'})
Write-Host "  ✓ Load hint shown: $hasLoadHint" -ForegroundColor $(if($hasLoadHint){'Green'}else{'Red'})

if ($output -match "(\d+) found\)") {
    $count = $matches[1]
    Write-Host "`n  → Found $count models" -ForegroundColor Green
}

# Show sample output
Write-Host "`n=== Sample Output ===" -ForegroundColor Cyan
$output -split "`n" | Where-Object { $_ -match "DISCOVERED MODELS|^\s+\[\d+\]|Scanning|load <number>" } | Select-Object -First 10 | ForEach-Object {
    Write-Host "  $_" -ForegroundColor Gray
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Cyan
