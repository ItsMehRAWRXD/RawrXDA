# 02-update-cmake.ps1
# Automatically update CMakeLists.txt to include new source files

param(
    [string]$RootDir = "D:\rawrxd",
    [switch]$Backup = $true,
    [switch]$DryRun = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "CMakeLists.txt Integration Script v1.0" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$cmakeFile = Join-Path $RootDir "CMakeLists.txt"

if (-not (Test-Path $cmakeFile)) {
    Write-Host "ERROR: CMakeLists.txt not found at $cmakeFile" -ForegroundColor Red
    exit 1
}

# Backup if requested
if ($Backup -and -not $DryRun) {
    $backupFile = "$cmakeFile.backup_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
    Copy-Item $cmakeFile $backupFile
    Write-Host "✓ Backup created: $backupFile" -ForegroundColor Green
}

# Define new source files to add
$newSources = @(
    # AI Systems
    "src/qtapp/ai_digestion_engine.cpp"
    "src/qtapp/ai_digestion_engine_extractors.cpp"
    "src/qtapp/ai_digestion_panel.cpp"
    "src/qtapp/ai_digestion_panel_impl.cpp"
    "src/qtapp/ai_digestion_widgets.cpp"
    "src/qtapp/ai_metrics_collector.cpp"
    "src/qtapp/ai_training_pipeline.cpp"
    "src/qtapp/ai_workers.cpp"
    "src/qtapp/ai_chat_panel_manager.cpp"
    "src/qtapp/bounded_autonomous_executor.cpp"
    
    # Monitoring
    "src/qtapp/metrics_dashboard.cpp"
    "src/qtapp/latency_monitor.cpp"
    
    # Code Intelligence
    "src/qtapp/code_completion_provider.cpp"
    "src/qtapp/real_time_refactoring.cpp"
    "src/qtapp/intelligent_error_analysis.cpp"
    "src/qtapp/syntax_highlighter.cpp"
    "src/qtapp/code_minimap.cpp"
    
    # Enterprise
    "src/qtapp/enterprise_tools_panel.cpp"
    "src/qtapp/interpretability_panel_production.cpp"
    
    # UI Panels
    "src/qtapp/problems_panel.cpp"
    "src/qtapp/TestExplorerPanel.cpp"
    "src/qtapp/DebuggerPanel.cpp"
    "src/qtapp/discovery_dashboard.cpp"
    "src/qtapp/blob_converter_panel.cpp"
    
    # Project Management
    "src/qtapp/project_manager.cpp"
    "src/qtapp/recent_projects_manager.cpp"
    "src/qtapp/task_runner.cpp"
    "src/qtapp/ThemeManager.cpp"
    
    # Build & Debug
    "src/qtapp/build_output_connector.cpp"
    "src/qtapp/compiler_interface.cpp"
    "src/qtapp/dap_handler.cpp"
    "src/qtapp/gui_command_menu.cpp"
    
    # Utilities
    "src/qtapp/alert_system.cpp"
    "src/qtapp/language_support_system.cpp"
    "src/qtapp/gitignore_parser.cpp"
    "src/qtapp/blob_to_gguf_converter.cpp"
    "src/qtapp/memory_persistence_system.cpp"
)

Write-Host "Reading CMakeLists.txt..." -ForegroundColor Yellow
$content = Get-Content $cmakeFile -Raw

# Find existing source files that actually exist
$existingSources = @()
foreach ($src in $newSources) {
    $fullPath = Join-Path $RootDir $src
    if (Test-Path $fullPath) {
        $existingSources += $src
        Write-Host "  ✓ Found: $src" -ForegroundColor Green
    } else {
        Write-Host "  ⊗ Missing: $src" -ForegroundColor Yellow
    }
}

if ($existingSources.Count -eq 0) {
    Write-Host ""
    Write-Host "No new source files found to add!" -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host "Found $($existingSources.Count) files to integrate" -ForegroundColor Cyan

if ($DryRun) {
    Write-Host ""
    Write-Host "DRY RUN - Would add these files to CMakeLists.txt:" -ForegroundColor Magenta
    $existingSources | ForEach-Object { Write-Host "  - $_" -ForegroundColor Gray }
    exit 0
}

# Create a formatted list of sources
$sourceList = $existingSources | ForEach-Object { "    $_" }
$sourceBlock = $sourceList -join "`n"

Write-Host ""
Write-Host "Adding sources to CMakeLists.txt..." -ForegroundColor Yellow
Write-Host ""
Write-Host "NOTE: Please manually add these files to the appropriate target:" -ForegroundColor Cyan
Write-Host ""
Write-Host $sourceBlock -ForegroundColor Gray
Write-Host ""
Write-Host "Look for 'RawrXD-AgenticIDE' or 'RawrXD-QtShell' source lists in CMakeLists.txt" -ForegroundColor Cyan
Write-Host ""

# Write instructions file
$instructionsFile = Join-Path $RootDir "integration-scripts\cmake-integration-instructions.txt"
$instructions = @"
CMakeLists.txt Integration Instructions
========================================
Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

Add these source files to the appropriate target in CMakeLists.txt:

$($existingSources -join "`n")

Suggested location:
-------------------
Find the section that looks like:

    set(AGENTIC_IDE_SOURCES
        src/qtapp/main_v5.cpp
        src/qtapp/MainWindow_v5.cpp
        ...
    )

And add the new files to this list.

Then rebuild:
-------------
cd D:\rawrxd\build
cmake --build . --config Release

"@

$instructions | Out-File -FilePath $instructionsFile -Encoding UTF8
Write-Host "✓ Instructions saved to: $instructionsFile" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "CMake integration preparation complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
