# Remove all Qt #include directives from source files

$targetPath = "D:\rawrxd\src\qtapp"

# Qt includes to remove
$qtIncludes = @(
    '#include <QObject>',
    '#include <QString>',
    '#include <QStringList>',
    '#include <QByteArray>',
    '#include <QVector>',
    '#include <QList>',
    '#include <QHash>',
    '#include <QMap>',
    '#include <QPair>',
    '#include <QSet>',
    '#include <QQueue>',
    '#include <QStack>',
    '#include <QThread>',
    '#include <QMutex>',
    '#include <QMutexLocker>',
    '#include <QReadWriteLock>',
    '#include <QWaitCondition>',
    '#include <QSemaphore>',
    '#include <QAtomicInt>',
    '#include <QAtomicInteger>',
    '#include <QFile>',
    '#include <QDir>',
    '#include <QFileInfo>',
    '#include <QTextStream>',
    '#include <QDataStream>',
    '#include <QDateTime>',
    '#include <QTime>',
    '#include <QDate>',
    '#include <QElapsedTimer>',
    '#include <QJsonDocument>',
    '#include <QJsonObject>',
    '#include <QJsonArray>',
    '#include <QSettings>',
    '#include <QUrl>',
    '#include <QRegExp>',
    '#include <QRegularExpression>',
    '#include <QVariant>',
    '#include <QChar>',
    '#include <QCoreApplication>',
    '#include <QApplication>',
    '#include <QTimer>',
    '#include <QEventLoop>',
    '#include <QTcpServer>',
    '#include <QTcpSocket>',
    '#include <QUdpSocket>',
    '#include <QNetworkAccessManager>',
    '#include <QDebug>'
)

# STL includes to add if not present
$stdIncludes = @(
    '#include <string>',
    '#include <vector>',
    '#include <map>',
    '#include <unordered_map>',
    '#include <set>',
    '#include <queue>',
    '#include <stack>',
    '#include <memory>',
    '#include <functional>',
    '#include <algorithm>',
    '#include <mutex>',
    '#include <thread>',
    '#include <condition_variable>',
    '#include <atomic>',
    '#include <fstream>',
    '#include <sstream>',
    '#include <chrono>',
    '#include <regex>',
    '#include <any>',
    '#include <filesystem>'
)

Write-Host "=== Qt Include Removal Script ===" -ForegroundColor Cyan
Write-Host ""

$allFiles = Get-ChildItem -Path $targetPath -Include "*.hpp","*.cpp","*.h" -Recurse |
    Where-Object { $_.Name -notlike "*_noqt.*" }

Write-Host "Processing $($allFiles.Count) files..." -ForegroundColor Yellow
Write-Host ""

$modifiedFiles = 0
$totalRemoved = 0

foreach ($file in $allFiles) {
    $filePath = $file.FullName
    $content = Get-Content -Path $filePath -Raw
    $originalContent = $content
    $removed = 0
    
    # Remove Qt includes
    foreach ($include in $qtIncludes) {
        $escapedInclude = [regex]::Escape($include)
        if ($content -match $escapedInclude) {
            $content = $content -replace "$escapedInclude\s*\r?\n", ""
            $removed++
        }
    }
    
    if ($content -ne $originalContent) {
        # Clean up multiple empty lines
        $content = $content -replace "(\r?\n){3,}", "`n`n"
        
        Set-Content -Path $filePath -Value $content -NoNewline
        $modifiedFiles++
        $totalRemoved += $removed
        
        $relativePath = $filePath.Replace("$targetPath\", "")
        Write-Host "✓ $relativePath ($removed includes removed)" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host "Files modified: $modifiedFiles" -ForegroundColor Green
Write-Host "Qt includes removed: $totalRemoved" -ForegroundColor Yellow
Write-Host ""
Write-Host "Qt includes removed! ✨" -ForegroundColor Green
