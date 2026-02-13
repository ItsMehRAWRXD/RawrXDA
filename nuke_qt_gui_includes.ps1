$srcDir = "D:\rawrxd\src"
$files = Get-ChildItem -Path $srcDir -Recurse -Include *.cpp, *.h, *.hpp

$qtIncludes = @(
    '#include <QApplication>',
    '#include <QWidget>',
    '#include <QMainWindow>',
    '#include <QDialog>',
    '#include <QTextEdit>',
    '#include <QLineEdit>',
    '#include <QClipboard>',
    '#include <QPushButton>',
    '#include <QLabel>',
    '#include <QVBoxLayout>',
    '#include <QHBoxLayout>',
    '#include <QComboBox>',
    '#include <QCheckBox>',
    '#include <QSpinBox>',
    '#include <QSlider>',
    '#include <QProgressBar>',
    '#include <QTreeView>',
    '#include <QListView>',
    '#include <QTableView>'
)

$totalModified = 0
$totalRemoved = 0

foreach ($file in $files) {
    $content = Get-Content -Path $file.FullName -Raw
    if (-not $content) { continue }
    
    $originalContent = $content
    $fileRemovals = 0
    
    foreach ($include in $qtIncludes) {
        $pattern = [regex]::Escape($include)
        $matches = [regex]::Matches($content, "(?m)^$pattern\s*$")
        if ($matches.Count -gt 0) {
            $content = $content -replace "(?m)^$pattern\s*$", ''
            $fileRemovals += $matches.Count
        }
    }
    
    # Also remove forward declarations like "class QWidget;"
    $content = $content -replace '(?m)^\s*class\s+Q[A-Z]\w+;\s*$', ''
    
    # Remove QApplication usage patterns
    $content = $content -replace 'QApplication::clipboard\(\)', 'nullptr'
    $content = $content -replace 'QApplication::processEvents\(\)', '// processEvents()'
    $content = $content -replace 'QApplication::instance\(\)', 'nullptr'
    
    if ($content -ne $originalContent) {
        Set-Content -Path $file.FullName -Value $content -Encoding UTF8
        Write-Host "Cleaned: $($file.Name) ($fileRemovals removals)"
        $totalModified++
        $totalRemoved += $fileRemovals
    }
}

Write-Host ""
Write-Host "Total files cleaned: $totalModified" -ForegroundColor Green
Write-Host "Total Qt includes removed: $totalRemoved" -ForegroundColor Green
