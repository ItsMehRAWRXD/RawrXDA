<#
.SYNOPSIS
    Test script to verify the refactored RawrXD module structure
    
.DESCRIPTION
    Tests module loading, function exports, and basic functionality
#>

$ErrorActionPreference = 'Stop'

Write-Host "`n=== Testing Refactored RawrXD Modules ===" -ForegroundColor Cyan
Write-Host ""

# Test 1: Check module files exist
Write-Host "[TEST 1] Checking module files..." -ForegroundColor Yellow
$modulesPath = Join-Path $PSScriptRoot "Modules\Modules"
$expectedModules = @(
    'RawrXD.Core',
    'RawrXD.Logging',
    'RawrXD.UI',
    'RawrXD.Editor',
    'RawrXD.FileOperations',
    'RawrXD.Git',
    'RawrXD.AI',
    'RawrXD.Browser',
    'RawrXD.Terminal',
    'RawrXD.Settings',
    'RawrXD.Agent',
    'RawrXD.Marketplace',
    'RawrXD.Video',
    'RawrXD.Security',
    'RawrXD.Performance',
    'RawrXD.Utilities'
)

$missingModules = @()
foreach ($module in $expectedModules) {
    $moduleFile = Join-Path $modulesPath "$module.psm1"
    if (Test-Path $moduleFile) {
        Write-Host "  ✓ $module.psm1 exists" -ForegroundColor Green
    }
    else {
        Write-Host "  ✗ $module.psm1 MISSING" -ForegroundColor Red
        $missingModules += $module
    }
}

if ($missingModules.Count -gt 0) {
    Write-Host "`n  WARNING: $($missingModules.Count) modules missing!" -ForegroundColor Yellow
}
Write-Host ""

# Test 2: Load and verify Core module
Write-Host "[TEST 2] Testing RawrXD.Core module..." -ForegroundColor Yellow
try {
    $coreModulePath = Join-Path $modulesPath "RawrXD.Core.psm1"
    if (Test-Path $coreModulePath) {
        Import-Module $coreModulePath -Force -ErrorAction Stop
        Write-Host "  ✓ Module loaded successfully" -ForegroundColor Green
        
        $exportedFunctions = Get-Command -Module RawrXD.Core -ErrorAction SilentlyContinue
        if ($exportedFunctions) {
            Write-Host "  ✓ Exported functions:" -ForegroundColor Green
            foreach ($func in $exportedFunctions) {
                Write-Host "    - $($func.Name)" -ForegroundColor Gray
            }
        }
        else {
            Write-Host "  ⚠ No functions exported or module not recognized" -ForegroundColor Yellow
        }
    }
    else {
        Write-Host "  ✗ Module file not found" -ForegroundColor Red
    }
}
catch {
    Write-Host "  ✗ Failed to load module: $($_.Exception.Message)" -ForegroundColor Red
}
Write-Host ""

# Test 3: Test main.ps1 module loading section
Write-Host "[TEST 3] Testing main.ps1 module loading logic..." -ForegroundColor Yellow
$mainPath = Join-Path $PSScriptRoot "Modules\main.ps1"
if (Test-Path $mainPath) {
    $mainContent = Get-Content $mainPath -Raw
    $moduleLoadingSection = $mainContent | Select-String -Pattern 'foreach.*module.*Modules' -Context 0,15
    
    if ($moduleLoadingSection) {
        Write-Host "  ✓ Module loading section found" -ForegroundColor Green
        
        # Check for the bug fix
        if ($mainContent -match '\$moduleFile = Join-Path \$ModulesPath "\$module\.psm1"') {
            Write-Host "  ✓ Module path construction is correct" -ForegroundColor Green
        }
        elseif ($mainContent -match '\$moduleFile = Join-Path \$ModulesPath "\.psm1"') {
            Write-Host "  ✗ BUG DETECTED: Module path missing `$module variable!" -ForegroundColor Red
        }
        else {
            Write-Host "  ⚠ Could not verify module path construction" -ForegroundColor Yellow
        }
    }
    else {
        Write-Host "  ✗ Module loading section not found" -ForegroundColor Red
    }
}
else {
    Write-Host "  ✗ main.ps1 not found" -ForegroundColor Red
}
Write-Host ""

# Test 4: Verify module exports
Write-Host "[TEST 4] Checking module exports..." -ForegroundColor Yellow
$modulesToTest = @('RawrXD.Core', 'RawrXD.AI', 'RawrXD.Browser')
foreach ($moduleName in $modulesToTest) {
    $moduleFile = Join-Path $modulesPath "$moduleName.psm1"
    if (Test-Path $moduleFile) {
        $moduleContent = Get-Content $moduleFile -Raw
        if ($moduleContent -match 'Export-ModuleMember') {
            Write-Host "  ✓ $moduleName has Export-ModuleMember" -ForegroundColor Green
        }
        else {
            Write-Host "  ✗ $moduleName missing Export-ModuleMember" -ForegroundColor Red
        }
    }
}
Write-Host ""

# Test 5: Check for common issues
Write-Host "[TEST 5] Checking for common issues..." -ForegroundColor Yellow

# Check Logging module size
$loggingModule = Join-Path $modulesPath "RawrXD.Logging.psm1"
if (Test-Path $loggingModule) {
    $loggingSize = (Get-Item $loggingModule).Length / 1MB
    Write-Host "  RawrXD.Logging.psm1 size: $([math]::Round($loggingSize, 2)) MB" -ForegroundColor Gray
    
    if ($loggingSize -gt 1) {
        Write-Host "  ⚠ WARNING: Logging module is very large ($([math]::Round($loggingSize, 2)) MB)" -ForegroundColor Yellow
        Write-Host "    This suggests over-categorization - many functions may be mis-categorized" -ForegroundColor Yellow
    }
}

# Check for duplicate function names
Write-Host "  Checking for duplicate function definitions..." -ForegroundColor Gray
$allFunctions = @()
Get-ChildItem -Path $modulesPath -Filter "*.psm1" | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $functions = [regex]::Matches($content, 'function\s+([\w-]+)')
    foreach ($match in $functions) {
        $allFunctions += @{
            Name = $match.Groups[1].Value
            Module = $_.BaseName
        }
    }
}

$duplicates = $allFunctions | Group-Object Name | Where-Object { $_.Count -gt 1 }
if ($duplicates) {
    Write-Host "  ✗ Found duplicate functions:" -ForegroundColor Red
    foreach ($dup in $duplicates) {
        Write-Host "    - $($dup.Name) in: $($dup.Group.Module -join ', ')" -ForegroundColor Red
    }
}
else {
    Write-Host "  ✓ No duplicate functions found" -ForegroundColor Green
}
Write-Host ""

# Test 6: Verify manifest
Write-Host "[TEST 6] Checking refactoring manifest..." -ForegroundColor Yellow
$manifestPath = Join-Path $PSScriptRoot "Modules\refactoring-manifest.json"
if (Test-Path $manifestPath) {
    try {
        $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
        Write-Host "  ✓ Manifest loaded successfully" -ForegroundColor Green
        Write-Host "    Total Functions: $($manifest.TotalFunctions)" -ForegroundColor Gray
        Write-Host "    Modules Created: $($manifest.Statistics.ModulesCreated)" -ForegroundColor Gray
        Write-Host "    Refactored Date: $($manifest.RefactoredDate)" -ForegroundColor Gray
    }
    catch {
        Write-Host "  ✗ Failed to parse manifest: $($_.Exception.Message)" -ForegroundColor Red
    }
}
else {
    Write-Host "  ✗ Manifest not found" -ForegroundColor Red
}
Write-Host ""

Write-Host "=== Test Complete ===" -ForegroundColor Cyan
Write-Host ""

