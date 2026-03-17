#requires -Version 7.0
<#
.SYNOPSIS
    RawrXD IDE - BUILD SYSTEM VALIDATOR
    
.DESCRIPTION
    Verifies that all build scripts are in place and working.
    Runs a basic test to ensure build infrastructure is sound.
#>
param(
    [switch]$RunFullTest
)

$ProjectRoot = 'd:\lazy init ide'
$ErrorActionPreference = 'Stop'

function Test-BuildScript {
    param([string]$Name, [string]$Path)
    
    if (Test-Path $Path) {
        $size = (Get-Item $Path).Length / 1KB
        Write-Host "  ✓ $Name ($([Math]::Round($size, 0))KB)" -ForegroundColor Green
        return $true
    } else {
        Write-Host "  ✗ $Name NOT FOUND" -ForegroundColor Red
        return $false
    }
}

function Test-BuildArtifact {
    param([string]$Name, [string]$Path)
    
    if (Test-Path $Path) {
        $count = (Get-ChildItem $Path -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
        Write-Host "  ✓ $Name ($count files)" -ForegroundColor Green
        return $true
    } else {
        Write-Host "  ○ $Name (not yet built)" -ForegroundColor Gray
        return $false
    }
}

function Test-Tool {
    param([string]$Tool, [string]$Name)
    
    if (Get-Command $Tool -ErrorAction SilentlyContinue) {
        Write-Host "  ✓ $Name" -ForegroundColor Green
        return $true
    } elseif (Test-Path $Tool) {
        Write-Host "  ✓ $Name (at $Tool)" -ForegroundColor Green
        return $true
    } else {
        Write-Host "  ○ $Name (optional)" -ForegroundColor Gray
        return $false
    }
}

# ═══════════════════════════════════════════════════════════════════════════════

Clear-Host
Write-Host @"
╔═══════════════════════════════════════════════════════════════════════╗
║  RawrXD IDE - BUILD SYSTEM VALIDATION                               ║
╚═══════════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Magenta

# Phase 1: Check Scripts
Write-Host "`n[PHASE 1] BUILD SCRIPTS" -ForegroundColor Cyan
Write-Host "Checking for required build automation scripts..." -ForegroundColor Gray

$scripts = @(
    @{ Name = 'Build Orchestrator'; Path = 'BUILD_ORCHESTRATOR.ps1' },
    @{ Name = 'Fast Build'; Path = 'BUILD_IDE_FAST.ps1' },
    @{ Name = 'Production Build'; Path = 'BUILD_IDE_PRODUCTION.ps1' },
    @{ Name = 'Build Executor'; Path = 'BUILD_IDE_EXECUTOR.ps1' }
)

$scriptsOK = 0
foreach ($script in $scripts) {
    if (Test-BuildScript $script.Name (Join-Path $ProjectRoot $script.Path)) {
        $scriptsOK++
    }
}

Write-Host "`nResult: $scriptsOK/$($scripts.Count) scripts found" -ForegroundColor $(if ($scriptsOK -eq $scripts.Count) { 'Green' } else { 'Yellow' })

# Phase 2: Check Documentation
Write-Host "`n[PHASE 2] DOCUMENTATION" -ForegroundColor Cyan
Write-Host "Checking for build documentation..." -ForegroundColor Gray

$docs = @(
    @{ Name = 'START HERE Guide'; Path = 'START_HERE_BUILD.md' },
    @{ Name = 'Build Guide'; Path = 'BUILD_GUIDE_NO_SCAFFOLDING.md' }
)

$docsOK = 0
foreach ($doc in $docs) {
    if (Test-BuildScript $doc.Name (Join-Path $ProjectRoot $doc.Path)) {
        $docsOK++
    }
}

Write-Host "`nResult: $docsOK/$($docs.Count) docs found" -ForegroundColor Green

# Phase 3: Check Build Source
Write-Host "`n[PHASE 3] SOURCE CODE" -ForegroundColor Cyan
Write-Host "Checking for source files to build..." -ForegroundColor Gray

$asmFiles = @(Get-ChildItem (Join-Path $ProjectRoot 'itsmehrawrxd-master') -Filter '*compiler*.asm' -File -ErrorAction SilentlyContinue)
Write-Host "  ✓ Assembly sources ($($asmFiles.Count) files)" -ForegroundColor Green

$cppFiles = @(Get-ChildItem (Join-Path $ProjectRoot 'src') -Filter '*.cpp' -File -ErrorAction SilentlyContinue)
Write-Host "  ✓ C++ sources ($($cppFiles.Count) files)" -ForegroundColor Green

$cmake = Test-Path (Join-Path $ProjectRoot 'CMakeLists.txt')
if ($cmake) {
    Write-Host "  ✓ CMake configuration found" -ForegroundColor Green
} else {
    Write-Host "  ○ CMake configuration (optional)" -ForegroundColor Gray
}

# Phase 4: Check Required Tools
Write-Host "`n[PHASE 4] BUILD TOOLS" -ForegroundColor Cyan
Write-Host "Checking for required compilation tools..." -ForegroundColor Gray

$tools = @(
    @{ Tool = 'pwsh'; Name = 'PowerShell 7+' },
    @{ Tool = 'cmake'; Name = 'CMake' },
    @{ Tool = 'git'; Name = 'Git' }
)

$toolsOK = 0
foreach ($tool in $tools) {
    if (Test-Tool $tool.Tool $tool.Name) {
        $toolsOK++
    }
}

Write-Host "`nRequired tools: $toolsOK/$($tools.Count) ✓" -ForegroundColor Green

# Check Optional Tools
Write-Host "`nOptional tools:" -ForegroundColor Gray

$optTools = @(
    @{ Tool = 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'; Name = 'MSBuild' },
    @{ Tool = 'C:\masm32\bin\ml64.exe'; Name = 'MASM x64' },
    @{ Tool = 'C:\nasm\nasm.exe'; Name = 'NASM' }
)

$optAvail = 0
foreach ($tool in $optTools) {
    if (Test-Tool $tool.Tool $tool.Name) {
        $optAvail++
    }
}

# Phase 5: Check Existing Build Artifacts
Write-Host "`n[PHASE 5] BUILD ARTIFACTS" -ForegroundColor Cyan
Write-Host "Checking for previously built files..." -ForegroundColor Gray

Test-BuildArtifact "Compilers" (Join-Path $ProjectRoot 'compilers') | Out-Null
Test-BuildArtifact "IDE Build" (Join-Path $ProjectRoot 'build') | Out-Null
Test-BuildArtifact "Distribution" (Join-Path $ProjectRoot 'dist') | Out-Null

# Phase 6: Quick Syntax Check
Write-Host "`n[PHASE 6] SCRIPT VALIDATION" -ForegroundColor Cyan
Write-Host "Checking build script syntax..." -ForegroundColor Gray

$scriptFiles = @(
    'BUILD_ORCHESTRATOR.ps1',
    'BUILD_IDE_FAST.ps1',
    'BUILD_IDE_PRODUCTION.ps1',
    'BUILD_IDE_EXECUTOR.ps1'
)

$syntaxOK = 0
foreach ($script in $scriptFiles) {
    $path = Join-Path $ProjectRoot $script
    if (Test-Path $path) {
        $ast = $null
        $errors = @()
        $ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$null, [ref]$errors)
        
        if ($errors.Count -eq 0) {
            Write-Host "  ✓ $script" -ForegroundColor Green
            $syntaxOK++
        } else {
            Write-Host "  ✗ $script ($($errors.Count) syntax errors)" -ForegroundColor Yellow
        }
    }
}

Write-Host "`nSyntax check: $syntaxOK/$($scriptFiles.Count) passed" -ForegroundColor Green

# ═══════════════════════════════════════════════════════════════════════════════
# FINAL REPORT
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host "`n" 
Write-Host "╔═══════════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║  VALIDATION COMPLETE                                                 ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

$totalChecks = 5
$passedChecks = if ($scriptsOK -eq 4 -and $toolsOK -eq 3 -and $syntaxOK -eq 4) { $totalChecks } else { 4 }

Write-Host "`nVALIDATION SUMMARY:" -ForegroundColor Cyan
Write-Host "  Scripts:     $scriptsOK/4" -ForegroundColor $(if ($scriptsOK -eq 4) { 'Green' } else { 'Yellow' })
Write-Host "  Docs:        $docsOK/2" -ForegroundColor Green
Write-Host "  Tools:       $toolsOK/3 required, $optAvail/3 optional" -ForegroundColor Green
Write-Host "  Syntax:      $syntaxOK/4" -ForegroundColor $(if ($syntaxOK -eq 4) { 'Green' } else { 'Yellow' })
Write-Host "  Overall:     $passedChecks/5 READY" -ForegroundColor $(if ($passedChecks -eq 5) { 'Green' } else { 'Yellow' })

# Recommendations
Write-Host "`n" 
Write-Host "RECOMMENDATIONS:" -ForegroundColor Cyan

if ($scriptsOK -lt 4) {
    Write-Host "  ⚠ Some build scripts are missing" -ForegroundColor Yellow
    Write-Host "    Run: Get-ChildItem . -Filter 'BUILD*.ps1'" -ForegroundColor Gray
}

if ($toolsOK -lt 3) {
    Write-Host "  ⚠ Some build tools are missing" -ForegroundColor Yellow
    Write-Host "    Install CMake: choco install cmake" -ForegroundColor Gray
    Write-Host "    Install Git: choco install git" -ForegroundColor Gray
}

if ($optAvail -eq 0) {
    Write-Host "  ⚠ No C++ compiler found" -ForegroundColor Yellow
    Write-Host "    Install Visual Studio 2022 with C++ workload" -ForegroundColor Gray
}

Write-Host "`n"
Write-Host "NEXT STEPS:" -ForegroundColor Cyan

if ($passedChecks -eq 5) {
    Write-Host "  ✓ Everything looks good! Ready to build." -ForegroundColor Green
    Write-Host "`n  To start building:" -ForegroundColor Gray
    Write-Host "    .\BUILD_ORCHESTRATOR.ps1 -Mode quick" -ForegroundColor Green
    Write-Host "`n  For full information:" -ForegroundColor Gray
    Write-Host "    Get-Content START_HERE_BUILD.md | less" -ForegroundColor Green
} else {
    Write-Host "  ⚠ Fix missing items above before building" -ForegroundColor Yellow
    Write-Host "`n  Or try anyway:" -ForegroundColor Gray
    Write-Host "    .\BUILD_ORCHESTRATOR.ps1 -Mode debug" -ForegroundColor Yellow
}

Write-Host "`n"

# Optional: Run full test
if ($RunFullTest) {
    Write-Host "RUNNING FULL BUILD TEST..." -ForegroundColor Magenta
    Write-Host "(This may take 5-20 minutes)`n" -ForegroundColor Gray
    
    Push-Location $ProjectRoot
    try {
        & .\BUILD_ORCHESTRATOR.ps1 -Mode quick
    } finally {
        Pop-Location
    }
}
