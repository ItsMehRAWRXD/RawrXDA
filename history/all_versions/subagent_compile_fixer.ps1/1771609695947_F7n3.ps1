# Subagent Compilation Fixer - Processes one batch of failed files in parallel
# Usage: Each subagent gets a slice of failed_files.json and fixes each file
param(
    [string]$FailedFilesJson = "d:\rawrxd\failed_files.json",
    [int]$BatchStart = 0,
    [int]$BatchSize = 50
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $FailedFilesJson)) {
    Write-Error "Failed files JSON not found: $FailedFilesJson"
    exit 1
}

# Load failed files
$failedData = Get-Content $FailedFilesJson -Raw | ConvertFrom-Json
if ($failedData -isnot [array]) {
    $failedData = @($failedData)
}

$batch = $failedData | Select-Object -Skip $BatchStart -First $BatchSize
$fixed = 0
$skipped = 0
$stillFailed = @()

Write-Host "Processing batch: files $BatchStart to $($BatchStart + $batch.Count - 1)" -ForegroundColor Cyan

foreach ($item in $batch) {
    $filePath = $item.Path
    $fileType = $item.Type
    
    if (-not (Test-Path $filePath)) {
        Write-Host "✗ File not found: $filePath" -ForegroundColor Red
        $skipped++
        continue
    }
    
    $content = Get-Content $filePath -Raw
    $originalContent = $content
    
    # ===== COMMON FIX PATTERNS =====
    
    # 1. Missing #include guards or includes
    if ($content -notmatch "#pragma once" -and $content -notmatch "#ifndef") {
        if ($filePath -like "*.h") {
            $headerGuard = "RAWRXD_" + [System.IO.Path]::GetFileNameWithoutExtension($filePath).ToUpper() + "_H"
            $content = "#pragma once`n`n" + $content
        }
    }
    
    # 2. Undefined identifiers - add forward declarations
    if ($content -like "*undefined*" -or $content -like "*not declared*") {
        # Common Windows types that might be missing
        if ($content -like "*OutputDebugStringA*" -and $content -notmatch "#include.*Windows.h") {
            $content = "#include <windows.h>`n" + $content
        }
        if ($content -like "*HWND*" -and $content -notmatch "#include.*Windows.h") {
            $content = "#include <windows.h>`n" + $content
        }
        if ($content -like "*std::*" -and $content -notmatch "#include.*algorithm|vector|string|memory") {
            if ($content -like "*std::vector*") { $content = "#include <vector>`n" + $content }
            if ($content -like "*std::string*") { $content = "#include <string>`n" + $content }
            if ($content -like "*std::make_shared*") { $content = "#include <memory>`n" + $content }
        }
    }
    
    # 3. Duplicate definitions - comment out duplicates
    $lines = $content -split "`n"
    $seen = @{}
    $deduped = @()
    
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        # Skip empty/comment lines
        if (-not $trimmed -or $trimmed.StartsWith("//")) {
            $deduped += $line
            continue
        }
        
        # Detect duplicate function/class definitions
        if ($trimmed -match "^(void|int|bool|std|class|struct)\s+\w+\s*\(") {
            $sig = $trimmed.Substring(0, [Math]::Min(60, $trimmed.Length))
            if ($seen.ContainsKey($sig)) {
                $deduped += "// [DEDUPED] $line"
            } else {
                $seen[$sig] = $true
                $deduped += $line
            }
        } else {
            $deduped += $line
        }
    }
    $content = $deduped -join "`n"
    
    # 4. Missing return statements
    $content = $content -replace '(?m)(\s*\n\s*\}\s*$)', "`n    return true;`n}`n"
    
    # 5. Syntax errors in catch blocks
    $content = $content -replace 'catch\s*\(\.\.\.', 'catch (...'
    $content = $content -replace 'catch\s*\(\s*Exception', 'catch (std::exception'
    
    # Write back if changed
    if ($content -ne $originalContent) {
        Set-Content -Path $filePath -Value $content -Encoding UTF8
    }
    
    # Verify compilation
    if ($fileType -eq "cpp" -or $fileType -eq "c") {
        $cl = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe"
        $result = & $cl /c /Fo"nul:" $filePath 2>&1
        $compiles = $LASTEXITCODE -eq 0
    } else {
        $ml64 = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
        $result = & $ml64 /c /Fo"nul:" $filePath 2>&1
        $compiles = $LASTEXITCODE -eq 0
    }
    
    if ($compiles) {
        Write-Host "✓ $([System.IO.Path]::GetFileName($filePath))" -ForegroundColor Green
        $fixed++
    } else {
        Write-Host "✗ $([System.IO.Path]::GetFileName($filePath))" -ForegroundColor Red
        $stillFailed += $filePath
    }
}

Write-Host "`nBatch Results: $fixed fixed, $($batch.Count - $fixed) still failing" -ForegroundColor Yellow

if ($stillFailed.Count -gt 0) {
    $stillFailed | ConvertTo-Json | Out-File -FilePath "d:\rawrxd\batch_$BatchStart`_failed.json" -Encoding UTF8
    Write-Host "Unfixed files saved to: d:\rawrxd\batch_$BatchStart`_failed.json" -ForegroundColor Yellow
}

exit ($stillFailed.Count)
