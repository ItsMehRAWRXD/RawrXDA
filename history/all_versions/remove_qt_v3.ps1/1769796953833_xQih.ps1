$srcDir = "D:\rawrxd\src"
$files = Get-ChildItem -Path $srcDir -Recurse -Include *.cpp, *.h, *.hpp | Where-Object { $_.FullName -notmatch "_noqt" }

$mappings = @{
    'QJsonObject' = 'void*'
    'QJsonArray' = 'void*'
    'QJsonValue' = 'void*'
    'QJsonDocument' = 'void*'
    'QtConcurrent::run' = '[](auto f){f();}'
    'qDebug()' = '// qDebug()'
    'qWarning()' = '// qWarning()'
    'qCritical()' = '// qCritical()'
    'qInfo()' = '// qInfo()'
    'qDebug' = '// qDebug'
    'qWarning' = '// qWarning'
    'qCritical' = '// qCritical'
    'qInfo' = '// qInfo'
    'QThread::sleep' = 'std::this_thread::sleep_for'
    'QThread::msleep' = 'std::this_thread::sleep_for(std::chrono::milliseconds'
    'QTcpSocket' = 'void*'
    'QTcpServer' = 'void*'
    'QLocalSocket' = 'void*'
    'QLocalServer' = 'void*'
    'QUdpSocket' = 'void*'
    'QNetworkAccessManager' = 'void*'
    'QNetworkRequest' = 'void*'
    'QNetworkReply' = 'void*'
    'QSettings' = 'void*'
    'QProcess' = 'void*'
    'QTimer' = 'void*'
    'QFile' = 'void*'
    'QDir' = 'void*'
    'QBuffer' = 'void*'
    'QUrl' = 'std::string'
    'Qt::' = ''
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

    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8
        Write-Host "Modified: $($file.FullName) ($fileReplacements replacements)"
        $totalModified++
        $totalReplacements += $fileReplacements
    }
}

Write-Host "`nTotal files modified: $totalModified"
Write-Host "Total replacements: $totalReplacements"
