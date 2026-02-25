# 01-copy-missing-files.ps1
# Automated integration script to copy missing production features from old source

param(
    [string]$SourceRoot = "D:\RawrXD-production-lazy-init",
    [string]$TargetRoot = "D:\rawrxd",
    [switch]$DryRun = $false,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD Feature Integration Script v1.0" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Define critical files to copy
$filesToCopy = @{
    # Core AI Systems
    "src/qtapp/ai_chat_panel_manager.cpp" = "src/qtapp/ai_chat_panel_manager.cpp"
    "src/qtapp/ai_chat_panel_manager.h" = "src/qtapp/ai_chat_panel_manager.h"
    "src/qtapp/bounded_autonomous_executor.cpp" = "src/qtapp/bounded_autonomous_executor.cpp"
    "src/qtapp/bounded_autonomous_executor.h" = "src/qtapp/bounded_autonomous_executor.h"
    
    # Build & Compiler Integration
    "src/qtapp/build_output_connector.cpp" = "src/qtapp/build_output_connector.cpp"
    "src/qtapp/build_output_connector.h" = "src/qtapp/build_output_connector.h"
    "src/qtapp/compiler_interface.cpp" = "src/qtapp/compiler_interface.cpp"
    "src/qtapp/compiler_interface.h" = "src/qtapp/compiler_interface.h"
    
    # UI Components
    "src/qtapp/ThemeManager.cpp" = "src/qtapp/ThemeManager.cpp"
    "src/qtapp/ThemeManager.h" = "src/qtapp/ThemeManager.h"
    "src/qtapp/code_minimap.cpp" = "src/qtapp/code_minimap.cpp"
    "src/qtapp/code_minimap.h" = "src/qtapp/code_minimap.h"
    "src/qtapp/syntax_highlighter.cpp" = "src/qtapp/syntax_highlighter.cpp"
    "src/qtapp/syntax_highlighter.h" = "src/qtapp/syntax_highlighter.h"
    "src/qtapp/problems_panel.cpp" = "src/qtapp/problems_panel.cpp"
    "src/qtapp/problems_panel.h" = "src/qtapp/problems_panel.h"
    
    # Project Management
    "src/qtapp/project_manager.cpp" = "src/qtapp/project_manager.cpp"
    "src/qtapp/project_manager.h" = "src/qtapp/project_manager.h"
    "src/qtapp/recent_projects_manager.cpp" = "src/qtapp/recent_projects_manager.cpp"
    "src/qtapp/recent_projects_manager.h" = "src/qtapp/recent_projects_manager.h"
    "src/qtapp/task_runner.cpp" = "src/qtapp/task_runner.cpp"
    "src/qtapp/task_runner.h" = "src/qtapp/task_runner.h"
    
    # Testing & Debugging
    "src/qtapp/TestExplorerPanel.cpp" = "src/qtapp/TestExplorerPanel.cpp"
    "src/qtapp/TestExplorerPanel.h" = "src/qtapp/TestExplorerPanel.h"
    "src/qtapp/DebuggerPanel.cpp" = "src/qtapp/DebuggerPanel.cpp"
    "src/qtapp/DebuggerPanel.h" = "src/qtapp/DebuggerPanel.h"
    "src/qtapp/dap_handler.cpp" = "src/qtapp/dap_handler.cpp"
    "src/qtapp/dap_handler.h" = "src/qtapp/dap_handler.h"
    
    # Advanced Features
    "src/qtapp/gui_command_menu.cpp" = "src/qtapp/gui_command_menu.cpp"
    "src/qtapp/gui_command_menu.h" = "src/qtapp/gui_command_menu.h"
    "src/qtapp/discovery_dashboard.cpp" = "src/qtapp/discovery_dashboard.cpp"
    "src/qtapp/discovery_dashboard.h" = "src/qtapp/discovery_dashboard.h"
    "src/qtapp/alert_system.cpp" = "src/qtapp/alert_system.cpp"
    "src/qtapp/alert_system.h" = "src/qtapp/alert_system.h"
    "src/qtapp/language_support_system.cpp" = "src/qtapp/language_support_system.cpp"
    "src/qtapp/language_support_system.h" = "src/qtapp/language_support_system.h"
    
    # Git Integration
    "src/qtapp/gitignore_parser.cpp" = "src/qtapp/gitignore_parser.cpp"
    "src/qtapp/gitignore_parser.h" = "src/qtapp/gitignore_parser.h"
    
    # Blob Converter
    "src/qtapp/blob_converter_panel.cpp" = "src/qtapp/blob_converter_panel.cpp"
    "src/qtapp/blob_converter_panel.h" = "src/qtapp/blob_converter_panel.h"
    "src/qtapp/blob_to_gguf_converter.cpp" = "src/qtapp/blob_to_gguf_converter.cpp"
    "src/qtapp/blob_to_gguf_converter.h" = "src/qtapp/blob_to_gguf_converter.h"
    
    # Memory & Persistence
    "src/qtapp/memory_persistence_system.cpp" = "src/qtapp/memory_persistence_system.cpp"
    "src/qtapp/memory_persistence_system.h" = "src/qtapp/memory_persistence_system.h"
}

$stats = @{
    Total = $filesToCopy.Count
    Copied = 0
    Skipped = 0
    Failed = 0
}

Write-Host "Processing $($stats.Total) files..." -ForegroundColor Yellow
Write-Host ""

foreach ($entry in $filesToCopy.GetEnumerator()) {
    $srcPath = Join-Path $SourceRoot $entry.Key
    $dstPath = Join-Path $TargetRoot $entry.Value
    
    if ($Verbose) {
        Write-Host "Checking: $($entry.Key)" -ForegroundColor Gray
    }
    
    if (-not (Test-Path $srcPath)) {
        Write-Host "  ✗ Source not found: $($entry.Key)" -ForegroundColor Red
        $stats.Failed++
        continue
    }
    
    if (Test-Path $dstPath) {
        Write-Host "  ⊗ Already exists: $($entry.Key)" -ForegroundColor Yellow
        $stats.Skipped++
        continue
    }
    
    if ($DryRun) {
        Write-Host "  [DRY RUN] Would copy: $($entry.Key)" -ForegroundColor Cyan
        $stats.Copied++
        continue
    }
    
    try {
        $dstDir = Split-Path $dstPath -Parent
        if (-not (Test-Path $dstDir)) {
            New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
        }
        
        Copy-Item -Path $srcPath -Destination $dstPath -Force
        Write-Host "  ✓ Copied: $($entry.Key)" -ForegroundColor Green
        $stats.Copied++
    }
    catch {
        Write-Host "  ✗ Failed: $($entry.Key) - $($_.Exception.Message)" -ForegroundColor Red
        $stats.Failed++
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Summary:" -ForegroundColor Cyan
Write-Host "  Total:   $($stats.Total)" -ForegroundColor White
Write-Host "  Copied:  $($stats.Copied)" -ForegroundColor Green
Write-Host "  Skipped: $($stats.Skipped)" -ForegroundColor Yellow
Write-Host "  Failed:  $($stats.Failed)" -ForegroundColor Red
Write-Host "========================================" -ForegroundColor Cyan

if ($DryRun) {
    Write-Host ""
    Write-Host "DRY RUN COMPLETE - No files were actually copied" -ForegroundColor Magenta
    Write-Host "Run without -DryRun to perform actual copy" -ForegroundColor Magenta
}

exit $stats.Failed
