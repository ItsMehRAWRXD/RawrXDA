# Complete Qt Removal Script - Replace ALL Qt types with STL
# This script processes all .hpp and .cpp files in D:\rawrxd\src\qtapp

$targetPath = "D:\rawrxd\src\qtapp"

# Define Qt to STL replacements
$replacements = @{
    # Qt String Types
    'QStringList' = 'std::vector<std::string>'
    'QString' = 'std::string'
    'QByteArray' = 'std::vector<uint8_t>'
    'QChar' = 'char'
    
    # Qt Container Types
    'QVector<' = 'std::vector<'
    'QHash<' = 'std::map<'
    'QMap<' = 'std::map<'
    'QList<' = 'std::vector<'
    'QPair<' = 'std::pair<'
    'QSet<' = 'std::set<'
    'QQueue<' = 'std::queue<'
    'QStack<' = 'std::stack<'
    
    # Qt Core Types
    'QVariant' = 'std::any'
    'QObject' = 'std::enable_shared_from_this<void>'
    
    # Qt Threading
    'QThread' = 'std::thread'
    'QMutex' = 'std::mutex'
    'QReadWriteLock' = 'std::shared_mutex'
    'QMutexLocker' = 'std::lock_guard<std::mutex>'
    'QWaitCondition' = 'std::condition_variable'
    'QSemaphore' = 'std::counting_semaphore'
    'QAtomicInt' = 'std::atomic<int>'
    'QAtomicInteger' = 'std::atomic'
    
    # Qt I/O
    'QFile' = 'std::fstream'
    'QDir' = 'std::filesystem::path'
    'QFileInfo' = 'std::filesystem::path'
    'QTextStream' = 'std::stringstream'
    'QDataStream' = 'std::stringstream'
    
    # Qt Time
    'QDateTime' = 'std::chrono::system_clock::time_point'
    'QTime' = 'std::chrono::time_point'
    'QDate' = 'std::chrono::year_month_day'
    'QElapsedTimer' = 'std::chrono::steady_clock'
    
    # Qt Utility
    'QJsonDocument' = 'nlohmann::json'
    'QJsonObject' = 'nlohmann::json'
    'QJsonArray' = 'nlohmann::json'
    'QSettings' = 'std::map<std::string, std::string>'
    'QUrl' = 'std::string'
    'QRegExp' = 'std::regex'
    'QRegularExpression' = 'std::regex'
}

# Files to skip (already converted to _noqt versions)
$skipFiles = @(
    '*_noqt.hpp',
    '*_noqt.cpp',
    'QT_*.md'
)

Write-Host "=== Qt to STL Replacement Script ===" -ForegroundColor Cyan
Write-Host "Target: $targetPath" -ForegroundColor Yellow
Write-Host ""

# Get all files
$allFiles = Get-ChildItem -Path $targetPath -Include "*.hpp","*.cpp" -Recurse | 
    Where-Object { 
        $file = $_
        $skip = $false
        foreach ($pattern in $skipFiles) {
            if ($file.Name -like $pattern) {
                $skip = $true
                break
            }
        }
        -not $skip
    }

Write-Host "Found $($allFiles.Count) files to process" -ForegroundColor Green
Write-Host ""

$processedFiles = 0
$modifiedFiles = 0
$totalReplacements = 0

foreach ($file in $allFiles) {
    $processedFiles++
    $filePath = $file.FullName
    $relativePath = $filePath.Replace("$targetPath\", "")
    
    Write-Progress -Activity "Processing Files" -Status "File $processedFiles of $($allFiles.Count)" `
                   -CurrentOperation $relativePath -PercentComplete (($processedFiles / $allFiles.Count) * 100)
    
    try {
        $content = Get-Content -Path $filePath -Raw -ErrorAction Stop
        $originalContent = $content
        $fileReplacements = 0
        
        # Apply replacements
        foreach ($key in $replacements.Keys) {
            $value = $replacements[$key]
            $pattern = [regex]::Escape($key)
            
            $matches = [regex]::Matches($content, $pattern)
            if ($matches.Count -gt 0) {
                $content = $content -replace $pattern, $value
                $fileReplacements += $matches.Count
            }
        }
        
        # If content changed, save it
        if ($content -ne $originalContent) {
            Set-Content -Path $filePath -Value $content -NoNewline
            $modifiedFiles++
            $totalReplacements += $fileReplacements
            Write-Host "✓ $relativePath ($fileReplacements replacements)" -ForegroundColor Green
        }
    }
    catch {
        Write-Host "✗ Error processing $relativePath : $_" -ForegroundColor Red
    }
}

Write-Progress -Activity "Processing Files" -Completed

Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host "Files processed: $processedFiles"
Write-Host "Files modified: $modifiedFiles" -ForegroundColor Green
Write-Host "Total replacements: $totalReplacements" -ForegroundColor Yellow
Write-Host ""
Write-Host "Qt removal complete! ✨" -ForegroundColor Green
