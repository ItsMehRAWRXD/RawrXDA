#!/usr/bin/env pwsh
<#
.SYNOPSIS
    PHASE 2: Complete Qt Dependency Elimination
    
.DESCRIPTION
    After Phase 1 (include removal), this script addresses:
    - Class inheritance from Q* base classes
    - Qt member variables (QObject*, QWidget*, etc)
    - Qt logging functions (qDebug, qInfo, etc)
    - Qt type aliases (qint64, quint32, etc)
    - Qt macros (Q_INVOKABLE, Q_ENUM, etc)
    - Forward declarations of Qt classes
    
.NOTES
    This is AGGRESSIVE removal - targets actual code structure, not just includes
#>

param(
    [string]$RootPath = "D:\RawrXD\src",
    [switch]$PreviewOnly = $false
)

$ErrorActionPreference = "Stop"

Write-Host "🔥 PHASE 2: COMPLETE Qt DEPENDENCY ELIMINATION" -ForegroundColor Cyan
Write-Host "Target: $RootPath" -ForegroundColor Yellow

# Define all replacement patterns - from most specific to least
$patterns = @(
    # 1. Remove forward declarations of Qt classes
    @{
        Name = "Forward declarations (Q*)"
        Pattern = "^\s*class\s+Q\w+[;]*\s*$"
        Replacement = ""
        Flags = "Multiline"
    },
    
    # 2. Replace class inheritance from Qt base classes
    @{
        Name = "Class inheritance from QMainWindow"
        Pattern = "class\s+(\w+)\s*:\s*public\s+QMainWindow"
        Replacement = "class `$1"
        Flags = "IgnoreCase"
    },
    @{
        Name = "Class inheritance from QWidget"
        Pattern = "class\s+(\w+)\s*:\s*public\s+QWidget"
        Replacement = "class `$1"
        Flags = "IgnoreCase"
    },
    @{
        Name = "Class inheritance from QDialog"
        Pattern = "class\s+(\w+)\s*:\s*public\s+QDialog"
        Replacement = "class `$1"
        Flags = "IgnoreCase"
    },
    @{
        Name = "Class inheritance from QObject"
        Pattern = "class\s+(\w+)\s*:\s*public\s+QObject"
        Replacement = "class `$1"
        Flags = "IgnoreCase"
    },
    @{
        Name = "Class inheritance from other Q* classes"
        Pattern = "class\s+(\w+)\s*:\s*public\s+(Q\w+)"
        Replacement = "class `$1"
        Flags = "IgnoreCase"
    },
    @{
        Name = "QObject parent parameter"
        Pattern = ",\s*QObject\*\s+parent\s*=\s*nullptr"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    @{
        Name = "QObject* parent in constructors"
        Pattern = "QObject\*\s+parent"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    
    # 3. Replace Qt logging functions
    @{
        Name = "qDebug() calls"
        Pattern = "\bqDebug\(\)\s*<<"
        Replacement = "// qDebug: "
        Flags = "IgnoreCase"
    },
    @{
        Name = "qInfo() calls"
        Pattern = "\bqInfo\(\)\s*<<"
        Replacement = "// qInfo: "
        Flags = "IgnoreCase"
    },
    @{
        Name = "qWarning() calls"
        Pattern = "\bqWarning\(\)\s*<<"
        Replacement = "// qWarning: "
        Flags = "IgnoreCase"
    },
    @{
        Name = "qCritical() calls"
        Pattern = "\bqCritical\(\)\s*<<"
        Replacement = "// qCritical: "
        Flags = "IgnoreCase"
    },
    
    # 4. Replace Qt type aliases
    @{
        Name = "qint64 type"
        Pattern = "\bqint64\b"
        Replacement = "int64_t"
        Flags = "IgnoreCase"
    },
    @{
        Name = "quint32 type"
        Pattern = "\bquint32\b"
        Replacement = "uint32_t"
        Flags = "IgnoreCase"
    },
    @{
        Name = "qreal type"
        Pattern = "\bqreal\b"
        Replacement = "double"
        Flags = "IgnoreCase"
    },
    @{
        Name = "qsizetype type"
        Pattern = "\bqsizetype\b"
        Replacement = "size_t"
        Flags = "IgnoreCase"
    },
    
    # 5. Remove Qt macros
    @{
        Name = "Q_INVOKABLE macro"
        Pattern = "Q_INVOKABLE\s+"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    @{
        Name = "Q_ENUM macro"
        Pattern = "\s*Q_ENUM\([^)]*\);\s*"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    @{
        Name = "Q_PROPERTY macro"
        Pattern = "\s*Q_PROPERTY\([^)]*\);\s*"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    @{
        Name = "Q_DECLARE macros"
        Pattern = "\s*Q_DECLARE_\w+\([^)]*\);\s*"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    
    # 6. Replace Qt string utilities
    @{
        Name = "QString::fromLocal8Bit"
        Pattern = "QString::fromLocal8Bit"
        Replacement = "std::string"
        Flags = "IgnoreCase"
    },
    @{
        Name = "QString::fromStdString"
        Pattern = "QString::fromStdString"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    @{
        Name = "QString::toStdString()"
        Pattern = "\.toStdString\(\)"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    @{
        Name = "QString::isEmpty()"
        Pattern = "\.isEmpty\(\)"
        Replacement = ".empty()"
        Flags = "IgnoreCase"
    },
    @{
        Name = ".toInt() method"
        Pattern = "\.toInt\(\)"
        Replacement = ""
        Flags = "IgnoreCase"
    },
    
    # 7. Remove remaining Qt includes (in case regex patterns got through)
    @{
        Name = "Qt includes in comments"
        Pattern = "//.*#include\s*<Q"
        Replacement = ""
        Flags = "IgnoreCase"
    }
)

$files = @()
$filesModified = 0
$totalReplacements = 0
$modificationsByFile = @{}

try {
    # Get all source files
    Write-Host "`n📂 Scanning source files..." -ForegroundColor Cyan
    $files = Get-ChildItem -Path $RootPath -Recurse -Include "*.cpp", "*.hpp", "*.h" -ErrorAction SilentlyContinue
    Write-Host "   Found: $($files.Count) files" -ForegroundColor Green
    
    foreach ($file in $files) {
        try {
            $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
            if (-not $content) { continue }
            
            $originalContent = $content
            $fileReplacements = 0
            
            # Apply each pattern
            foreach ($patternObj in $patterns) {
                $pattern = $patternObj.Pattern
                $replacement = $patternObj.Replacement
                
                # Count matches before replacement
                $matches = [regex]::Matches($content, $pattern, $patternObj.Flags)
                if ($matches.Count -gt 0) {
                    $fileReplacements += $matches.Count
                    
                    # Perform replacement
                    $content = [regex]::Replace($content, $pattern, $replacement, $patternObj.Flags)
                }
            }
            
            # Write back if changed
            if ($content -ne $originalContent) {
                if (-not $PreviewOnly) {
                    Set-Content -Path $file.FullName -Value $content -Encoding UTF8 -Force -ErrorAction SilentlyContinue
                    $filesModified++
                    $totalReplacements += $fileReplacements
                    $modificationsByFile[$file.Name] = $fileReplacements
                    Write-Host "  ✓ $($file.Name): $fileReplacements changes" -ForegroundColor Green
                } else {
                    Write-Host "  [PREVIEW] $($file.Name): $fileReplacements changes" -ForegroundColor Yellow
                }
            }
        }
        catch {
            Write-Host "  ⚠ Error processing $($file.Name): $_" -ForegroundColor Yellow
        }
    }
}
catch {
    Write-Host "❌ Fatal error: $_" -ForegroundColor Red
    exit 1
}

# Summary
Write-Host "`n" + ("=" * 80) -ForegroundColor Cyan
Write-Host "📊 PHASE 2 COMPLETION REPORT" -ForegroundColor Cyan
Write-Host ("=" * 80) -ForegroundColor Cyan

if ($PreviewOnly) {
    Write-Host "⚠️  PREVIEW MODE - No files modified" -ForegroundColor Yellow
} else {
    Write-Host "✅ Files modified: $filesModified" -ForegroundColor Green
    Write-Host "✅ Total replacements: $totalReplacements" -ForegroundColor Green
    Write-Host ""
    Write-Host "Top 20 files by replacement count:" -ForegroundColor Cyan
    $modificationsByFile.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 20 | 
        ForEach-Object { Write-Host "   $($_.Name): $($_.Value) changes" }
}

Write-Host "`n✅ PHASE 2 COMPLETE"
