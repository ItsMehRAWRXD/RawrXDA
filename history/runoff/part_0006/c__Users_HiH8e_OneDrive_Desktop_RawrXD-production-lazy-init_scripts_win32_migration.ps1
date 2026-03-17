param(
    [string]$SourceRoot = "D:\\temp\\RawrXD-agentic-ide-production",
    [string]$DestRoot = "$PSScriptRoot/..",
    [switch]$Force,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

function Ensure-Dir($path) {
    if (-not (Test-Path -LiteralPath $path)) {
        New-Item -ItemType Directory -Force -Path $path | Out-Null
    }
}

function Copy-ItemSafe($src, $dst) {
    $dstDir = Split-Path -Parent $dst
    Ensure-Dir $dstDir
    if (Test-Path -LiteralPath $dst) {
        if ($Force) {
            $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
            $bak = "$dst.$stamp.bak"
            Write-Host "Backing up existing: $dst -> $bak" -ForegroundColor Yellow
            if (-not $DryRun) { Copy-Item -LiteralPath $dst -Destination $bak -Force }
        } else {
            Write-Host "Skip (exists): $dst" -ForegroundColor DarkYellow
            return
        }
    }
    Write-Host "Copy: $src -> $dst" -ForegroundColor Cyan
    if (-not $DryRun) { Copy-Item -LiteralPath $src -Destination $dst -Force }
}

# Map of source->destination relative paths
$map = @{
    # Sources (Qt-free Win32 UI and core glue)
    'src\native_ui.cpp'              = 'src\win32\native_ui.cpp'
    'src\native_widgets.cpp'         = 'src\win32\native_widgets.cpp'
    'src\native_layout.cpp'          = 'src\win32\native_layout.cpp'
    'src\native_file_tree.cpp'       = 'src\win32\native_file_tree.cpp'
    'src\native_file_dialog.cpp'     = 'src\win32\native_file_dialog.cpp'
    'src\native_paint_canvas.cpp'    = 'src\win32\native_paint_canvas.cpp'
    'src\win32_ui.cpp'               = 'src\win32\win32_ui.cpp'
    'src\windows_main.cpp'           = 'src\win32\windows_main.cpp'
    'src\noqt_ide_app.cpp'           = 'src\win32\noqt_ide_app.cpp'
    'src\noqt_ide_main.cpp'          = 'src\win32\noqt_ide_main.cpp'
    'src\model_system.cpp'           = 'src\win32\model_system.cpp'
    'src\editor_agent_integration_win32.cpp' = 'src\win32\editor_agent_integration_win32.cpp'
    'src\production_agentic_ide.cpp' = 'src\win32\production_agentic_ide.cpp'
    'src\cross_platform_terminal.cpp'= 'src\win32\cross_platform_terminal.cpp'
    # Headers (namespaced under include/win32 to avoid collisions)
    'include\native_ui.h'            = 'include\win32\native_ui.h'
    'include\native_widgets.h'       = 'include\win32\native_widgets.h'
    'include\native_layout.h'        = 'include\win32\native_layout.h'
    'include\native_file_tree.h'     = 'include\win32\native_file_tree.h'
    'include\native_file_dialog.h'   = 'include\win32\native_file_dialog.h'
    'include\native_paint_canvas.h'  = 'include\win32\native_paint_canvas.h'
    'include\windows_gui_framework.h'= 'include\win32\windows_gui_framework.h'
    'include\win32_ui.h'             = 'include\win32\win32_ui.h'
    'include\model_system.h'         = 'include\win32\model_system.h'
    'include\editor_agent_integration_win32.h' = 'include\win32\editor_agent_integration_win32.h'
    'include\production_agentic_ide.h'= 'include\win32\production_agentic_ide.h'
}

# Optional orchestration bridge (Node + PowerShell) — copied into extensions
$extraDirs = @{
    'cursor-ai-copilot-extension-win32\orchestration' = 'extensions\win32-orchestration'
}

Write-Host "=== Win32 IDE Migration ===" -ForegroundColor Green
Write-Host "Source: $SourceRoot" -ForegroundColor Gray
Write-Host "Dest  : $DestRoot" -ForegroundColor Gray

foreach ($k in $map.Keys) {
    $src = Join-Path -Path $SourceRoot -ChildPath $k
    $dst = Join-Path -Path $DestRoot -ChildPath $map[$k]
    if (Test-Path -LiteralPath $src) {
        Copy-ItemSafe $src $dst
    } else {
        Write-Host "Missing source, skipping: $src" -ForegroundColor DarkRed
    }
}

foreach ($d in $extraDirs.Keys) {
    $srcDir = Join-Path -Path $SourceRoot -ChildPath $d
    $dstDir = Join-Path -Path $DestRoot -ChildPath $extraDirs[$d]
    if (Test-Path -LiteralPath $srcDir) {
        Write-Host "Mirror dir: $srcDir -> $dstDir" -ForegroundColor Cyan
        if (-not $DryRun) {
            Ensure-Dir $dstDir
            robocopy $srcDir $dstDir /E /NFL /NDL /NJH /NJS /NC /NS | Out-Null
        }
    } else {
        Write-Host "Missing dir, skipping: $srcDir" -ForegroundColor DarkRed
    }
}

Write-Host "Done. If BUILD_WIN32_IDE=ON, run CMake to build AgenticIDEWin." -ForegroundColor Green
