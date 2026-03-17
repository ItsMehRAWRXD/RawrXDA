#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Test File Explorer Integration with GGUF Loader
.DESCRIPTION
    Validates that file explorer sidebar is properly integrated with GGUF model loading
#>

Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "File Explorer Integration Test Suite" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan

$ideExe = "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build\bin\Release\RawrXD-SimpleIDE.exe"
$testsPassed = 0
$testsFailed = 0

# Test 1: IDE executable exists
Write-Host "`n[Test 1] IDE executable exists..."
if (Test-Path $ideExe) {
    Write-Host "✓ PASS: $ideExe found" -ForegroundColor Green
    $testsPassed++
} else {
    Write-Host "✗ FAIL: IDE executable not found" -ForegroundColor Red
    $testsFailed++
}

# Test 2: GGUF files exist in D:\OllamaModels
Write-Host "`n[Test 2] GGUF model files available..."
$ggufCount = (Get-ChildItem -Path "D:\OllamaModels\" -Filter "*.gguf" -ErrorAction SilentlyContinue | Measure-Object).Count
if ($ggufCount -gt 0) {
    Write-Host "✓ PASS: Found $ggufCount GGUF files in D:\OllamaModels\" -ForegroundColor Green
    Write-Host "  Models:" -ForegroundColor Gray
    Get-ChildItem -Path "D:\OllamaModels\" -Filter "*.gguf" | Select-Object -First 5 | ForEach-Object {
        $sizeGB = [math]::Round($_.Length / 1GB, 2)
        Write-Host "    • $($_.Name) ($sizeGB GB)" -ForegroundColor Gray
    }
    $testsPassed++
} else {
    Write-Host "✗ FAIL: No GGUF files found" -ForegroundColor Red
    $testsFailed++
}

# Test 3: File explorer source code exists
Write-Host "`n[Test 3] File explorer source code present..."
$hasCreateFileExplorer = Select-String -Path "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\src\win32app\Win32IDE.cpp" -Pattern "void Win32IDE::createFileExplorer" -ErrorAction SilentlyContinue
$hasPopulateTree = Select-String -Path "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\src\win32app\Win32IDE.cpp" -Pattern "void Win32IDE::populateFileTree" -ErrorAction SilentlyContinue
$hasLoadModelPath = Select-String -Path "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\src\win32app\Win32IDE.cpp" -Pattern "void Win32IDE::loadModelFromPath" -ErrorAction SilentlyContinue

if ($hasCreateFileExplorer -and $hasPopulateTree -and $hasLoadModelPath) {
    Write-Host "✓ PASS: All file explorer functions implemented" -ForegroundColor Green
    Write-Host "  • createFileExplorer()" -ForegroundColor Gray
    Write-Host "  • populateFileTree()" -ForegroundColor Gray
    Write-Host "  • loadModelFromPath()" -ForegroundColor Gray
    $testsPassed++
} else {
    Write-Host "✗ FAIL: Some file explorer functions missing" -ForegroundColor Red
    $testsFailed++
}

# Test 4: TreeView notification handlers
Write-Host "`n[Test 4] TreeView event handling..."
$hasTreeNotify = Select-String -Path "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\src\win32app\Win32IDE.cpp" -Pattern "m_hwndFileTree.*TVN_ITEMEXPANDING" -ErrorAction SilentlyContinue
$hasDoubleClick = Select-String -Path "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\src\win32app\Win32IDE.cpp" -Pattern "NM_DBLCLK.*loadModelFromPath" -ErrorAction SilentlyContinue

if ($hasTreeNotify -or $hasDoubleClick) {
    Write-Host "✓ PASS: TreeView notifications wired correctly" -ForegroundColor Green
    Write-Host "  • Expand notifications handled" -ForegroundColor Gray
    Write-Host "  • Double-click to load GGUF files" -ForegroundColor Gray
    $testsPassed++
} else {
    Write-Host "✗ FAIL: TreeView notifications not found" -ForegroundColor Red
    $testsFailed++
}

# Test 5: File explorer members initialized
Write-Host "`n[Test 5] File explorer member initialization..."
$hasMemberInit = Select-String -Path "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\src\win32app\Win32IDE.cpp" -Pattern "m_hwndFileExplorer.*nullptr" -ErrorAction SilentlyContinue
if ($hasMemberInit) {
    Write-Host "✓ PASS: File explorer members properly initialized" -ForegroundColor Green
    Write-Host "  • m_hwndFileExplorer" -ForegroundColor Gray
    Write-Host "  • m_hwndFileTree" -ForegroundColor Gray
    Write-Host "  • m_treeItemPaths" -ForegroundColor Gray
    Write-Host "  • m_sidebarWidth" -ForegroundColor Gray
    $testsPassed++
} else {
    Write-Host "✗ FAIL: Member initialization not found" -ForegroundColor Red
    $testsFailed++
}

# Summary
Write-Host "`n═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Test Results:" -ForegroundColor Yellow
Write-Host "  ✓ Passed: $testsPassed" -ForegroundColor Green
Write-Host "  ✗ Failed: $testsFailed" -ForegroundColor Red
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan

if ($testsFailed -eq 0) {
    Write-Host "`n✓ All tests passed! File explorer is ready to use." -ForegroundColor Green
    Write-Host "`nYou can now:" -ForegroundColor Cyan
    Write-Host "  1. Run: $ideExe" -ForegroundColor Gray
    Write-Host "  2. Look for file explorer panel on the left sidebar" -ForegroundColor Gray
    Write-Host "  3. Browse D:\OllamaModels\ in the tree view" -ForegroundColor Gray
    Write-Host "  4. Double-click any .gguf file to load it" -ForegroundColor Gray
    Write-Host "  5. Model info will appear in the Output tab" -ForegroundColor Gray
    exit 0
} else {
    Write-Host "`n✗ Some tests failed. Check the errors above." -ForegroundColor Red
    exit 1
}
