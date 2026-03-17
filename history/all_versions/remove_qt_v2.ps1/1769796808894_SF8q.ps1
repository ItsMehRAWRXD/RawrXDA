$srcDir = "D:\rawrxd\src"
$files = Get-ChildItem -Path $srcDir -Recurse -Include *.cpp, *.h, *.hpp | Where-Object { $_.FullName -notmatch "_noqt" }

$mappings = @{
    'QTextStream' = 'std::stringstream'
    'QHeaderView' = 'void'
    'QTreeView' = 'void'
    'QStandardItemModel' = 'void'
    'QStandardItem' = 'void'
    'QModelIndex' = 'int'
    'QSortFilterProxyModel' = 'void'
    'QAction' = 'void'
    'QMenu' = 'void'
    'QMenuBar' = 'void'
    'QToolBar' = 'void'
    'QStatusBar' = 'void'
    'QDialog' = 'void'
    'QMessageBox' = 'void'
    'QFileDialog' = 'void'
    'QInputDialog' = 'void'
    'QProgressDialog' = 'void'
    'QColorDialog' = 'void'
    'QFontDialog' = 'void'
    'QTabWidget' = 'void'
    'QStackedWidget' = 'void'
    'QSplitter' = 'void'
    'QScrollArea' = 'void'
    'QGroupBox' = 'void'
    'QFrame' = 'void'
    'QLineEdit' = 'void'
    'QTextEdit' = 'void'
    'QPlainTextEdit' = 'void'
    'QComboBox' = 'void'
    'QSpinBox' = 'void'
    'QDoubleSpinBox' = 'void'
    'QCheckBox' = 'void'
    'QRadioButton' = 'void'
    'QButtonGroup' = 'void'
    'QSlider' = 'void'
    'QProgressBar' = 'void'
    'QLCDNumber' = 'void'
    'QCalendarWidget' = 'void'
    'QCompleter' = 'void'
    'QFileSystemModel' = 'void'
    'QDirModel' = 'void'
    'QIcon' = 'void'
    'QPixmap' = 'void'
    'QImage' = 'void'
    'QPainter' = 'void'
    'QBrush' = 'void'
    'QPen' = 'void'
    'QFont' = 'void'
    'QColor' = 'void'
    'QPalette' = 'void'
    'QCursor' = 'void'
    'QShortcut' = 'void'
    'QKeySequence' = 'void'
    'QClipboard' = 'void'
    'QMimeData' = 'void'
    'QDrag' = 'void'
    'QDropEvent' = 'void'
    'QMouseEvent' = 'void'
    'QKeyEvent' = 'void'
    'QWheelEvent' = 'void'
    'QFocusEvent' = 'void'
    'QResizeEvent' = 'void'
    'QMoveEvent' = 'void'
    'QCloseEvent' = 'void'
    'QHideEvent' = 'void'
    'QShowEvent' = 'void'
    'QPaintEvent' = 'void'
    'QTimerEvent' = 'void'
    'QEvent' = 'void'
    'QPoint' = 'struct { int x; int y; }'
    'QPointF' = 'struct { float x; float y; }'
    'QSize' = 'struct { int w; int h; }'
    'QSizeF' = 'struct { float w; float h; }'
    'QRect' = 'struct { int x; int y; int w; int h; }'
    'QRectF' = 'struct { float x; float y; float w; float h; }'
    'QLine' = 'void'
    'QLineF' = 'void'
    'QPolygon' = 'void'
    'QPolygonF' = 'void'
    'QMatrix' = 'void'
    'QTransform' = 'void'
    'QRegion' = 'void'
    'slot' = ''
    'signals:' = 'public:'
    'slots:' = 'public:'
    'Q_OBJECT' = ''
    'Q_PROPERTY' = '// Q_PROPERTY'
    'Q_ENUM' = '// Q_ENUM'
    'Q_DECLARE_METATYPE' = '// Q_DECLARE_METATYPE'
    'Q_SIGNALS' = 'public:'
    'Q_SLOTS' = 'public:'
    'emit ' = ''
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

    # Remove includes for logging/telemetry/instrumentation
    $loggerIncludes = '(?m)^#include\s+["<].*?(telemetry|logger|metrics|profiler|observability).*?[">]\s*$'
    $matches = [regex]::Matches($content, $loggerIncludes)
    if ($matches.Count -gt 0) {
        $content = $content -replace $loggerIncludes, ''
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
