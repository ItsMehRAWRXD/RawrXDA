# Remove remaining Qt macros and patterns
$filesToFix = @(
    "agent\model_invoker.cpp",
    "agent\release_agent.cpp",
    "agent\self_code.cpp",
    "ggml-vulkan\ggml-vulkan.cpp",
    "qtapp\compliance_logger_TEST2.hpp",
    "qtapp\inference_engine.hpp",
    "qtapp\MainWindow_v5.cpp",
    "qtapp\security_manager.h",
    "qtapp\Subsystems.h",
    "thermal\EnhancedDynamicLoadBalancer.hpp",
    "win32app\ModelConnection.h",
    "agentic_engine.cpp",
    "ide_window.cpp"
)

$sourceRoot = "D:\rawrxd\src"
$totalChanges = 0

Write-Host "════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  REMOVING REMAINING QT MACROS" -ForegroundColor Yellow
Write-Host "════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

foreach ($file in $filesToFix) {
    $filePath = Join-Path $sourceRoot $file
    
    if (-not (Test-Path $filePath)) {
        Write-Host "⚠ File not found: $file" -ForegroundColor Yellow
        continue
    }
    
    $content = Get-Content -Path $filePath -Raw
    if (-not $content) { continue }
    
    $originalLength = $content.Length
    
    # Remove Q_OBJECT, Q_PROPERTY, Q_ENUM, Q_FLAG, Q_GADGET
    $content = $content -replace '(?m)^\s*Q_OBJECT\s*$', ''
    $content = $content -replace '(?m)^\s*Q_PROPERTY\([^)]+\)\s*$', ''
    $content = $content -replace '(?m)^\s*Q_ENUM\([^)]+\)\s*$', ''
    $content = $content -replace '(?m)^\s*Q_FLAG\([^)]+\)\s*$', ''
    $content = $content -replace '(?m)^\s*Q_GADGET\s*$', ''
    
    # Clean up multiple blank lines
    $content = $content -replace '(?m)^\s*$(\r?\n\s*$)+', "`r`n"
    
    if ($content.Length -ne $originalLength) {
        Set-Content -Path $filePath -Value $content -NoNewline
        $changes = $originalLength - $content.Length
        $totalChanges += $changes
        Write-Host "✓ $file - $changes bytes removed" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Total Changes: $totalChanges bytes" -ForegroundColor Green
Write-Host "════════════════════════════════════" -ForegroundColor Cyan
