# Smoke Test with JSON Output
# Compiles all 1610 files individually, outputs structured JSON with pass/fail + errors

param(
    [string]$OutputFile = "d:\rawrxd\compilation_results.json",
    [switch]$QuickMode
)

$ErrorActionPreference = "SilentlyContinue"

$results = @{
    timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    total_files = 0
    passed = 0
    failed = 0
    files = @()
}

# Compiler paths
$cl = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe"
$ml64 = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"

# Ensure temp directory exists
$tempObj = "d:\rawrxd\temp_obj"
$null = mkdir $tempObj -Force -ErrorAction SilentlyContinue

Write-Host "=== RawrXD Smoke Test (JSON Output) ===" -ForegroundColor Cyan
Write-Host "Output file: $OutputFile`n" -ForegroundColor Gray

# Get all source files
$cppFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue)
$cFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.c" -Recurse -ErrorAction SilentlyContinue)
$asmFiles = @(Get-ChildItem -Path "d:\rawrxd\src" -Filter "*.asm" -Recurse -ErrorAction SilentlyContinue)

$allFiles = @($cppFiles) + @($cFiles) + @($asmFiles)
$results.total_files = $allFiles.Count

Write-Host "CPP files: $($cppFiles.Count)" -ForegroundColor White
Write-Host "C files: $($cFiles.Count)" -ForegroundColor White
Write-Host "ASM files: $($asmFiles.Count)" -ForegroundColor White
Write-Host "Total: $($allFiles.Count)`n" -ForegroundColor White

$processed = 0

# Process CPP and C files
foreach ($file in ($cppFiles + $cFiles)) {
    $processed++
    $outObj = Join-Path $tempObj "$($file.BaseName).obj"
    
    if (-not $QuickMode -and ($processed % 100 -eq 0)) {
        Write-Host "[$processed/$($results.total_files)] Processing..." -ForegroundColor Gray
    }
    
    $errorOutput = $null
    $result = & $cl /c /W0 /Fo"$outObj" $file.FullName 2>&1
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -eq 0) {
        $results.passed++
        $results.files += @{
            file_name = $file.FullName
            file_type = "cpp"
            compilation_status = "PASS"
        }
        Write-Host "✓" -NoNewline -ForegroundColor Green
    } else {
        $results.failed++
        $firstError = ""
        if ($result -is [array]) {
            $firstError = $result[0]
        } else {
            $firstError = $result
        }
        
        $results.files += @{
            file_name = $file.FullName
            file_type = "cpp"
            compilation_status = "FAIL"
            first_error = ($firstError | Out-String).Trim().Substring(0, [Math]::Min(200, ($firstError | Out-String).Length))
            exit_code = $exitCode
        }
        Write-Host "✗" -NoNewline -ForegroundColor Red
    }
    
    if ($processed % 50 -eq 0) {
        Write-Host " [$processed]" -ForegroundColor Gray
    }
}

Write-Host "`n"

# Process ASM files
foreach ($file in $asmFiles) {
    $processed++
    $outObj = Join-Path $tempObj "$($file.BaseName).obj"
    
    if (-not $QuickMode -and ($processed % 100 -eq 0)) {
        Write-Host "[$processed/$($results.total_files)] Processing..." -ForegroundColor Gray
    }
    
    $result = & $ml64 /c /Fo"$outObj" $file.FullName 2>&1
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -eq 0) {
        $results.passed++
        $results.files += @{
            file_name = $file.FullName
            file_type = "asm"
            compilation_status = "PASS"
        }
        Write-Host "✓" -NoNewline -ForegroundColor Green
    } else {
        $results.failed++
        $firstError = ""
        if ($result -is [array]) {
            $firstError = $result[0]
        } else {
            $firstError = $result
        }
        
        $results.files += @{
            file_name = $file.FullName
            file_type = "asm"
            compilation_status = "FAIL"
            first_error = ($firstError | Out-String).Trim().Substring(0, [Math]::Min(200, ($firstError | Out-String).Length))
            exit_code = $exitCode
        }
        Write-Host "✗" -NoNewline -ForegroundColor Red
    }
    
    if ($processed % 50 -eq 0) {
        Write-Host " [$processed]" -ForegroundColor Gray
    }
}

Write-Host "`n"

# Output results
Write-Host "`n=== Results ===" -ForegroundColor Cyan
Write-Host "Passed: $($results.passed) / $($results.total_files)" -ForegroundColor Green
Write-Host "Failed: $($results.failed) / $($results.total_files)" -ForegroundColor Red
Write-Host "Success Rate: $(($results.passed / $results.total_files * 100).ToString('F1'))%`n" -ForegroundColor Yellow

# Save JSON
$results | ConvertTo-Json -Depth 5 | Out-File -FilePath $OutputFile -Encoding UTF8

Write-Host "Results saved to: $OutputFile" -ForegroundColor Green

# Show first 10 failures
$failures = @($results.files | Where-Object { $_.compilation_status -eq "FAIL" })
if ($failures.Count -gt 0) {
    Write-Host "`nFirst 10 failures:" -ForegroundColor Yellow
    $failures | Select-Object -First 10 | ForEach-Object {
        Write-Host "  ✗ $([System.IO.Path]::GetFileName($_.file_name))" -ForegroundColor Red
        Write-Host "    Error: $($_.first_error)" -ForegroundColor Gray
    }
}

exit ($results.failed)
