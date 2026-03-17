# Smoke Test: Compile/Assemble each source file individually
# Verifies every module compiles before integration

param(
    [switch]$QuickMode
)

$ErrorActionPreference = "SilentlyContinue"

# Initialize counters
$cppFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.cpp" -Recurse)
$cFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.c" -Recurse)
$asmFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.asm" -Recurse)

$totalFiles = $cppFiles.Count + $cFiles.Count + $asmFiles.Count
$passed = 0
$failed = 0
$failedFiles = @()

Write-Host "=== RawrXD Source Smoke Test ===" -ForegroundColor Cyan
Write-Host "Total files to verify: $totalFiles" -ForegroundColor White
Write-Host "CPP: $($cppFiles.Count), C: $($cFiles.Count), ASM: $($asmFiles.Count)`n" -ForegroundColor Gray

# Setup compile environment
$cl = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe"
$ml64 = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
$tempObj = "d:\rawrxd\temp_obj"
$null = mkdir $tempObj -Force -ErrorAction SilentlyContinue

$compileCount = 0

# Test CPP/C files
$allCppC = $cppFiles + $cFiles
foreach ($file in $allCppC) {
    $compileCount++
    
    if ($env:TERM) {
        Write-Progress -Activity "Compiling ($compileCount/$totalFiles)" -Status $file.Name -PercentComplete (($compileCount / $totalFiles) * 100)
    }
    
    $outObj = Join-Path $tempObj "$($file.BaseName).obj"
    $result = & $cl /c /Fo"$outObj" $file.FullName 2>&1
    
    if ($LASTEXITCODE -eq 0 -or $result -like "*error*" -eq $false) {
        $passed++
        if (-not $QuickMode) {
            Write-Host "✓" -NoNewline -ForegroundColor Green
        }
    } else {
        $failed++
        $failedFiles += $file.FullName
        if (-not $QuickMode) {
            Write-Host "✗" -NoNewline -ForegroundColor Red
        }
    }
    
    if ($compileCount % 50 -eq 0) {
        Write-Host " [$compileCount/$totalFiles]" -ForegroundColor Gray
    }
}

Write-Host "`n"

# Test ASM files
foreach ($file in $asmFiles) {
    $compileCount++
    
    if ($env:TERM) {
        Write-Progress -Activity "Assembling ($compileCount/$totalFiles)" -Status $file.Name -PercentComplete (($compileCount / $totalFiles) * 100)
    }
    
    $outObj = Join-Path $tempObj "$($file.BaseName).obj"
    $result = & $ml64 /c /Fo"$outObj" $file.FullName 2>&1
    
    if ($LASTEXITCODE -eq 0 -or $result -like "*error*" -eq $false) {
        $passed++
        if (-not $QuickMode) {
            Write-Host "✓" -NoNewline -ForegroundColor Green
        }
    } else {
        $failed++
        $failedFiles += $file.FullName
        if (-not $QuickMode) {
            Write-Host "✗" -NoNewline -ForegroundColor Red
        }
    }
    
    if ($compileCount % 50 -eq 0) {
        Write-Host " [$compileCount/$totalFiles]" -ForegroundColor Gray
    }
}

Write-Host "`n`n=== Results ===" -ForegroundColor Cyan
Write-Host "Passed: $passed" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor Red
Write-Host "Success Rate: $(($passed / $totalFiles * 100).ToString('F1'))%" -ForegroundColor Yellow

if ($failed -gt 0) {
    Write-Host "`nFailed files:" -ForegroundColor Red
    $failedFiles | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
}

# Cleanup
Remove-Item $tempObj -Force -Recurse -ErrorAction SilentlyContinue

if ($failed -eq 0) {
    Write-Host "`n✓ All modules verified!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`n✗ $failed modules failed verification" -ForegroundColor Red
    exit 1
}
