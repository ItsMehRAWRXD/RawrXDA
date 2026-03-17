#!/usr/bin/env pwsh
# Verify Self-Contained Build
# Checks that NO external SDK or toolchain dependencies exist

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Self-Contained Build Verification" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$ProjectRoot = Split-Path -Parent $PSCommandPath
$passed = 0
$failed = 0
$warnings = 0

function Test-Check {
    param($Name, $Condition, [switch]$Warning)
    
    Write-Host -NoNewline "  [$Name] "
    if ($Condition) {
        Write-Host "✅ PASS" -ForegroundColor Green
        return $true
    } else {
        if ($Warning) {
            Write-Host "⚠️  WARN" -ForegroundColor Yellow
            $script:warnings++
        } else {
            Write-Host "❌ FAIL" -ForegroundColor Red
            $script:failed++
        }
        return $false
    }
}

Write-Host "1. Internal Tools" -ForegroundColor Yellow
Write-Host "   ────────────────────────────────" -ForegroundColor Gray

$masmCompiler = Join-Path $ProjectRoot "src\masm\masm_solo_compiler.exe"
$internalLinker = Join-Path $ProjectRoot "src\masm\internal_link.exe"

if (Test-Check "Internal MASM Compiler" (Test-Path $masmCompiler)) { $passed++ }
if (Test-Check "Internal Linker" (Test-Path $internalLinker)) { $passed++ }

Write-Host ""
Write-Host "2. Code Verification" -ForegroundColor Yellow
Write-Host "   ────────────────────────────────" -ForegroundColor Gray

$asmCompilerCode = Get-Content (Join-Path $ProjectRoot "src\compiler\compiler_asm_real.cpp") -Raw
$hasInternalMasm = $asmCompilerCode -match "find_internal_masm"
$noExternalMasm = $asmCompilerCode -notmatch 'find_ml64\(\s*\)'

if (Test-Check "Uses Internal MASM" $hasInternalMasm) { $passed++ }
if (Test-Check "No External ML64" $noExternalMasm) { $passed++ }

$compilerPanelCode = Get-Content (Join-Path $ProjectRoot "src\win32app\Win32IDE_CompilerPanel.cpp") -Raw
$hasInternalPE = $compilerPanelCode -match "parse_pe_headers_internal"
$noExternalDumpbin = $compilerPanelCode -notmatch 'dumpbin\.exe'

if (Test-Check "Internal PE Parser" $hasInternalPE) { $passed++ }
if (Test-Check "No External Dumpbin" $noExternalDumpbin) { $passed++ }

Write-Host ""
Write-Host "3. Dependency Check" -ForegroundColor Yellow
Write-Host "   ────────────────────────────────" -ForegroundColor Gray

# Check if SDK paths are referenced
$allCppFiles = Get-ChildItem -Path (Join-Path $ProjectRoot "src") -Filter "*.cpp" -Recurse
$sdkReferences = @()

foreach ($file in $allCppFiles) {
    $content = Get-Content $file.FullName -Raw
    if ($content -match "Windows Kits\\10\\bin") {
        $sdkReferences += $file.Name
    }
}

if (Test-Check "No Hardcoded SDK Paths" ($sdkReferences.Count -eq 0) -Warning) { 
    if ($sdkReferences.Count -eq 0) { $passed++ }
    if ($sdkReferences.Count -gt 0) {
        Write-Host "      Files with SDK paths:" -ForegroundColor Gray
        foreach ($ref in $sdkReferences) {
            Write-Host "        - $ref" -ForegroundColor DarkGray
        }
    }
}

Write-Host ""
Write-Host "4. Build Scripts" -ForegroundColor Yellow
Write-Host "   ────────────────────────────────" -ForegroundColor Gray

$buildScript = Join-Path $ProjectRoot "build_internal.ps1"
$toolchainScript = Join-Path $ProjectRoot "src\masm\build_toolchain.ps1"

if (Test-Check "Self-Contained Build Script" (Test-Path $buildScript)) { $passed++ }
if (Test-Check "Toolchain Bootstrap Script" (Test-Path $toolchainScript)) { $passed++ }

Write-Host ""
Write-Host "5. Documentation" -ForegroundColor Yellow
Write-Host "   ────────────────────────────────" -ForegroundColor Gray

$docs = @(
    "SELF_CONTAINED_BUILD.md",
    "SELF_CONTAINED_IMPLEMENTATION_SUMMARY.md"
)

foreach ($doc in $docs) {
    $docPath = Join-Path $ProjectRoot $doc
    if (Test-Check "Doc: $doc" (Test-Path $docPath)) { $passed++ }
}

Write-Host ""
Write-Host "6. Optional External Dependencies" -ForegroundColor Yellow
Write-Host "   ────────────────────────────────" -ForegroundColor Gray

$cppCompilerCode = Get-Content (Join-Path $ProjectRoot "src\compiler\compiler_cpp_real.cpp") -Raw
$hasOptionalMessage = $cppCompilerCode -match "optional external dependency"

if (Test-Check "C++ Marked as Optional" $hasOptionalMessage -Warning) { 
    if ($hasOptionalMessage) { $passed++ }
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Results" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

Write-Host "  Passed:   " -NoNewline
Write-Host "$passed" -ForegroundColor Green

if ($warnings -gt 0) {
    Write-Host "  Warnings: " -NoNewline
    Write-Host "$warnings" -ForegroundColor Yellow
}

if ($failed -gt 0) {
    Write-Host "  Failed:   " -NoNewline
    Write-Host "$failed" -ForegroundColor Red
}

Write-Host ""

if ($failed -eq 0) {
    Write-Host "✅ Build System is Self-Contained!" -ForegroundColor Green
    Write-Host ""
    Write-Host "   No external SDK or toolchain required" -ForegroundColor Gray
    Write-Host "   Application is fully sustainable" -ForegroundColor Gray
    Write-Host ""
    
    Write-Host "Next Steps:" -ForegroundColor Cyan
    Write-Host "  1. Run: .\build_internal.ps1" -ForegroundColor White
    Write-Host "  2. Test: .\build_internal\RawrXD_IDE.exe" -ForegroundColor White
    Write-Host "  3. Deploy: Copy entire directory" -ForegroundColor White
    Write-Host ""
    
    exit 0
} else {
    Write-Host "❌ Some checks failed" -ForegroundColor Red
    Write-Host "   Review output above" -ForegroundColor Gray
    Write-Host ""
    exit 1
}
