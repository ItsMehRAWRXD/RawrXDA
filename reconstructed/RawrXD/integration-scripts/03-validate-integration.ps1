# Validate integration - check for missing dependencies and common issues
# Identifies compilation issues before running cmake

param(
    [string]$SrcPath = "D:\rawrxd\src",
    [string]$IncludePath = "D:\rawrxd\include"
)

$ErrorActionPreference = "Continue"
$issues = @()
$warnings = @()

Write-Host "╔════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║ RawrXD Integration Validator                          ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

# Check 1: Verify key files exist
Write-Host "`n[1/5] Checking key integrated files..." -ForegroundColor Cyan
$keyFiles = @(
    "src/qtapp/ai_digestion_engine.cpp",
    "src/qtapp/metrics_dashboard.cpp",
    "src/qtapp/real_time_refactoring.cpp",
    "src/agentic/agentic_engine.cpp",
    "src/lsp_client.cpp",
    "src/RawrXD_LSP_Core.asm",
    "src/RawrXD_LSP_Handshake_Ext.asm"
)

$found = 0
foreach ($file in $keyFiles) {
    $fullPath = Join-Path "D:\rawrxd" $file
    if (Test-Path $fullPath) {
        Write-Host "  ✓ $file" -ForegroundColor Green
        $found++
    }
    else {
        Write-Host "  ✗ $file" -ForegroundColor Red
        $issues += "Missing: $file"
    }
}
Write-Host "  Found: $found / $($keyFiles.Count)" -ForegroundColor Yellow

# Check 2: Verify header files
Write-Host "`n[2/5] Checking critical headers..." -ForegroundColor Cyan
$headerFiles = @(
    "include/lsp_client.h",
    "include/RawrXD_LSP_Loader.h",
    "include/agentic_engine.h",
    "include/agentic_executor.h",
    "include/streaming_inference.hpp",
    "include/inference_engine.hpp"
)

$headerFound = 0
foreach ($header in $headerFiles) {
    $fullPath = Join-Path "D:\rawrxd" $header
    if (Test-Path $fullPath) {
        Write-Host "  ✓ $header" -ForegroundColor Green
        $headerFound++
    }
    else {
        Write-Host "  ⚠ $header (may need creation)" -ForegroundColor Yellow
        $warnings += "Missing header: $header"
    }
}
Write-Host "  Found: $headerFound / $($headerFiles.Count)" -ForegroundColor Yellow

# Check 3: Scan for undefined symbol patterns
Write-Host "`n[3/5] Scanning for common integration issues..." -ForegroundColor Cyan
$cppFiles = Get-ChildItem -Path "$SrcPath\qtapp" -Filter "*.cpp" | Select-Object -First 10

$undefinedCount = 0
foreach ($file in $cppFiles) {
    $content = Get-Content $file.FullName -Raw
    
    # Look for Q_OBJECT outside of class (common Qt moc issue)
    if ($content -match 'Q_OBJECT.*class') {
        Write-Host "  ⚠ Possible Q_OBJECT placement issue in $($file.Name)" -ForegroundColor Yellow
        $warnings += "Q_OBJECT in $($file.Name)"
    }
    
    # Look for unresolved NewtonSoft references (should be nlohmann)
    if ($content -match 'NewtonSoft|Newtonsoft') {
        Write-Host "  ✗ NewtonSoft reference in $($file.Name) (use nlohmann/json)" -ForegroundColor Red
        $issues += "NewtonSoft in $($file.Name)"
    }
}

# Check 4: Verify MASM integration
Write-Host "`n[4/5] Checking MASM integration..." -ForegroundColor Cyan
$masmFiles = Get-ChildItem -Path "$SrcPath" -Filter "*.asm" -ErrorAction SilentlyContinue | Measure-Object
Write-Host "  Found $($masmFiles.Count) MASM files" -ForegroundColor Gray

# Check 5: CMakeLists.txt consistency
Write-Host "`n[5/5] Checking CMakeLists.txt..." -ForegroundColor Cyan
$cmakeContent = Get-Content "D:\rawrxd\CMakeLists.txt" -Raw

$criticalPatterns = @(
    "set(AgenticIDE_SOURCES",
    "add_executable(RawrXD-QtShell",
    "target_link_libraries(RawrXD-QtShell",
    "enable_language(ASM_MASM)"
)

foreach ($pattern in $criticalPatterns) {
    if ($cmakeContent -match [regex]::Escape($pattern)) {
        Write-Host "  ✓ $pattern" -ForegroundColor Green
    }
    else {
        Write-Host "  ✗ Missing: $pattern" -ForegroundColor Red
        $issues += "CMake missing: $pattern"
    }
}

# Summary
Write-Host "`n╔════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║ Validation Summary                                    ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

if ($issues.Count -eq 0 -and $warnings.Count -eq 0) {
    Write-Host "`n✓ Integration looks good! Ready to build." -ForegroundColor Green
    exit 0
}
elseif ($issues.Count -eq 0) {
    Write-Host "`n⚠ $($warnings.Count) warnings found (non-blocking)" -ForegroundColor Yellow
    foreach ($w in $warnings) {
        Write-Host "  • $w" -ForegroundColor Yellow
    }
    exit 0
}
else {
    Write-Host "`n✗ $($issues.Count) critical issues found (blocking)" -ForegroundColor Red
    foreach ($i in $issues) {
        Write-Host "  • $i" -ForegroundColor Red
    }
    exit 1
}
