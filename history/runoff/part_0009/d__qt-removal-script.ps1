# Qt Removal Batch Processing Script
# This script helps remove Qt dependencies from C++ source files

param(
    [string]$InputFile = "",
    [string]$OutputFile = "",
    [switch]$Interactive = $false,
    [switch]$DryRun = $true
)

# Mapping of Qt types to std C++ equivalents
$qtReplacements = @{
    # String types
    'QString' = 'std::string'
    
    # Container types
    'QList<' = 'std::vector<'
    'QVector<' = 'std::vector<'
    'QMap<' = 'std::map<'
    'QHash<' = 'std::map<'
    'QStringList' = 'std::vector<std::string>'
    'QByteArray' = 'std::vector<uint8_t>'
    
    # Thread/Sync types
    'QMutex' = 'std::mutex'
    'QMutexLocker' = 'std::lock_guard<std::mutex>'
    'QThread' = 'std::thread'
    'QSemaphore' = 'std::semaphore'
    'QReadWriteLock' = 'std::shared_mutex'
    
    # JSON types - will be replaced with nlohmann/json
    'QJsonObject' = 'nlohmann::json'
    'QJsonArray' = 'nlohmann::json'
    'QJsonDocument' = 'nlohmann::json'
    'QJsonValue' = 'nlohmann::json::value_t'
    
    # Core types
    'QObject' = 'class'
    'QCoreApplication' = 'void'
    'QApplication' = 'void'
    
    # File/Path types
    'QFile' = 'std::ifstream/std::ofstream'
    'QDir' = 'std::filesystem::path'
    'QStandardPaths' = 'std::filesystem::path'
    'QFileInfo' = 'std::filesystem::path'
    
    # Timer/Time types
    'QTimer' = 'std::chrono'
    'QElapsedTimer' = 'std::chrono::high_resolution_clock'
    'QDateTime' = 'std::chrono::system_clock'
    'QTime' = 'std::chrono::duration'
    
    # Process/System types
    'QProcess' = 'std::system/subprocess'
    'QProcessEnvironment' = 'std::getenv'
    
    # UI types (remove entirely if no GUI needed)
    'QWidget' = 'REMOVE_GUI_ELEMENT'
    'QMainWindow' = 'REMOVE_GUI_ELEMENT'
    'QDialog' = 'REMOVE_GUI_ELEMENT'
    'QTextEdit' = 'REMOVE_GUI_ELEMENT'
    'QLineEdit' = 'REMOVE_GUI_ELEMENT'
    'QLabel' = 'REMOVE_GUI_ELEMENT'
    'QPushButton' = 'REMOVE_GUI_ELEMENT'
    'QVBoxLayout' = 'REMOVE_GUI_ELEMENT'
    'QHBoxLayout' = 'REMOVE_GUI_ELEMENT'
    'QApplication' = 'REMOVE_GUI_ELEMENT'
    'QMessageBox' = 'REMOVE_GUI_ELEMENT'
    'QInputDialog' = 'REMOVE_GUI_ELEMENT'
    'QClipboard' = 'REMOVE_GUI_ELEMENT'
    'QPlainTextEdit' = 'REMOVE_GUI_ELEMENT'
}

# Logging replacements to remove
$loggingPatterns = @(
    'qDebug\(\s*\).*<<',
    'qWarning\(\s*\).*<<',
    'qCritical\(\s*\).*<<',
    'qInfo\(\s*\).*<<',
    'qFatal\(\s*\).*<<'
)

# Qt includes to remove
$qtIncludes = @(
    '#include <QCoreApplication>',
    '#include <QApplication>',
    '#include <QString>',
    '#include <QDebug>',
    '#include <QJsonObject>',
    '#include <QJsonArray>',
    '#include <QJsonDocument>',
    '#include <QMutex>',
    '#include <QThread>',
    '#include <QFile>',
    '#include <QDir>',
    '#include <QTimer>',
    '#include <QProcess>',
    '#include <QProcessEnvironment>',
    '#include <QtConcurrent>',
    '#include <QTest>',
    '#include <QSignalSpy>',
    '#include <QWidget>',
    '#include <QMainWindow>',
    '#include <QDialog>',
    '#include <QTextEdit>',
    '#include <QLineEdit>',
    '#include <QLabel>',
    '#include <QPushButton>',
    '#include <QVBoxLayout>',
    '#include <QHBoxLayout>',
    '#include <QMessageBox>',
    '#include <QInputDialog>',
    '#include <QClipboard>'
)

function Remove-QtIncludes {
    param(
        [string]$FilePath
    )
    
    $content = Get-Content -Path $FilePath -Raw -ErrorAction Stop
    $originalContent = $content
    
    foreach ($include in $qtIncludes) {
        $content = $content -replace [regex]::Escape($include + "`n"), ""
        $content = $content -replace [regex]::Escape($include), ""
    }
    
    # Remove blank line sequences created by include removal
    $content = $content -replace '\n\s*\n\s*\n\s*\n', "`n`n`n"
    
    return $content
}

function Remove-Logging {
    param(
        [string]$Content
    )
    
    foreach ($pattern in $loggingPatterns) {
        # Simple approach: remove entire lines with logging
        $Content = $Content -replace ".*$pattern.*`n", ""
        $Content = $Content -replace ".*$pattern.*", ""
    }
    
    return $Content
}

function Replace-QtTypes {
    param(
        [string]$Content
    )
    
    foreach ($key in $qtReplacements.Keys) {
        $value = $qtReplacements[$key]
        
        # Use regex to avoid partial replacements
        $pattern = "\b" + [regex]::Escape($key) + "\b"
        $Content = $Content -replace $pattern, $value
    }
    
    return $Content
}

function Add-StandardIncludes {
    param(
        [string]$Content
    )
    
    $stdIncludes = @(
        '#include <string>',
        '#include <vector>',
        '#include <map>',
        '#include <memory>',
        '#include <functional>',
        '#include <thread>',
        '#include <mutex>',
        '#include <filesystem>',
        '#include <nlohmann/json.hpp>'
    )
    
    $includes = ""
    foreach ($inc in $stdIncludes) {
        if ($Content -match [regex]::Escape($inc)) {
            continue
        }
        $includes += "$inc`n"
    }
    
    # Find insertion point after existing #include statements
    if ($Content -match '(#include.*\n)+') {
        $insertPos = $Matches[0].Length
        $Content = $Content.Insert($insertPos, "`n$includes")
    }
    
    return $Content
}

# Main processing
if ($InputFile -and (Test-Path $InputFile)) {
    Write-Host "Processing: $InputFile" -ForegroundColor Cyan
    
    $content = Get-Content -Path $InputFile -Raw
    
    # Step 1: Remove Qt includes
    $content = Remove-QtIncludes $InputFile
    
    # Step 2: Remove logging
    $content = Remove-Logging $content
    
    # Step 3: Add standard includes
    $content = Add-StandardIncludes $content
    
    # Step 4: Replace Qt types (with caution)
    $content = Replace-QtTypes $content
    
    if ($DryRun) {
        Write-Host "`n--- PREVIEW OF CHANGES (Dry Run) ---`n" -ForegroundColor Yellow
        $content | Select-Object -First 50
    }
    else {
        if ($OutputFile) {
            Set-Content -Path $OutputFile -Value $content
            Write-Host "✓ Saved to: $OutputFile" -ForegroundColor Green
        }
        else {
            Set-Content -Path $InputFile -Value $content
            Write-Host "✓ Updated original file: $InputFile" -ForegroundColor Green
        }
    }
}
else {
    Write-Host "Usage: .\qt-removal.ps1 -InputFile <path> [-OutputFile <path>] [-DryRun]" -ForegroundColor Yellow
}
