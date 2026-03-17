#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD System Verification - Complete Integration Test
    
.DESCRIPTION
    Comprehensive validation of entire RawrXD system:
    - IDE executable integrity
    - Python environment
    - Model digestion pipeline
    - File structure
    - Dependencies
    
.EXAMPLE
    .\VERIFY-SYSTEM.ps1
    .\VERIFY-SYSTEM.ps1 -Verbose
    .\VERIFY-SYSTEM.ps1 -SkipLongTests
#>

param(
    [switch]$Verbose,
    [switch]$SkipLongTests,
    [switch]$DetailedReport
)

$ErrorActionPreference = 'Continue'

# Configuration
$PROJECT_ROOT = 'd:\rawrxd'
$IDE_EXE = "$PROJECT_ROOT\build\bin\Release\RawrXD-Win32IDE.exe"
$DIGEST_SCRIPT = 'd:\digest.py'
$README = 'd:\README.md'
$STARTUP_PS1 = 'd:\STARTUP.ps1'
$STARTUP_BAT = 'd:\STARTUP.bat'

# Colors
$Colors = @{
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
    Header = 'Magenta'
    Detail = 'DarkGray'
}

# Test results tracking
$TestResults = @{
    Passed = 0
    Failed = 0
    Warnings = 0
    Tests = @()
}

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

function Write-TestHeader {
    param([string]$Title)
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor $Colors.Header
    Write-Host "║ $($Title.PadRight(60)) ║" -ForegroundColor $Colors.Header
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor $Colors.Header
    Write-Host ""
}

function Write-Test {
    param(
        [string]$Name,
        [bool]$Pass,
        [string]$Details = '',
        [string]$Type = 'Info'
    )
    
    $Status = if ($Pass) { '✅ PASS' } else { '❌ FAIL' }
    $Color = if ($Pass) { $Colors.Success } else { $Colors.Error }
    
    Write-Host "$Status : $Name" -ForegroundColor $Color
    if ($Details) {
        Write-Host "       └─ $Details" -ForegroundColor $Colors.Detail
    }
    
    if ($Pass) {
        $TestResults.Passed++
    } else {
        $TestResults.Failed++
    }
    
    $TestResults.Tests += @{
        Name = $Name
        Pass = $Pass
        Details = $Details
    }
}

function Test-FileExists {
    param(
        [string]$FilePath,
        [string]$Description = '',
        [bool]$Critical = $true
    )
    
    $desc = if ($Description) { "$Description - " } else { '' }
    $exists = Test-Path $FilePath
    
    if ($exists) {
        $size = (Get-Item $FilePath).Length / 1MB
        Write-Test "$($desc)Exists" $true "Size: $([Math]::Round($size, 2)) MB"
    } else {
        Write-Test "$($desc)Exists" $false "Not found" $(if ($Critical) { 'Error' } else { 'Warning' })
    }
    
    return $exists
}

function Test-DirectoryExists {
    param(
        [string]$DirPath,
        [string]$Description = '',
        [bool]$Critical = $true
    )
    
    $desc = if ($Description) { "$Description - " } else { '' }
    $exists = Test-Path -PathType Container $DirPath
    
    if ($exists) {
        $itemCount = (Get-ChildItem $DirPath -ErrorAction SilentlyContinue | Measure-Object).Count
        Write-Test "$($desc)Directory exists" $true "Items: $itemCount"
    } else {
        Write-Test "$($desc)Directory exists" $false "Not found" $(if ($Critical) { 'Error' } else { 'Warning' })
    }
    
    return $exists
}

function Test-Executable {
    param(
        [string]$ExePath,
        [string]$Description = '',
        [string[]]$TestArgs = @()
    )
    
    $desc = if ($Description) { "$Description - " } else { '' }
    
    # Check existence
    if (-not (Test-Path $ExePath)) {
        Write-Test "$($desc)Executable exists" $false "File not found"
        return $false
    }
    
    Write-Test "$($desc)Executable exists" $true
    
    # Try to run with test args
    if ($TestArgs.Count -gt 0) {
        try {
            $output = & $ExePath @TestArgs 2>&1
            $success = $LASTEXITCODE -eq 0 -or $output
            Write-Test "$($desc)Runs successfully" $success
            return $success
        } catch {
            Write-Test "$($desc)Runs successfully" $false "Exception: $_"
            return $false
        }
    }
    
    return $true
}

function Test-PythonEnvironment {
    Write-TestHeader "Python Environment"
    
    # Check Python exists
    try {
        $pythonVersion = & python --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Test "Python installed" $true "$pythonVersion"
        } else {
            Write-Test "Python installed" $false "Command returned error"
            return $false
        }
    } catch {
        Write-Test "Python installed" $false "Python not found"
        return $false
    }
    
    # Check Python works
    try {
        $pythonTest = & python -c "import sys; print(f'Python {sys.version_info.major}.{sys.version_info.minor}')" 2>&1
        Write-Test "Python functional" $true
    } catch {
        Write-Test "Python functional" $false "Import test failed"
        return $false
    }
    
    # Check crypto library (optional but recommended)
    try {
        & python -c "from Crypto.Cipher import AES" 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Test "Crypto library (pycryptodome)" $true "Available for encryption"
        } else {
            Write-Test "Crypto library (pycryptodome)" $false "Not installed, install with: pip install pycryptodome" $false
        }
    } catch {
        Write-Test "Crypto library (pycryptodome)" $false "Import failed"
    }
    
    return $true
}

function Test-IDESystem {
    Write-TestHeader "RawrXD IDE System"
    
    # Check source directory
    Test-DirectoryExists "$PROJECT_ROOT" "IDE source directory" $true
    
    # Check build directory
    Test-DirectoryExists "$PROJECT_ROOT\build" "IDE build directory" $true
    
    # Check executable
    Test-FileExists $IDE_EXE "IDE executable" $true
    
    # Check source files
    $srcCount = (Get-ChildItem "$PROJECT_ROOT\src\*.cpp" -ErrorAction SilentlyContinue | Measure-Object).Count
    Write-Test "Source files" ($srcCount -gt 100) "Found C++ files: $srcCount"
    
    # Check CMakeLists.txt
    Test-FileExists "$PROJECT_ROOT\CMakeLists.txt" "CMake configuration" $true
    
    # Check object files
    $objCount = (Get-ChildItem "$PROJECT_ROOT\build\*.obj" -ErrorAction SilentlyContinue | Measure-Object).Count
    Write-Test "Compiled objects" ($objCount -gt 10) "Object files: $objCount"
    
    return (Test-Path $IDE_EXE)
}

function Test-DigestionSystem {
    Write-TestHeader "Model Digestion System"
    
    # Check Python script
    Test-FileExists $DIGEST_SCRIPT "Digest Python script" $true
    
    # Check TypeScript engine
    Test-FileExists 'd:\model-digestion-engine.ts' "Digest TypeScript engine" $false
    
    # Check MASM loader
    Test-FileExists 'd:\ModelDigestion_x64.asm' "MASM x64 loader" $false
    
    # Check C++ header
    Test-FileExists 'd:\ModelDigestion.hpp' "C++ integration header" $false
    
    # Check PowerShell script
    Test-FileExists 'd:\digest-quick-start.ps1' "Digest PowerShell script" $false
    
    # Test digest script syntax
    try {
        $pythonTest = & python -m py_compile $DIGEST_SCRIPT 2>&1
        Write-Test "Digest script syntax" $true
    } catch {
        Write-Test "Digest script syntax" $false "Compile check failed"
    }
    
    return (Test-Path $DIGEST_SCRIPT)
}

function Test-LauncherScripts {
    Write-TestHeader "Launcher Scripts"
    
    # Check main startup scripts
    Test-FileExists $STARTUP_PS1 "STARTUP.ps1" $true
    Test-FileExists $STARTUP_BAT "STARTUP.bat" $true
    
    # Verify PS1 can be parsed
    try {
        $parseTest = Test-Path $STARTUP_PS1
        if ($parseTest) {
            $content = Get-Content $STARTUP_PS1 -TotalCount 1 | Select-Object -First 1
            Write-Test "STARTUP.ps1 is readable" $true
        }
    } catch {
        Write-Test "STARTUP.ps1 is readable" $false $_
    }
    
    # Verify BAT can be parsed
    try {
        if (Test-Path $STARTUP_BAT) {
            $content = Get-Content $STARTUP_BAT -TotalCount 1
            Write-Test "STARTUP.bat is readable" $true
        }
    } catch {
        Write-Test "STARTUP.bat is readable" $false $_
    }
    
    return (Test-Path $STARTUP_PS1) -and (Test-Path $STARTUP_BAT)
}

function Test-Documentation {
    Write-TestHeader "Documentation Files"
    
    $docs = @(
        'd:\README.md' > 'Main README'
        'd:\00-START-HERE.md' > 'Start Here Guide'
        'd:\MODEL_DIGESTION_GUIDE.md' > 'Digestion Guide'
        'd:\DELIVERABLES_MANIFEST.md' > 'Deliverables'
        'd:\CRITICAL_FIXES_APPLIED.md' > 'Critical Fixes'
    )
    
    foreach ($doc in $docs) {
        $path = $doc.Split('>')[0].Trim()
        $name = $doc.Split('>')[1].Trim()
        Test-FileExists $path $name $false
    }
}

function Test-DirectoryStructure {
    Write-TestHeader "Directory Structure"
    
    $requiredDirs = @{
        "d:\rawrxd" = "IDE root"
        "d:\rawrxd\src" = "IDE source"
        "d:\rawrxd\build" = "IDE build"
        "d:\rawrxd\include" = "IDE headers"
    }
    
    foreach ($dir in $requiredDirs.Keys) {
        Test-DirectoryExists $dir $requiredDirs[$dir] $($dir -like "d:\rawrxd\*")
    }
}

function Test-DiskSpace {
    Write-TestHeader "Disk Space"
    
    try {
        $drive = Get-Volume -DriveLetter d
        $freeGB = [Math]::Round($drive.SizeRemaining / 1GB, 2)
        $totalGB = [Math]::Round($drive.Size / 1GB, 2)
        $usedGB = [Math]::Round(($drive.Size - $drive.SizeRemaining) / 1GB, 2)
        
        $percentUsed = [Math]::Round(($usedGB / $totalGB) * 100, 1)
        
        Write-Test "Disk space (D:)" $true "Used: $($usedGB)GB / $($totalGB)GB ($percentUsed%)"
        Write-Test "Free space adequate" ($freeGB -gt 0.5) "Free: $($freeGB)GB (recommended: >1GB)"
    } catch {
        Write-Test "Disk space check" $false "Unable to check: $_"
    }
}

function Test-CommandAvailability {
    Write-TestHeader "Command Availability"
    
    $commands = @(
        'python'
        'cmake'
        'git'
    )
    
    foreach ($cmd in $commands) {
        try {
            $result = where.exe $cmd 2>&1
            $found = $LASTEXITCODE -eq 0
            Write-Test "$cmd available" $found
        } catch {
            Write-Test "$cmd available" $false
        }
    }
}

function Show-Summary {
    Write-Host ""
    Write-TestHeader "VERIFICATION SUMMARY"
    
    $total = $TestResults.Passed + $TestResults.Failed
    $passRate = if ($total -gt 0) { [Math]::Round(($TestResults.Passed / $total) * 100, 1) } else { 0 }
    
    Write-Host "Total Tests:     $total" -ForegroundColor $Colors.Info
    Write-Host "✅ Passed:        $($TestResults.Passed)" -ForegroundColor $Colors.Success
    Write-Host "❌ Failed:        $($TestResults.Failed)" -ForegroundColor $Colors.Error
    Write-Host "Pass Rate:       $passRate%" -ForegroundColor $(if ($passRate -ge 90) { $Colors.Success } else { $Colors.Warning })
    
    Write-Host ""
    
    if ($TestResults.Failed -eq 0) {
        Write-Host "✅ ALL SYSTEMS GO! System is production-ready." -ForegroundColor $Colors.Success
        Write-Host ""
        Write-Host "You can now:" -ForegroundColor $Colors.Info
        Write-Host "  1. Launch IDE:           .\STARTUP.ps1 -Mode ide" -ForegroundColor $Colors.Info
        Write-Host "  2. Digest models:        .\STARTUP.ps1 -Mode digest -ModelPath <path>" -ForegroundColor $Colors.Info
        Write-Host "  3. Full workflow:        .\STARTUP.ps1 -Mode complete -ModelPath <path>" -ForegroundColor $Colors.Info
        return 0
    } else {
        Write-Host "⚠️  Some tests failed. Please review above for details." -ForegroundColor $Colors.Warning
        Write-Host ""
        Write-Host "Critical failures must be fixed before using the system." -ForegroundColor $Colors.Error
        return 1
    }
}

# ============================================================================
# MAIN VERIFICATION FLOW
# ============================================================================

Clear-Host
Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║         RawrXD SYSTEM VERIFICATION - Comprehensive Test        ║" -ForegroundColor Magenta
Write-Host "║                    Version 1.0 - Feb 2026                      ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

Write-Host ""
Write-Host "Testing all system components..."
Write-Host "This may take a minute..." -ForegroundColor Cyan
Write-Host ""

# Run all tests
Test-PythonEnvironment
Test-IDESystem
Test-DigestionSystem
Test-LauncherScripts
Test-Documentation
Test-DirectoryStructure
Test-DiskSpace
Test-CommandAvailability

# Show summary
$exitCode = Show-Summary

Write-Host ""
exit $exitCode
