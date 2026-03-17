#!/usr/bin/env pwsh
# PHASE 3: Complete Qt Code Usage Elimination

param([string]$RootPath = "D:\RawrXD\src", [switch]$PreviewOnly = $false)

Write-Host "🔥 PHASE 3: COMPLETE Qt CODE USAGE ELIMINATION" -ForegroundColor Cyan

$files = Get-ChildItem -Path $RootPath -Recurse -Include "*.cpp", "*.hpp", "*.h"
$filesModified = 0

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $original = $content
    
    # Remove QObject constructor initializers
    $content = $content -replace ":\s*QObject\(parent\)", ""
    $content = $content -replace ":\s*QObject\(\)", ""
    
    # Remove QFile operations - replace with std::filesystem equivalents
    $content = $content -replace "QFile\s+\w+\s*\([^)]+\)", "// File operation removed"
    $content = $content -replace "QFile::exists\(([^)]+)\)", "std::filesystem::exists(`$1)"
    $content = $content -replace "QFile::remove\(([^)]+)\)", "std::filesystem::remove(`$1)"
    $content = $content -replace "QFile::copy\(", "std::filesystem::copy("
    $content = $content -replace "QFile ", "// "
    
    # Remove QDir operations
    $content = $content -replace "QDir\(\)\.mkpath\(([^)]+)\)", "std::filesystem::create_directories(`$1)"
    $content = $content -replace "QDir ", "// "
    $content = $content -replace "\.entryInfoList\([^)]*\)", "// Dir listing"
    
    # Remove QTimer
    $content = $content -replace "QTimer\s+", "// Timer "
    $content = $content -replace "QTimer::", "// Timer::"
    
    # Remove connect calls
    $content = $content -replace "QObject::connect\([^)]*\)", "// Connect removed"
    $content = $content -replace "\bconnect\s*\([^)]*\)", "// Connect removed"
    
    # Fix path methods
    $content = $content -replace "\.absolutePath\(\)", ".string()"
    $content = $content -replace "\.absoluteFilePath\(\)", ".string()"
    
    if ($content -ne $original) {
        if (-not $PreviewOnly) {
            Set-Content -Path $file.FullName -Value $content -Encoding UTF8 -Force
            $filesModified++
            Write-Host "  ✓ $($file.Name)"
        }
    }
}

Write-Host "`n✅ PHASE 3 COMPLETE - Files modified: $filesModified" -ForegroundColor Green
