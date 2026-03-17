# RawrXD Agentic IDE - Enhanced Build Script
# Includes GGUF streaming, performance metrics, and 4K @ 540Hz display support

$ErrorActionPreference = 'Continue'

$masmRoot = 'C:\masm32'
$ml      = Join-Path $masmRoot 'bin\ml.exe'
$link    = Join-Path $masmRoot 'bin\link.exe'
$libPath = Join-Path $masmRoot 'lib'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$include = Join-Path $scriptDir 'include'
$src     = Join-Path $scriptDir 'src'
$out     = Join-Path $scriptDir 'build'

if (-not (Test-Path $masmRoot)) {
    Write-Error "MASM32 not found at $masmRoot"
    exit 1
}

if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

# Enhanced IDE modules with GGUF streaming and performance metrics
$workingFiles = @(
    'masm_main',
    'engine',
    'window',
    'config_manager',
    'menu_system',
    'orchestra',
    'tab_control_fixed',
    'file_tree_working_enhanced',
    'ui_layout_v2',
    'perf_metrics',
    'gguf_stream'
)

$compiledObjs = @()

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "RawrXD Agentic IDE - Enhanced Build" -ForegroundColor Cyan
Write-Host "4K @ 540Hz | GGUF Streaming | Perf Metrics" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

foreach ($f in $workingFiles) {
    Write-Host "Assembling $f.asm..." -ForegroundColor Cyan
    $result = & $ml /c /coff /Cp /nologo /I"$masmRoot\include" /I"$include" /Fo"$out\$f.obj" "$src\$f.asm" 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✓ $f.asm compiled successfully" -ForegroundColor Green
        $compiledObjs += "$out\$f.obj"
    } else {
        Write-Host "  ✗ $f.asm failed" -ForegroundColor Red
        $result | Select-String "error" | ForEach-Object { Write-Host "    $_" -ForegroundColor Yellow }
    }
}

if ($compiledObjs.Count -gt 0) {
    Write-Host "`nLinking $($compiledObjs.Count) object files..." -ForegroundColor Cyan
    & $link /SUBSYSTEM:WINDOWS /OUT:"$out\AgenticIDEWin.exe" /LIBPATH:"$libPath" $compiledObjs `
        kernel32.lib user32.lib gdi32.lib shell32.lib shlwapi.lib comctl32.lib 2>&1 | Select-String "error|warning|Creating"
    
    if ($LASTEXITCODE -eq 0 -and (Test-Path "$out\AgenticIDEWin.exe")) {
        $size = (Get-Item "$out\AgenticIDEWin.exe").Length
        Write-Host "`n✓ Build completed successfully!" -ForegroundColor Green
        Write-Host "  Output: $out\AgenticIDEWin.exe" -ForegroundColor White
        Write-Host "  Size: $([math]::Round($size/1KB, 2)) KB" -ForegroundColor White
        Write-Host "`nFeatures:" -ForegroundColor Cyan
        Write-Host "  • 4K @ 540Hz display support" -ForegroundColor White
        Write-Host "  • GGUF model streaming" -ForegroundColor White
        Write-Host "  • Real-time FPS & bitrate metrics" -ForegroundColor White
        Write-Host "  • Lazy-loading file tree" -ForegroundColor White
    } else {
        Write-Host "`n✗ Link failed" -ForegroundColor Red
    }
} else {
    Write-Host "`nNo object files to link" -ForegroundColor Red
}
