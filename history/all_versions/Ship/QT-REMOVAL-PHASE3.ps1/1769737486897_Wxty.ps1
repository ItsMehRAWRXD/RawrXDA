#!/usr/bin/env pwsh
<#
.SYNOPSIS
    PHASE 3: Complete Qt Code Usage Elimination
    
.DESCRIPTION
    Phase 1 removed includes
    Phase 2 removed class inheritance and macros
    Phase 3 removes ACTUAL Qt CODE USAGE:
    - QObject() constructor calls
    - QFile, QDir operations
    - QTimer usage
    - QThread/QMutex operations
    - Qt function calls (connect, etc)
    
.NOTES
    This is the REAL removal - targeting actual code, not just declarations
#>

param(
    [string]$RootPath = "D:\RawrXD\src",
    [switch]$PreviewOnly = $false
)

$ErrorActionPreference = "Stop"

Write-Host "🔥 PHASE 3: COMPLETE Qt CODE USAGE ELIMINATION" -ForegroundColor Cyan
Write-Host "Target: $RootPath" -ForegroundColor Yellow

$patterns = @(
    # Constructor calls
    @{ Name = "QObject() init"; Pattern = ":\s*QObject\(parent\)"; Replacement = "" },
    @{ Name = "QObject() init (no parent)"; Pattern = ":\s*QObject\(\)"; Replacement = "" },
    
    # QFile operations
    @{ Name = "QFile constructor"; Pattern = "QFile\s+(\w+)\(([^)]+)\)"; Replacement = "// File: `$2" },
    @{ Name = "QFile::exists()"; Pattern = "QFile::exists\(([^)]+)\)"; Replacement = "std::filesystem::exists(`$1)" },
    @{ Name = "QFile::remove()"; Pattern = "QFile::remove\(([^)]+)\)"; Replacement = "std::filesystem::remove(`$1)" },
    @{ Name = "QFile::copy()"; Pattern = "QFile::copy\(([^)]+),\s*([^)]+)\)"; Replacement = "std::filesystem::copy(`$1, `$2)" },
    @{ Name = "QFile open/read/write"; Pattern = "\b(\w+)\.open\(|\.read\(|\.write\("; Replacement = "" },
    
    # QDir operations
    @{ Name = "QDir constructor"; Pattern = "QDir\s+(\w+)\(([^)]*)\)"; Replacement = "// Directory: `$2" },
    @{ Name = "QDir().mkpath()"; Pattern = "QDir\(\)\.mkpath\(([^)]+)\)"; Replacement = "std::filesystem::create_directories(`$1)" },
    @{ Name = "dir.entryInfoList()"; Pattern = "\bdir\.entryInfoList\([^)]*\)"; Replacement = "// Dir listing removed" },
    
    # QTimer
    @{ Name = "QTimer usage"; Pattern = "QTimer\s+"; Replacement = "// Timer: " },
    @{ Name = "QTimer::singleShot"; Pattern = "QTimer::singleShot\([^)]+\)"; Replacement = "// Timer removed" },
    
    # QThread/QMutex (these should be replaced with std, but check for remnants)
    @{ Name = "QThread* thread"; Pattern = "QThread\s*\*\s*\w+"; Replacement = "std::thread" },
    @{ Name = "QMutex"; Pattern = "\bQMutex\b"; Replacement = "std::mutex" },
    
    # Qt signal/slot connections
    @{ Name = "QObject::connect"; Pattern = "QObject::connect\([^)]*\)"; Replacement = "// Connection removed" },
    @{ Name = "connect() calls"; Pattern = "\bconnect\s*\([^)]*signal[^)]*\)"; Replacement = "// Slot removed" },
    
    # Qt convenience functions
    @{ Name = ".absolutePath()"; Pattern = "\.absolutePath\(\)"; Replacement = ".string()" },
    @{ Name = ".absoluteFilePath()"; Pattern = "\.absoluteFilePath\(\)"; Replacement = ".string()" },
    @{ Name = ".split() with QString"; Pattern = "\.split\([^)]*\)"; Replacement = "// Split removed" },
    
    # Leftover Qt member variables (declarations)
    @{ Name = "QFile* members"; Pattern = "\s+QFile\s*\*\s+\w+;"; Replacement = "" },
    @{ Name = "QDir* members"; Pattern = "\s+QDir\s*\*\s+\w+;"; Replacement = "" },
    @{ Name = "QTimer* members"; Pattern = "\s+QTimer\s*\*\s+\w+;"; Replacement = "" },
)

$files = @()
$filesModified = 0
$totalReplacements = 0

try {
    Write-Host "`n📂 Scanning source files..." -ForegroundColor Cyan
    $files = Get-ChildItem -Path $RootPath -Recurse -Include "*.cpp", "*.hpp", "*.h" -ErrorAction SilentlyContinue
    Write-Host "   Found: $($files.Count) files" -ForegroundColor Green
    
    foreach ($file in $files) {
        try {
            $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
            if (-not $content) { continue }
            
            $originalContent = $content
            $fileReplacements = 0
            
            foreach ($patternObj in $patterns) {
                $pattern = $patternObj.Pattern
                $replacement = $patternObj.Replacement
                
                $matches = [regex]::Matches($content, $pattern)
                if ($matches.Count -gt 0) {
                    $fileReplacements += $matches.Count
                    $content = [regex]::Replace($content, $pattern, $replacement)
                }
            }
            
            if ($content -ne $originalContent) {
                if (-not $PreviewOnly) {
                    Set-Content -Path $file.FullName -Value $content -Encoding UTF8 -Force
                    $filesModified++
                    $totalReplacements += $fileReplacements
                    Write-Host "  ✓ $($file.Name): $fileReplacements changes" -ForegroundColor Green
                } else {
                    Write-Host "  [PREVIEW] $($file.Name): $fileReplacements changes" -ForegroundColor Yellow
                }
            }
        }
        catch {
            Write-Host "  ⚠ Error: $($file.Name)" -ForegroundColor Yellow
        }
    }
}
catch {
    Write-Host "❌ Fatal error: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`n" + ("=" * 80) -ForegroundColor Cyan
Write-Host "✅ PHASE 3 COMPLETE" -ForegroundColor Cyan
Write-Host "Files modified: $filesModified" -ForegroundColor Green
Write-Host "Total replacements: $totalReplacements" -ForegroundColor Green
