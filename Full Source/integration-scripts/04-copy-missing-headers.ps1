# 04-copy-missing-headers.ps1
# Copy missing .hpp header files for integrated production features

param(
    [string]$SourceRoot = "D:\RawrXD-production-lazy-init",
    [string]$TargetRoot = "D:\rawrxd",
    [switch]$DryRun = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD Header Integration Script v1.0" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Missing headers identified from build errors
$headersToCreate = @{
    # AI Digestion System
    "src/qtapp/ai_digestion_engine.hpp" = "src/qtapp/ai_digestion_engine.hpp"
    "src/qtapp/ai_digestion_panel.hpp" = "src/qtapp/ai_digestion_panel.hpp"
    "src/qtapp/ai_metrics_collector.hpp" = "src/qtapp/ai_metrics_collector.hpp"
    "src/qtapp/ai_training_pipeline.hpp" = "src/qtapp/ai_training_pipeline.hpp"
    
    # AI Management
    "src/qtapp/ai_chat_panel_manager.hpp" = "src/qtapp/ai_chat_panel_manager.hpp"
    "src/qtapp/bounded_autonomous_executor.hpp" = "src/qtapp/bounded_autonomous_executor.hpp"
    
    # Metrics & Monitoring  
    "src/qtapp/metrics_dashboard.hpp" = "src/qtapp/metrics_dashboard.hpp"
    "src/qtapp/latency_monitor.hpp" = "src/qtapp/latency_monitor.hpp"
    
    # Code Intelligence
    "src/qtapp/code_completion_provider.hpp" = "src/qtapp/code_completion_provider.hpp"
    "src/qtapp/real_time_refactoring.hpp" = "src/qtapp/real_time_refactoring.hpp"
    "src/qtapp/intelligent_error_analysis.hpp" = "src/qtapp/intelligent_error_analysis.hpp"
    "src/qtapp/syntax_highlighter.hpp" = "src/qtapp/syntax_highlighter.hpp"
    "src/qtapp/code_minimap.hpp" = "src/qtapp/code_minimap.hpp"
    
    # Enterprise Panels
    "src/qtapp/enterprise_tools_panel.hpp" = "src/qtapp/enterprise_tools_panel.hpp"
    "src/qtapp/interpretability_panel_production.hpp" = "src/qtapp/interpretability_panel_production.hpp"
    "src/qtapp/problems_panel.hpp" = "src/qtapp/problems_panel.hpp"
    "src/qtapp/TestExplorerPanel.hpp" = "src/qtapp/TestExplorerPanel.hpp"
    "src/qtapp/DebuggerPanel.hpp" = "src/qtapp/DebuggerPanel.hpp"
    "src/qtapp/discovery_dashboard.hpp" = "src/qtapp/discovery_dashboard.hpp"
    "src/qtapp/blob_converter_panel.hpp" = "src/qtapp/blob_converter_panel.hpp"
    
    # Project Management
    "src/qtapp/project_manager.hpp" = "src/qtapp/project_manager.hpp"
    "src/qtapp/recent_projects_manager.hpp" = "src/qtapp/recent_projects_manager.hpp"
    "src/qtapp/task_runner.hpp" = "src/qtapp/task_runner.hpp"
    "src/qtapp/ThemeManager.hpp" = "src/qtapp/ThemeManager.hpp"
    
    # Build & Debug Integration
    "src/qtapp/build_output_connector.hpp" = "src/qtapp/build_output_connector.hpp"
    "src/qtapp/compiler_interface.hpp" = "src/qtapp/compiler_interface.hpp"
    "src/qtapp/dap_handler.hpp" = "src/qtapp/dap_handler.hpp"
    
    # Utilities
    "src/qtapp/gui_command_menu.hpp" = "src/qtapp/gui_command_menu.hpp"
    "src/qtapp/alert_system.hpp" = "src/qtapp/alert_system.hpp"
    "src/qtapp/language_support_system.hpp" = "src/qtapp/language_support_system.hpp"
    "src/qtapp/gitignore_parser.hpp" = "src/qtapp/gitignore_parser.hpp"
    "src/qtapp/blob_to_gguf_converter.hpp" = "src/qtapp/blob_to_gguf_converter.hpp"
    "src/qtapp/memory_persistence_system.hpp" = "src/qtapp/memory_persistence_system.hpp"
}

$stats = @{
    Total = $headersToCreate.Count
    Copied = 0
    Skipped = 0
    Failed = 0
}

Write-Host "Processing $($stats.Total) header files..." -ForegroundColor Yellow
Write-Host ""

foreach ($entry in $headersToCreate.GetEnumerator()) {
    $srcPath = Join-Path $SourceRoot $entry.Key
    $dstPath = Join-Path $TargetRoot $entry.Value
    
    # Check if source exists
    if (-not (Test-Path $srcPath)) {
        Write-Host "  ⚠ MISSING: $($entry.Key)" -ForegroundColor Yellow
        $stats.Failed++
        continue
    }
    
    # Check if destination already exists
    if (Test-Path $dstPath) {
        Write-Host "  ⏭ SKIP: $($entry.Key) (already exists)" -ForegroundColor Gray
        $stats.Skipped++
        continue
    }
    
    if ($DryRun) {
        Write-Host "  [DRY RUN] Would copy: $($entry.Key)" -ForegroundColor Cyan
        $stats.Copied++
    }
    else {
        try {
            # Ensure target directory exists
            $dstDir = Split-Path $dstPath -Parent
            if (-not (Test-Path $dstDir)) {
                New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
            }
            
            # Copy file
            Copy-Item -Path $srcPath -Destination $dstPath -Force
            Write-Host "  ✓ COPIED: $($entry.Key)" -ForegroundColor Green
            $stats.Copied++
        }
        catch {
            Write-Host "  ✗ ERROR: $($entry.Key) - $($_.Exception.Message)" -ForegroundColor Red
            $stats.Failed++
        }
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Header Integration Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Total:   $($stats.Total)" -ForegroundColor White
Write-Host "  Copied:  $($stats.Copied)" -ForegroundColor Green
Write-Host "  Skipped: $($stats.Skipped)" -ForegroundColor Gray
Write-Host "  Failed:  $($stats.Failed)" -ForegroundColor Red
Write-Host ""

if ($DryRun) {
    Write-Host "DRY RUN MODE - No files were actually copied" -ForegroundColor Yellow
    Write-Host "Run without -DryRun to perform actual copy" -ForegroundColor Yellow
}

# Exit with error count
exit $stats.Failed
