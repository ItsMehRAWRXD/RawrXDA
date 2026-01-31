#!/usr/bin/env pwsh
# Save as: Scan-QtDependencies.ps1
# Run: .\Scan-QtDependencies.ps1 > qt_migration_report.txt
# Purpose: Scan entire RawrXD codebase for Qt dependencies and categorize by migration priority

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║      RAWRXD QT DEPENDENCY SCANNER & MIGRATION PLANNER          ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Define Qt patterns to search for
$qtPatterns = @(
    '#include\s*<Q[A-Z]',           # Qt headers
    '#include\s*"Q[A-Z]',           # Qt local headers  
    'Q_OBJECT',                      # Qt macro
    'Q_SIGNAL',                      # Qt signals
    'Q_SLOT',                        # Qt slots
    'emit\s+\w+',                    # Qt emit
    'QString',                       # Qt string
    'QWidget|QMainWindow',           # Qt UI
    'QThread|QMutex',                # Qt threading
    'QFile|QDir',                    # Qt file I/O
    'QSettings',                     # Qt settings
    'QProcess',                      # Qt process
    'QTimer',                        # Qt timer
    'QQueue|QVector|QMap',           # Qt containers
    'std::unique_ptr<Q',             # Qt smart pointers
    'Qt::\w+',                       # Qt namespace
    'SIGNAL\s*\(|SLOT\s*\('          # Qt connect syntax
)

$srcPath = "D:\RawrXD\src"
$shipPath = "D:\RawrXD\Ship"

# Scan all source files
Write-Host "📂 Scanning source files..." -ForegroundColor Yellow
$allFiles = Get-ChildItem -Path $srcPath -Recurse -Include *.cpp, *.hpp, *.h -ErrorAction SilentlyContinue
$qtFiles = @()
$totalQtRefs = 0
$filesProcessed = 0

foreach ($file in $allFiles) {
    $filesProcessed++
    if ($filesProcessed % 50 -eq 0) {
        Write-Host "   Processed $filesProcessed files..." -ForegroundColor DarkGray
    }
    
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $fileQtRefs = @()
    $refCount = 0
    
    foreach ($pattern in $qtPatterns) {
        $matches = [regex]::Matches($content, $pattern)
        if ($matches.Count -gt 0) {
            $fileQtRefs += "$pattern ($($matches.Count) refs)"
            $refCount += $matches.Count
        }
    }
    
    if ($refCount -gt 0) {
        $qtFiles += [PSCustomObject]@{
            File = $file.FullName.Replace($srcPath, "src")
            Lines = (Get-Content $file.FullName -ErrorAction SilentlyContinue | Measure-Object -Line).Lines
            QtRefs = $refCount
            Patterns = $fileQtRefs -join "; "
            Category = Categorize-File $file.FullName
            Priority = Calculate-Priority $file.FullName $refCount
        }
        $totalQtRefs += $refCount
    }
}

function Categorize-File($path) {
    if ($path -like "*\qtapp\*") { return "UI_Layer" }
    if ($path -like "*\agentic\*") { return "Agentic_Core" }
    if ($path -like "*\agent\*") { return "Agent_System" }
    if ($path -like "*\utils\*") { return "Utilities" }
    if ($path -like "*\orchestration\*") { return "Orchestration" }
    if ($path -like "*\test*") { return "Tests" }
    if ($path -like "*\ui\*") { return "UI_Components" }
    return "Other"
}

function Calculate-Priority($path, $refs) {
    $basePriority = switch -Wildcard ($path) {
        "*MainWindow.cpp" { 10 }
        "*main_qt.cpp" { 10 }
        "*TerminalWidget.cpp" { 9 }
        "*agentic_executor*" { 9 }
        "*agentic_text_edit*" { 8 }
        "*orchestrat*" { 8 }
        "*plan_*" { 7 }
        "*utils*" { 6 }
        "*test*" { 3 }
        default { 5 }
    }
    return [math]::Min(10, $basePriority + [math]::Floor($refs / 10))
}

# Sort by priority (descending)
$qtFiles = $qtFiles | Sort-Object Priority -Descending

# Generate report
Write-Host ""
Write-Host "📊 SCAN RESULTS:" -ForegroundColor Yellow
Write-Host "Total Files Scanned: $($allFiles.Count)" -ForegroundColor White
Write-Host "Files with Qt Dependencies: $($qtFiles.Count)" -ForegroundColor Red
Write-Host "Total Qt References: $totalQtRefs" -ForegroundColor Red
Write-Host ""

# Category breakdown
Write-Host "📁 BREAKDOWN BY CATEGORY:" -ForegroundColor Yellow
$categories = $qtFiles | Group-Object Category | Sort-Object { $_.Count } -Descending
foreach ($cat in $categories) {
    $color = if ($cat.Name -eq "UI_Layer") { "Red" } elseif ($cat.Name -eq "Agentic_Core") { "Yellow" } else { "White" }
    Write-Host "  $($cat.Name): $($cat.Count) files" -ForegroundColor $color
}
Write-Host ""

# Top 20 highest priority files
Write-Host "🔥 TOP 20 PRIORITY FILES (Migrate First):" -ForegroundColor Magenta
$qtFiles | Select-Object -First 20 | ForEach-Object {
    $status = switch ($_.Priority) {
        10 { "🔴 CRITICAL" }
        9  { "🟠 HIGH" }
        8  { "🟡 MEDIUM-HIGH" }
        default { "🟢 MEDIUM" }
    }
    Write-Host "  [$status] $($_.File) - $($_.QtRefs) Qt refs, $($_.Lines) lines" -ForegroundColor Gray
}
Write-Host ""

# Migration mapping suggestions
Write-Host "🗺️  MIGRATION MAPPINGS (Qt → Win32):" -ForegroundColor Green
$mappings = @{
    "MainWindow.cpp" = "RawrXD_MainWindow_Win32.dll"
    "main_qt.cpp" = "RawrXD_Foundation.dll + RawrXD_IDE.exe"
    "TerminalWidget.cpp" = "RawrXD_TerminalManager_Win32.dll"
    "agentic_executor.cpp" = "RawrXD_Executor.dll"
    "agentic_text_edit.cpp" = "RawrXD_TextEditor_Win32.dll"
    "plan_orchestrator.cpp" = "RawrXD_PlanOrchestrator.dll"
    "qt_directory_manager.*" = "RawrXD_FileManager_Win32.dll"
    "QSettings usage" = "RawrXD_SettingsManager_Win32.dll (Registry)"
    "QProcess usage" = "RawrXD_TerminalManager_Win32.dll (CreateProcessW)"
}

foreach ($map in $mappings.GetEnumerator()) {
    Write-Host "  $($map.Key) → $($map.Value)" -ForegroundColor Cyan
}
Write-Host ""

# Generate TODO checklist
Write-Host "✅ MIGRATION CHECKLIST TEMPLATE:" -ForegroundColor Yellow
Write-Host "For each file, create tracker entry:" -ForegroundColor Gray
$qtFiles | Select-Object -First 10 | ForEach-Object {
    Write-Host ""
    Write-Host "File: $($_.File)" -ForegroundColor White
    Write-Host "  ☐ ① Rewire to Win32 API/DLL export" -ForegroundColor DarkYellow
    Write-Host "  ☐ ② Remove Qt #includes (<Q*, QString, etc.)" -ForegroundColor DarkYellow
    Write-Host "  ☐ ③ Update build target (remove Qt libs, add Win32 .lib)" -ForegroundColor DarkYellow
    Write-Host "  ☐ ④ Verify with Foundation test harness" -ForegroundColor DarkYellow
    Write-Host "  Priority: $($_.Priority)/10 | Est. Effort: $([math]::Ceiling($_.Lines / 100)) hours" -ForegroundColor DarkGray
}
Write-Host ""

# Next actions
Write-Host "🚀 RECOMMENDED NEXT ACTIONS:" -ForegroundColor Green
Write-Host "  1. Start with: src/qtapp/MainWindow.cpp (highest impact)" -ForegroundColor White
Write-Host "  2. Then: src/qtapp/main_qt.cpp (entry point)" -ForegroundColor White
Write-Host "  3. Then: src/agentic_*.cpp (core logic)" -ForegroundColor White
Write-Host "  4. Batch process utils/ (low complexity, high volume)" -ForegroundColor White
Write-Host "  5. Finally: tests/ (validate everything works)" -ForegroundColor White
Write-Host ""
Write-Host "Hint: Get-Content qt_migration_report.txt | Select-String 'CRITICAL|HIGH'" -ForegroundColor DarkGray
Write-Host ""

# Export detailed CSV for tracking
$csvPath = Join-Path $shipPath "qt_migration_detailed.csv"
$qtFiles | Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8 -Force
Write-Host "📋 Detailed report exported to: $csvPath" -ForegroundColor Cyan
Write-Host ""
