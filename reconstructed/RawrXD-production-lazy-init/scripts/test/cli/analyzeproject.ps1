#!/usr/bin/env pwsh
param(
    [string]$CLIPath = "D:\RawrXD-production-lazy-init\build\bin-msvc\Release\RawrXD-CLI.exe",
    [string]$TestPath = "D:\RawrXD-production-lazy-init\src\cli"
)

Write-Host "`n=== Testing CLI analyzeproject Command ===" -ForegroundColor Cyan
Write-Host "CLI: $CLIPath"
Write-Host "Path: $TestPath`n"

# Create input commands
$commands = @"
analyzeproject $TestPath
quit
"@

# Run CLI with piped input
$output = $commands | & $CLIPath 2>&1 | Out-String

# Check for success indicators
$hasStats = $output -match "PROJECT STATISTICS"
$hasFiles = $output -match "Total Files:"
$hasComplete = $output -match "Project analysis complete"
$noError = -not ($output -match "requires Qt IDE")

Write-Host "Results:" -ForegroundColor Yellow
Write-Host "  Found PROJECT STATISTICS: $hasStats"
Write-Host "  Found Total Files count: $hasFiles"
Write-Host "  Found completion message: $hasComplete"
Write-Host "  No 'requires Qt IDE' error: $noError"

if ($hasStats -and $hasFiles -and $hasComplete -and $noError) {
    Write-Host "`n✅ SUCCESS: analyzeproject works in CLI!" -ForegroundColor Green
    
    # Extract and show key stats
    if ($output -match "Total Files: (\d+)") {
        Write-Host "  → Total Files: $($matches[1])" -ForegroundColor Green
    }
    if ($output -match "C\+\+ Files \(.cpp\): (\d+)") {
        Write-Host "  → C++ Files: $($matches[1])" -ForegroundColor Green
    }
    if ($output -match "Header Files \(.h/.hpp\): (\d+)") {
        Write-Host "  → Header Files: $($matches[1])" -ForegroundColor Green
    }
    if ($output -match "Total Lines: (\d+)") {
        Write-Host "  → Total Lines: $($matches[1])" -ForegroundColor Green
    }
    
    exit 0
} else {
    Write-Host "`n❌ FAILED: Command did not produce expected output" -ForegroundColor Red
    Write-Host "`nOutput sample:" -ForegroundColor Yellow
    $output -split "`n" | Select-Object -First 50 | ForEach-Object { Write-Host "  $_" }
    exit 1
}
