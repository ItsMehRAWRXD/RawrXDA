#!/usr/bin/env pwsh
# PHASE 5: Template & Parameter Type Cleanup

param([string]$RootPath = "D:\RawrXD\src")

Write-Host "🔥 PHASE 5: PARAMETER & TEMPLATE TYPE CLEANUP" -ForegroundColor Cyan

$files = Get-ChildItem -Path $RootPath -Recurse -Include "*.cpp", "*.hpp", "*.h"
$filesModified = 0

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $original = $content
    
    # Parameters: QWidget* parent, QObject* parent, etc
    $content = $content -replace "QWidget\s*\*\s*parent\s*=\s*nullptr", "void* parent = nullptr"
    $content = $content -replace "QWidget\s*\*\s*parent\)", "void* parent)"
    $content = $content -replace "QObject\s*\*\s*parent\s*=\s*nullptr", "void* parent = nullptr"
    $content = $content -replace "QObject\s*\*\s*parent\)", "void* parent)"
    
    # Template unique_ptr, shared_ptr, etc
    $content = $content -replace "std::unique_ptr<QTimer>", "std::unique_ptr<void>"
    $content = $content -replace "std::unique_ptr<QFile>", "std::unique_ptr<void>"
    $content = $content -replace "std::unique_ptr<QWidget>", "std::unique_ptr<void>"
    $content = $content -replace "std::shared_ptr<QObject>", "std::shared_ptr<void>"
    
    # Other Qt types in declarations
    $content = $content -replace "QTimer\s*\*\s*\w+\s*=", "void* timer_var ="
    $content = $content -replace "QWidget\s*\*\s*", "void* "
    $content = $content -replace "QObject\s*\*\s*", "void* "
    $content = $content -replace "QDir\s*", "// "
    $content = $content -replace "QFile\s*", "// "
    
    # Remaining Q* type calls
    $content = $content -replace "QDir\(", "// Dir("
    $content = $content -replace "QFile\(", "// File("
    $content = $content -replace "QTimer\(", "// Timer("
    $content = $content -replace "QWidget\(", "// Widget("
    $content = $content -replace "QObject\(", "// Object("
    
    if ($content -ne $original) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8 -Force
        $filesModified++
        Write-Host "  ✓ $($file.Name)"
    }
}

Write-Host "`n✅ PHASE 5 COMPLETE - Files modified: $filesModified" -ForegroundColor Green
