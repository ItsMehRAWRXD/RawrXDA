# Compile Fix Orchestrator - Run smoke test, identify failures, spawn parallel fixers
param(
    [int]$MaxParallelSubagents = 8,
    [switch]$DryRun
)

$ErrorActionPreference = "Continue"

# Step 1: Run smoke test to get list of failing files
Write-Host "=== Step 1: Running Smoke Test ===" -ForegroundColor Cyan

$cppFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.cpp" -Recurse)
$cFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.c" -Recurse)
$asmFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.asm" -Recurse)

$totalFiles = $cppFiles.Count + $cFiles.Count + $asmFiles.Count
$failedFiles = @()

Write-Host "Total files to test: $totalFiles`n" -ForegroundColor White

# Compile test - collect all failures
$allCppC = $cppFiles + $cFiles
foreach ($file in $allCppC) {
    $cl = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe"
    $result = & $cl /c /Fo"nul:" $file.FullName 2>&1
    
    if ($LASTEXITCODE -ne 0 -or $result -like "*error*") {
        $failedFiles += @{
            Path = $file.FullName
            Type = "cpp"
            Error = $result -join "`n"
        }
    }
}

# Assemble test - collect failures
foreach ($file in $asmFiles) {
    $ml64 = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
    $result = & $ml64 /c /Fo"nul:" $file.FullName 2>&1
    
    if ($LASTEXITCODE -ne 0 -or $result -like "*error*") {
        $failedFiles += @{
            Path = $file.FullName
            Type = "asm"
            Error = $result -join "`n"
        }
    }
}

Write-Host "Smoke test complete: $($failedFiles.Count) failures found`n" -ForegroundColor Yellow

if ($failedFiles.Count -eq 0) {
    Write-Host "✓ ALL FILES COMPILE SUCCESSFULLY" -ForegroundColor Green
    exit 0
}

# Step 2: Output failures for subagent processing
Write-Host "=== Step 2: Failed Files for Subagent Processing ===" -ForegroundColor Cyan
Write-Host "Total: $($failedFiles.Count) files need fixes`n" -ForegroundColor Yellow

# Save to JSON for subagent consumption
$failedFiles | ConvertTo-Json | Out-File -FilePath "d:\rawrxd\failed_files.json" -Encoding UTF8

Write-Host "Failed files saved to: d:\rawrxd\failed_files.json" -ForegroundColor Green
Write-Host "Ready for parallel subagent processing (max $MaxParallelSubagents in parallel)`n" -ForegroundColor Cyan

# Output batch instructions
Write-Host "=== Subagent Task Batch ===" -ForegroundColor Magenta
Write-Host "Spawn $MaxParallelSubagents subagents with task:"
Write-Host "  - Read failed_files.json"
Write-Host "  - Fix compilation error in each file"
Write-Host "  - Verify file compiles after fix"
Write-Host "  - Report success/failure"
Write-Host "`nNo architecture changes. Pure compilation fixes only.`n"

if ($DryRun) {
    Write-Host "[DRY RUN] Would spawn $MaxParallelSubagents subagents" -ForegroundColor Gray
} else {
    Write-Host "Ready to spawn subagents. Use manage_subagent with failed_files.json" -ForegroundColor Green
}
