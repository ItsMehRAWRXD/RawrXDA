#!/usr/bin/env pwsh
# Aggressive Qt method cleanup

$srcPath = "D:\rawrxd\src"
$fileList = Get-ChildItem -Path $srcPath -Recurse -Include "*.cpp", "*.h", "*.hpp"

Write-Host "Running aggressive Qt cleanup on $($fileList.Count) files..."

$totalChanges = 0
foreach ($file in $fileList) {
    try {
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if (-not $content) { continue }
        
        $original = $content
        
        # Make replacements - be very explicit
        $content = $content -replace '\.isEmpty\(\)', '.empty()'
        $content = $content -replace '\bqint64\b', 'int64_t'
        $content = $content -replace '\bqint32\b', 'int32_t'
        $content = $content -replace '\bquint64\b', 'uint64_t'
        $content = $content -replace '\bquint32\b', 'uint32_t'
        $content = $content -replace '\bqint8\b', 'int8_t'
        $content = $content -replace '\bquint8\b', 'uint8_t'
        
        if ($content -ne $original) {
            Set-Content -Path $file.FullName -Value $content -Force
            $totalChanges++
        }
    } catch {
        # Silently skip files with errors
    }
}

Write-Host "Processed $totalChanges files with changes"
