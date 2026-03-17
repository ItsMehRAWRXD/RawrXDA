#!/usr/bin/env pwsh
# PHASE 4: Final Qt Remnants Removal

param([string]$RootPath = "D:\RawrXD\src")

Write-Host "🔥 PHASE 4: FINAL Qt REMNANTS ELIMINATION" -ForegroundColor Cyan

$files = Get-ChildItem -Path $RootPath -Recurse -Include "*.cpp", "*.hpp", "*.h"
$filesModified = 0

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $original = $content
    
    # Constructor parameters with QObject
    $content = $content -replace "\(QObject\s*\*parent\s*=\s*nullptr\)", "()"
    $content = $content -replace "\(QObject\s*\*parent\)", "()"
    
    # Type declarations: QTimer*, QFile, etc
    $content = $content -replace "QTimer\s*\*\s*", "// Timer "
    $content = $content -replace "QFileInfoList", "std::vector<std::string>"
    $content = $content -replace "QDirIterator", "// DirIterator"
    $content = $content -replace "QDateTime", "// DateTime"
    $content = $content -replace "QFile::", "// File::"
    $content = $content -replace "QDir::", "// Dir::"
    $content = $content -replace "QObject::", "// Object::"
    
    # More function signatures
    $content = $content -replace "explicit\s+\w+\(QObject\s*\*parent\s*=\s*nullptr\)", "explicit"
    
    if ($content -ne $original) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8 -Force
        $filesModified++
        Write-Host "  ✓ $($file.Name)"
    }
}

Write-Host "`n✅ PHASE 4 COMPLETE - Files modified: $filesModified" -ForegroundColor Green
