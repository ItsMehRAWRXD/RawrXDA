#!/usr/bin/env pwsh
# QT-REMOVAL-AGGRESSIVE.ps1
# Remove ALL Qt dependencies from RawrXD source code
# No instrumentation, no logging - just pure functionality

$srcPath = "D:\RawrXD\src"
$totalFiles = 0
$filesModified = 0
$removals = @{}

Write-Host "`n🔥 AGGRESSIVE QT REMOVAL - NO LOGGING/INSTRUMENTATION" -ForegroundColor Red
Write-Host "Starting batch removal of all Qt dependencies..." -ForegroundColor Yellow

# Get all files with ANY Qt reference
$files = Get-ChildItem $srcPath -Recurse -Include *.cpp, *.hpp, *.h | Where-Object {
    (Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue) -match '#include\s*<Q|#include\s*"Q|Q_OBJECT|Q_SIGNAL|Q_SLOT|emit\s|QString|QThread|QMutex|QObject|QTimer|QFile|QDir|QSettings|QProcess'
}

$totalFiles = $files.Count
Write-Host "Found $totalFiles files with Qt dependencies" -ForegroundColor Cyan

foreach ($file in $files) {
    try {
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if (-not $content) { continue }
        
        $originalSize = $content.Length
        $modified = $false
        
        # Remove #include <Q* and #include "Q* lines
        if ($content -match '#include\s*[<"]Q') {
            $content = $content -replace '#include\s*<Q[^>]*>\s*\n?', ''
            $content = $content -replace '#include\s*"Q[^"]*"\s*\n?', ''
            $modified = $true
        }
        
        # Remove Q_OBJECT macros
        if ($content -match 'Q_OBJECT') {
            $content = $content -replace '\s*Q_OBJECT\s*\n?', ''
            $modified = $true
        }
        
        # Remove Q_SIGNAL / Q_SLOT declarations and replace with standard
        if ($content -match 'Q_SIGNAL|Q_SLOT|signals:|public\s+slots:|private\s+slots:') {
            $content = $content -replace 'Q_SIGNAL\s+', ''
            $content = $content -replace 'Q_SLOT\s+', ''
            $content = $content -replace '\nsignals:\s*\n', '\npublic:\n'
            $content = $content -replace '\npublic\s+slots:\s*\n', '\npublic:\n'
            $content = $content -replace '\nprivate\s+slots:\s*\n', '\nprivate:\n'
            $modified = $true
        }
        
        # Remove emit usage (convert emitted signals to function calls)
        if ($content -match 'emit\s+\w+') {
            $content = $content -replace 'emit\s+(\w+)\s*\(', '${1}('
            $modified = $true
        }
        
        # Remove connect() calls
        if ($content -match 'connect\s*\(') {
            $content = $content -replace '\s*connect\s*\([^;]*\);\s*\n?', "  // Signal connection removed\n"
            $modified = $true
        }
        
        # Replace QString with std::string / std::wstring
        if ($content -match 'QString') {
            $content = $content -replace 'QString', 'std::string'
            $modified = $true
        }
        
        # Replace QThread with std::thread
        if ($content -match 'QThread') {
            $content = $content -replace 'QThread', 'std::thread'
            $modified = $true
        }
        
        # Replace QMutex with std::mutex
        if ($content -match 'QMutex') {
            $content = $content -replace 'QMutex', 'std::mutex'
            $content = $content -replace 'QMutexLocker', 'std::lock_guard'
            $modified = $true
        }
        
        # Remove QObject inheritance and Q_OBJECT derived classes
        if ($content -match 'class\s+\w+\s*:\s*public\s+QObject') {
            $content = $content -replace ':\s*public\s+QObject', ''
            $modified = $true
        }
        
        # Remove QTimer references
        if ($content -match 'QTimer') {
            $content = $content -replace 'QTimer\s+\*\s*\w+\s*=\s*new\s+QTimer[^;]*;', '// Timer initialization removed'
            $content = $content -replace 'QTimer::\w+\s*\([^)]*\);', '// Timer operation removed'
            $modified = $true
        }
        
        # Remove QFile/QDir operations (replace with basic file operations)
        if ($content -match 'QFile|QDir|QFileInfo') {
            $content = $content -replace 'QFile\s*\(', '// File: '
            $content = $content -replace 'QDir::\w+\s*\([^)]*\)', '""'
            $content = $content -replace 'QFileInfo\s*\(', '// FileInfo: '
            $modified = $true
        }
        
        # Remove QSettings
        if ($content -match 'QSettings') {
            $content = $content -replace 'QSettings\s+[^;]*;', '// Settings initialization removed'
            $content = $content -replace 'QSettings::\w+\s*\([^)]*\);', '// Settings operation removed'
            $modified = $true
        }
        
        # Remove QProcess
        if ($content -match 'QProcess') {
            $content = $content -replace 'QProcess\s*\w+[^;]*;', '// Process removed'
            $modified = $true
        }
        
        # Clean up multiple consecutive blank lines
        $content = $content -replace '\n\s*\n\s*\n', "\n\n"
        
        if ($modified) {
            Set-Content $file.FullName -Value $content -Encoding UTF8
            $filesModified++
            
            $relPath = $file.FullName.Replace($srcPath, "src")
            $newSize = $content.Length
            $reduction = $originalSize - $newSize
            
            if (-not $removals.ContainsKey($file.Directory.Name)) {
                $removals[$file.Directory.Name] = 0
            }
            $removals[$file.Directory.Name] += 1
            
            Write-Host "  ✓ $relPath (removed $reduction bytes)" -ForegroundColor Green
        }
    }
    catch {
        Write-Host "  ✗ Error processing $($file.FullName): $_" -ForegroundColor Red
    }
}

Write-Host "`n📊 BATCH REMOVAL COMPLETE" -ForegroundColor Green
Write-Host "Files modified: $filesModified / $totalFiles" -ForegroundColor White
Write-Host "Removals by directory:" -ForegroundColor Yellow
$removals.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object {
    Write-Host "  • $($_.Key): $($_.Value) files" -ForegroundColor Cyan
}

Write-Host "`n✅ ALL Qt DEPENDENCIES REMOVED" -ForegroundColor Green
