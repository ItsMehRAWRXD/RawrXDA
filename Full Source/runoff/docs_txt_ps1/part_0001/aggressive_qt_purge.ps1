# Aggressive Final Qt Cleanup
$sourceRoot = "D:\rawrxd\src"
$statsTotal = 0
$filesModified = @()

Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   AGGRESSIVE FINAL Qt PURGE" -ForegroundColor Red
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$files = Get-ChildItem -Path $sourceRoot -Recurse -Include *.cpp,*.h,*.hpp | Where-Object { $_.DirectoryName -notmatch "\\build\\" }

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $originalContent = $content
    $lineCount = 0
    
    # Remove ALL Qt includes (even commented ones)
    $before = $content
    $content = $content -replace '(?m)^.*#include\s+<Q[A-Z][^>]+>.*$', ''
    $lineCount += (($before -split "`n").Count - ($content -split "`n").Count)
    
    # Replace Qt types everywhere
    $content = $content -replace '\bQString\b', 'std::string'
    $content = $content -replace '\bQStringList\b', 'std::vector<std::string>'
    $content = $content -replace '\bQVector\b', 'std::vector'
    $content = $content -replace '\bQList\b', 'std::vector'
    $content = $content -replace '\bQHash\b', 'std::unordered_map'
    $content = $content -replace '\bQMap\b', 'std::map'
    $content = $content -replace '\bQSet\b', 'std::unordered_set'
    $content = $content -replace '\bQPair\b', 'std::pair'
    $content = $content -replace '\bQVariant\b', 'std::any'
    $content = $content -replace '\bQByteArray\b', 'std::vector<uint8_t>'
    $content = $content -replace '\bQUrl\b', 'std::string'
    $content = $content -replace '\bQDateTime\b', 'std::chrono::system_clock::time_point'
    $content = $content -replace '\bQTime\b', 'std::chrono::system_clock::time_point'
    $content = $content -replace '\bQDate\b', 'std::chrono::system_clock::time_point'
    $content = $content -replace '\bQMutex\b', 'std::mutex'
    $content = $content -replace '\bQMutexLocker\b', 'std::lock_guard<std::mutex>'
    $content = $content -replace '\bQThread\b', 'std::thread'
    $content = $content -replace '\bQFile\b', 'std::fstream'
    $content = $content -replace '\bQDir\b', 'std::filesystem::path'
    $content = $content -replace '\bQFileInfo\b', 'std::filesystem::path'
    $content = $content -replace '\bQJsonObject\b', 'void*'
    $content = $content -replace '\bQJsonArray\b', 'void*'
    $content = $content -replace '\bQJsonValue\b', 'void*'
    $content = $content -replace '\bQJsonDocument\b', 'void*'
    $content = $content -replace '\bQWidget\b', 'void'
    $content = $content -replace '\bQMainWindow\b', 'void'
    $content = $content -replace '\bQDialog\b', 'void'
    $content = $content -replace '\bQObject\b', 'void'
    $content = $content -replace '\bQTcpSocket\b', 'void*'
    $content = $content -replace '\bQTcpServer\b', 'void*'
    $content = $content -replace '\bQNetworkAccessManager\b', 'void*'
    $content = $content -replace '\bQNetworkReply\b', 'void*'
    $content = $content -replace '\bQEventLoop\b', 'void*'
    $content = $content -replace '\bQTimer\b', 'void*'
    
    # Remove Qt macros
    $before = $content
    $content = $content -replace '(?m)^\s*(Q_OBJECT|Q_PROPERTY|Q_ENUM|Q_FLAG|Q_GADGET|Q_INTERFACES).*$', ''
    $lineCount += (($before -split "`n").Count - ($content -split "`n").Count)
    
    # Remove signals/slots
    $content = $content -replace '\bsignals:\s*$', 'public:'
    $content = $content -replace '\bslots:\s*$', 'public:'
    $content = $content -replace '\bpublic\s+slots:', 'public:'
    $content = $content -replace '\bprivate\s+slots:', 'private:'
    $content = $content -replace '\bprotected\s+slots:', 'protected:'
    
    # Remove Qt:: namespace references
    $content = $content -replace 'Qt::', '//'
    
    # Remove emit keyword
    $content = $content -replace '\bemit\s+', ''
    
    # Remove connect/disconnect calls (Qt-specific)
    $before = $content
    $content = $content -replace '(?m)^\s*connect\([^;]+;?\s*$', '// Qt connect removed'
    $content = $content -replace '(?m)^\s*disconnect\([^;]+;?\s*$', '// Qt disconnect removed'
    $lineCount += (($before -split "`n").Count - ($content -split "`n").Count)
    
    # Remove logger calls
    $before = $content
    $content = $content -replace '(?m)^\s*(logger|m_logger|device->memory_logger|ctx->device->perf_logger)->.*$', ''
    $content = $content -replace '(?m)^\s*LOG_(INFO|DEBUG|ERROR|WARN|TRACE)\(.*\);?\s*$', ''
    $lineCount += (($before -split "`n").Count - ($content -split "`n").Count)
    
    # Clean up multiple blank lines
    $content = $content -replace '(?m)^\s*$(\r?\n\s*$)+', "`r`n"
    
    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -NoNewline
        $statsTotal += $lineCount
        $filesModified += $file.FullName
        if ($lineCount -gt 5) {
            Write-Host "✓ $($file.Name) - $lineCount lines cleaned" -ForegroundColor Green
        }
    }
}

Write-Host ""
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Files Modified: $($filesModified.Count)" -ForegroundColor Green
Write-Host "Lines Removed: ~$statsTotal" -ForegroundColor Green
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
