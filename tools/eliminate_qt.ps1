# RawrXD v14.6 Qt Elimination Audit
param([string]$SourceDir = "D:\rawrxd\src")

$qtPatterns = @(
    "QApplication",
    "QMainWindow", 
    "QWidget",
    "QObject",
    "SIGNAL\(",
    "SLOT\(",
    "QWebEngineView",
    "Qt::",
    "#include <Q",
    "qt_metacall",
    "QMAKE",
    "moc_"
)

$violations = @()
$totalFiles = 0

Get-ChildItem -Path $SourceDir -Recurse -Include "*.cpp","*.hpp","*.h" | ForEach-Object {
    $totalFiles++
    $filePath = $_.FullName
    $lines = Get-Content $filePath
    
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        foreach ($pattern in $qtPatterns) {
            if ($line -like "*$pattern*") {
                $violations += [PSCustomObject]@{
                    File = $filePath
                    Pattern = $pattern
                    Line = $i + 1
                    Context = $line.Trim()
                }
                break 
            }
        }
    }
}

Write-Host "=== RawrXD v14.6 Qt Elimination Audit ===" -ForegroundColor Cyan
Write-Host "Files scanned: $totalFiles"
Write-Host "Qt violations: $($violations.Count)"

if ($violations.Count -eq 0) {
    Write-Host "`n✅ ZERO QT DEPENDENCIES CONFIRMED" -ForegroundColor Green
    Write-Host "v14.6 'Zero Bloat' criteria: SATISFIED"
} else {
    Write-Host "`n⚠️  Remaining Qt references detected:" -ForegroundColor Yellow
    $violations | Group-Object Pattern | ForEach-Object {
        Write-Host "  $($_.Name): $($_.Count) occurrences"
    }
    Write-Host "`nTop 10 Violations:"
    $violations | Select-Object -First 10 | ForEach-Object {
        Write-Host "  $($_.File): Line $($_.Line) -> $($_.Context)"
    }
}

# DLL dependency check
Write-Host "`n=== DLL Dependency Check ==="
$exePath = "D:\rawrxd\build\bin\RawrXD-Win32IDE.exe"
if (Test-Path $exePath) {
    if (Get-Command dumpbin -ErrorAction SilentlyContinue) {
        $deps = dumpbin /dependents $exePath | Select-String "Qt6"
        if ($deps) {
            Write-Host "❌ Qt6 DLLs still linked:" -ForegroundColor Red
            $deps | ForEach-Object { Write-Host "  $_" }
        } else {
            Write-Host "✅ No Qt6 DLL dependencies found in header" -ForegroundColor Green
        }
    } else {
        Write-Host "ℹ️  'dumpbin' not found in path. Use VS Developer Prompt for DLL check." -ForegroundColor Gray
    }
} else {
    Write-Host "ℹ️  Executable not found at $exePath - build first." -ForegroundColor Gray
}
