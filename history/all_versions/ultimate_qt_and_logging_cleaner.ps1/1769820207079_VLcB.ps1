# Ultimate Qt and Logging Cleanup
$sourceRoot = "D:\rawrxd\src"
$files = Get-ChildItem -Path $sourceRoot -Recurse -Include *.cpp,*.h,*.hpp | Where-Object { $_.DirectoryName -notmatch "\\build\\" }

$stats = @{
    Files = 0
    Replacements = 0
}

Write-Host "Starting Ultimate Cleanup..." -ForegroundColor Cyan

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    $originalContent = $content

    # 1. Remove ALL Qt includes
    $content = $content -replace '(?m)^#include\s+<Q[A-Z][^>]*>\s*$', ''

    # 2. Replace common Qt types in signatures/declarations
    $types = @{
        'QString' = 'std::string'
        'QStringList' = 'std::vector<std::string>'
        'QVector' = 'std::vector'
        'QList' = 'std::vector'
        'QHash' = 'std::unordered_map'
        'QMap' = 'std::map'
        'QSet' = 'std::unordered_set'
        'QPair' = 'std::pair'
        'QVariant' = 'std::any'
        'QByteArray' = 'std::vector<uint8_t>'
        'QUrl' = 'std::string'
        'QDateTime' = 'std::chrono::system_clock::time_point'
        'QMutex' = 'std::mutex'
        'QMutexLocker' = 'std::lock_guard<std::mutex>'
        'QThread' = 'std::thread'
        'QFile' = 'std::fstream'
        'QDir' = 'std::filesystem::path'
        'QFileInfo' = 'std::filesystem::path'
        'QJsonObject' = 'void*'
        'QJsonArray' = 'void*'
        'QObject' = 'void'
        'QWidget' = 'void'
        'QMainWindow' = 'void'
        'QDialog' = 'void'
        'QDockWidget' = 'void'
        'QLayout' = 'void'
        'QVBoxLayout' = 'void'
        'QHBoxLayout' = 'void'
        'QGridLayout' = 'void'
        'QTabWidget' = 'void'
        'QStackedWidget' = 'void'
        'QSplitter' = 'void'
        'QScrollArea' = 'void'
        'QFrame' = 'void'
        'QGroupBox' = 'void'
        'QPushButton' = 'void'
        'QLabel' = 'void'
        'QLineEdit' = 'void'
        'QTextEdit' = 'void'
        'QComboBox' = 'void'
        'QProgressBar' = 'void'
        'QToolBar' = 'void'
        'QMenuBar' = 'void'
        'QMenu' = 'void'
        'QAction' = 'void'
        'QStatusBar' = 'void'
        'QApplication' = 'void'
        'QCheckBox' = 'void'
        'QSpinBox' = 'void'
        'QRadioButton' = 'void'
        'QSlider' = 'void'
        'QIcon' = 'std::string'
        'QPixmap' = 'std::string'
        'QColor' = 'uint32_t'
        'QFont' = 'std::string'
        'QTimer' = 'void*'
        'QProcess' = 'void*'
        'QSettings' = 'void*'
        'QCache' = 'std::map'
        'QNetworkRequest' = 'void*'
        'QNetworkReply' = 'void*'
        'QNetworkAccessManager' = 'void*'
        'QTemporaryFile' = 'std::fstream'
        'QReadWriteLock' = 'std::shared_mutex'
        'QBuffer' = 'std::stringstream'
        'QRect' = 'void*'
        'QPoint' = 'void*'
        'QSize' = 'void*'
        'QHostAddress' = 'std::string'
        'QRegExp' = 'std::regex'
        'QRegularExpression' = 'std::regex'
        'QRegularExpressionMatch' = 'std::smatch'
        'QRegularExpressionMatchIterator' = 'std::sregex_iterator'
        'QClipboard' = 'void*'
        'QWaitCondition' = 'std::condition_variable'
        'QElapsedTimer' = 'std::chrono::steady_clock::time_point'
        'QSemaphore' = 'std::counting_semaphore<1024>'
    }

    foreach ($type in $types.Keys) {
        $content = $content -replace "\b$type\b", $types[$type]
    }

    # 3. Replace instantiations of Qt objects with nullptr
    # Matches 'new Q\w+(...)'
    $content = $content -replace 'new\s+Q[A-Z]\w+\([^)]*\)', 'nullptr'
    
    # 4. Remove Qt Macros and signals/slots
    $content = $content -replace '(?m)^\s*Q_OBJECT\s*$', ''
    $content = $content -replace '(?m)^\s*Q_PROPERTY\([^)]+\)\s*$', ''
    $content = $content -replace '(?m)^\s*Q_ENUM\([^)]+\)\s*$', ''
    $content = $content -replace '(?m)^\s*Q_DECLARE_METATYPE\([^)]+\)\s*$', ''
    $content = $content -replace '(?m)^\s*qRegisterMetaType<[^>]+>\([^)]*\);', '// qRegisterMetaType removed'
    $content = $content -replace '\bsignals:\s*$', 'public:'
    $content = $content -replace '\bslots:\s*$', 'public:'
    $content = $content -replace '\bQ_SLOTS\b', 'public'
    $content = $content -replace '\bQ_SIGNALS\b', 'public'
    $content = $content -replace '\bemit\b', ''
    $content = $content -replace '\bconnect\([^;]+\);', '// connect removed'
    $content = $content -replace '\bdisconnect\([^;]+\);', '// disconnect removed'

    # 5. Fix tr() and QStringLiteral
    $content = $content -replace '\w+::tr\("([^"]*)"\)', '"$1"'
    $content = $content -replace 'QStringLiteral\("([^"]*)"\)', '"$1"'
    $content = $content -replace 'QLatin1String\("([^"]*)"\)', '"$1"'

    # 6. Remove remaining instrumentation/logging calls
    $content = $content -replace '(?s)(\w+->)?logger->[^;]+;', ''
    $content = $content -replace '(?s)\bLOG_(INFO|DEBUG|ERROR|WARN|TRACE)\([^;]+\);?', ''
    $content = $content -replace '(?s)\b(GGML|VK)_LOG_(INFO|DEBUG|ERROR|WARN|TRACE)[^;]+;', ''
    $content = $content -replace '(?s)\bfprintf\(stderr, [^;]+\);?', ''
    $content = $content -replace '(?s)\bstd::cerr\s*<<[^;]+;', ''
    $content = $content -replace '(?s)\bstd::cout\s*<<[^;]+;', ''
    
    # Simple check for multiple empty lines
    $content = $content -replace '(?m)^\s*$(\r?\n\s*$)+', "`r`n"

    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -NoNewline
        $stats.Files++
        # Rough estimate of replacements
        $stats.Replacements += 10 
    }
}

Write-Host "Cleanup finished. Modified $($stats.Files) files." -ForegroundColor Green
