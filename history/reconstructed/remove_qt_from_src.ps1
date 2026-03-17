$srcDir = "D:\rawrxd\src"
$files = Get-ChildItem -Path $srcDir -Recurse -Include *.cpp, *.h, *.hpp | Where-Object { $_.FullName -notmatch "_noqt" }

$mappings = @{
    'QString' = 'std::string'
    'QVector<' = 'std::vector<'
    'QHash<' = 'std::map<'
    'QList<' = 'std::vector<'
    'QMap<' = 'std::map<'
    'QByteArray' = 'std::vector<uint8_t>'
    'QThread' = 'std::thread'
    'QMutex' = 'std::mutex'
    'QMutexLocker' = 'std::lock_guard<std::mutex>'
    'QVariant' = 'std::any'
    'QSettings' = 'std::map<std::string, std::string>'
    'QPair<' = 'std::pair<'
    'QElapsedTimer' = 'std::chrono::steady_clock::time_point'
    'QDateTime' = 'std::chrono::system_clock::time_point'
    'QTime' = 'std::chrono::system_clock::time_point'
    'QDate' = 'std::chrono::system_clock::time_point'
    'QFile' = 'std::fstream'
    'QDir' = 'std::filesystem::path'
    'QBuffer' = 'std::stringstream'
    'QUrl' = 'std::string'
    'QProcess' = 'void*'
    'qint64' = 'int64_t'
    'quint64' = 'uint64_t'
    'qint32' = 'int32_t'
    'quint32' = 'uint32_t'
    'qint16' = 'int16_t'
    'quint16' = 'uint16_t'
    'qint8' = 'int8_t'
    'quint8' = 'uint8_t'
    'qreal' = 'double'
    'QStringList' = 'std::vector<std::string>'
    'QFileInfo' = 'std::filesystem::path'
    'QIODevice' = 'std::iostream'
    'QTcpSocket' = 'void*'
    'QTcpServer' = 'void*'
    'QRegularExpression' = 'std::regex'
    'QRegExp' = 'std::regex'
    'QObject' = 'void'
    'QWidget' = 'void'
    'QMainWindow' = 'void'
    'QLabel' = 'void'
    'QPushButton' = 'void'
    'QVBoxLayout' = 'void'
    'QHBoxLayout' = 'void'
    'QGridLayout' = 'void'
}

$totalModified = 0
$totalReplacements = 0

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw
    $originalContent = $content
    $fileReplacements = 0

    foreach ($key in $mappings.Keys) {
        $val = $mappings[$key]
        $pattern = [regex]::Escape($key)
        $matches = [regex]::Matches($content, $pattern)
        if ($matches.Count -gt 0) {
            $content = $content -replace $pattern, $val
            $fileReplacements += $matches.Count
        }
    }

    # Also remove Qt includes
    $qtIncludePattern = '(?m)^#include\s+<Q.*?>\s*$'
    $matches = [regex]::Matches($content, $qtIncludePattern)
    if ($matches.Count -gt 0) {
        $content = $content -replace $qtIncludePattern, ''
        $fileReplacements += $matches.Count
    }

    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8
        Write-Host "Modified: $($file.FullName) ($fileReplacements replacements)"
        $totalModified++
        $totalReplacements += $fileReplacements
    }
}

Write-Host "`nTotal files modified: $totalModified"
Write-Host "Total replacements: $totalReplacements"
