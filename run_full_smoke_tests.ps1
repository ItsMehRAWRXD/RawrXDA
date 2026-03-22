#!/usr/bin/env pwsh
<#
.SYNOPSIS
Comprehensive smoke tests for RawrXD IDE (Win32 + Electron + IRC)

.DESCRIPTION
Tests:
  1. C++ Win32IDE binary exists and is executable
  2. React/Electron app builds/dependencies
  3. IRC protocol module tests
  4. Core configuration files
  5. Asset availability
#>

param()

$ErrorActionPreference = "Continue"
$VerbosePreference = "SilentlyContinue"

# Colors
$GREEN = "Green"
$RED = "Red"
$YELLOW = "Yellow"
$CYAN = "Cyan"
$GRAY = "Gray"

# Test counters
$passed = 0
$failed = 0
$warnings = 0
$startTime = Get-Date

function Write-TestHeader {
    param([string]$Title)
    Write-Host "`n$('=' * 70)" -ForegroundColor $CYAN
    Write-Host "[TEST] $Title" -ForegroundColor $CYAN
    Write-Host "=" * 70 -ForegroundColor $CYAN
}

function Write-TestPass {
    param([string]$Message)
    Write-Host "✓ PASS: $Message" -ForegroundColor $GREEN
    $script:passed++
}

function Write-TestFail {
    param([string]$Message)
    Write-Host "✗ FAIL: $Message" -ForegroundColor $RED
    $script:failed++
}

function Write-TestWarn {
    param([string]$Message)
    Write-Host "⚠ WARN: $Message" -ForegroundColor $YELLOW
    $script:warnings++
}

function Write-TestInfo {
    param([string]$Message)
    Write-Host "  → $Message" -ForegroundColor $GRAY
}

# ============================================================================
# Test 1: C++ Win32IDE Binary
# ============================================================================
Write-TestHeader "C++ Win32IDE Binary Tests"

$ideExe = "d:\RawrXD\bin\RawrXD-Win32IDE.exe"
if (Test-Path $ideExe) {
    Write-TestPass "Win32IDE binary exists at $ideExe"
    
    $fileInfo = Get-Item $ideExe
    $sizeKB = [math]::Round($fileInfo.Length / 1KB, 2)
    Write-TestInfo "Binary size: $sizeKB KB"
    Write-TestInfo "Created: $($fileInfo.CreationTime)"
    
    # Try to get version info
    $versionInfo = $null
    try {
        $versionInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($ideExe)
        if ($versionInfo.ProductVersion) {
            Write-TestPass "Version info detected: $($versionInfo.ProductVersion)"
        }
    } catch {
        Write-TestWarn "Could not read version info: $_"
    }
    
    # Quick execution test (--version if supported, otherwise just check it starts)
    Write-TestInfo "Attempting execution validation..."
    try {
        $proc = Start-Process -FilePath $ideExe -NoNewWindow -PassThru -ArgumentList "--version" -ErrorAction SilentlyContinue
        if ($proc) {
            $proc | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
            Write-TestPass "Win32IDE executable launched successfully"
        }
    } catch {
        Write-TestWarn "Could not execute Win32IDE (may require GUI): $_"
    }
} else {
    Write-TestFail "Win32IDE binary NOT found at $ideExe"
}

# ============================================================================
# Test 2: React/Electron App Structure
# ============================================================================
Write-TestHeader "React/Electron Application Tests"

$appRoot = "d:\RawrXD\bigdaddyg-ide"

# Check key directories
@(
    (Join-Path $appRoot "src"),
    (Join-Path $appRoot "electron"),
    (Join-Path $appRoot "public"),
    (Join-Path $appRoot "node_modules")
) | ForEach-Object {
    if (Test-Path $_) {
        Write-TestPass "Directory exists: $_"
    } else {
        Write-TestFail "Directory missing: $_"
    }
}

# Check key package.json
if (Test-Path (Join-Path $appRoot "package.json")) {
    Write-TestPass "package.json exists"
    
    $pkgContent = Get-Content (Join-Path $appRoot "package.json") -Raw | ConvertFrom-Json
    Write-TestInfo "App: $($pkgContent.name) v$($pkgContent.version)"
    
    # Check key scripts
    @("build", "dev:react", "dev:electron", "start") | ForEach-Object {
        if ($pkgContent.scripts.$_) {
            Write-TestPass "Script defined: $_"
        } else {
            Write-TestWarn "Script missing: $_"
        }
    }
} else {
    Write-TestFail "package.json not found"
}

# Check Node modules installed
$nodeModulesPath = Join-Path $appRoot "node_modules"
if (Test-Path $nodeModulesPath) {
    $moduleCount = @(Get-ChildItem $nodeModulesPath -Directory).Count
    if ($moduleCount -gt 100) {
        Write-TestPass "Node modules installed: ~$moduleCount packages"
    } else {
        Write-TestWarn "Fewer than expected Node modules: $moduleCount packages"
    }
} else {
    Write-TestWarn "Node modules not installed yet"
}

# ============================================================================
# Test 3: React/Electron File Structure
# ============================================================================
Write-TestHeader "React/Electron Component Tests"

@(
    "public/index.html",
    "src/App.js",
    "src/index.js",
    "src/contexts/IdeFeaturesContext.js",
    "src/components/ChatPanel.js",
    "electron/main.js",
    "electron/irc_protocol.js",
    "electron/preload.js"
) | ForEach-Object {
    $fullPath = Join-Path $appRoot $_
    if (Test-Path $fullPath) {
        $item = Get-Item $fullPath
        $size = [math]::Round($item.Length / 1KB, 2)
        Write-TestPass "Component exists: $_ ($size KB)"
    } else {
        Write-TestFail "Component missing: $_"
    }
}

# ============================================================================
# Test 4: IRC Protocol Tests
# ============================================================================
Write-TestHeader "IRC Protocol Unit Tests"

$ircTestPath = Join-Path $appRoot "electron/irc_protocol.test.js"
if (Test-Path $ircTestPath) {
    Write-TestPass "IRC protocol test file exists"
    Write-TestInfo "Running IRC protocol tests..."
    
    try {
        $output = & node $ircTestPath 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-TestPass "IRC protocol tests passed"
            Write-TestInfo "Test output:"
            $output | Where-Object { $_ } | ForEach-Object {
                Write-TestInfo "  $_"
            }
        } else {
            Write-TestFail "IRC protocol tests failed (exit code: $LASTEXITCODE)"
            Write-TestInfo "Output: $output"
        }
    } catch {
        Write-TestWarn "Could not run IRC protocol tests: $_"
    }
} else {
    Write-TestFail "IRC protocol test file not found: $ircTestPath"
}

# ============================================================================
# Test 5: IRC Bridge Configuration
# ============================================================================
Write-TestHeader "IRC Bridge Configuration Tests"

$ircBridgePath = Join-Path $appRoot "electron/irc_bridge.js"
$ircProtocolPath = Join-Path $appRoot "electron/irc_protocol.js"

if (Test-Path $ircBridgePath) {
    Write-TestPass "IRC bridge file exists"
    $size = (Get-Item $ircBridgePath).Length
    Write-TestInfo "File size: $([math]::Round($size / 1KB, 2)) KB"
} else {
    Write-TestFail "IRC bridge file not found"
}

if (Test-Path $ircProtocolPath) {
    Write-TestPass "IRC protocol file exists"
    $size = (Get-Item $ircProtocolPath).Length
    Write-TestInfo "File size: $([math]::Round($size / 1KB, 2)) KB"
} else {
    Write-TestFail "IRC protocol file not found"
}

# ============================================================================
# Test 6: Win32IDE Header/Source Files
# ============================================================================
Write-TestHeader "Win32IDE Source Code Tests"

$srcRoot = "d:\RawrXD\src"

@(
    "win32app/Win32IDE.h",
    "win32app/Win32IDE.cpp",
    "win32app/Win32IDE_AgenticComposerUX.cpp",
    "win32app/Win32IDE_AgenticPlanningPanel.cpp",
    "agentic/agentic_planning_orchestrator.hpp",
    "agentic/failure_intelligence_orchestrator.hpp",
    "core/patch_result.hpp"
) | ForEach-Object {
    $fullPath = Join-Path $srcRoot $_
    if (Test-Path $fullPath) {
        $item = Get-Item $fullPath
        $size = [math]::Round($item.Length / 1KB, 2)
        Write-TestPass "Source file exists: $_ ($size KB)"
    } else {
        Write-TestFail "Source file missing: $_"
    }
}

# ============================================================================
# Test 7: Build Output Verification
# ============================================================================
Write-TestHeader "Build Output Tests"

$buildDir = "d:\rxdn\bin"
if (Test-Path $buildDir) {
    Write-TestPass "Build directory exists: $buildDir"
    
    $exeFile = Join-Path $buildDir "RawrXD-Win32IDE.exe"
    if (Test-Path $exeFile) {
        $fileInfo = Get-Item $exeFile
        Write-TestPass "Compiled executable found: RawrXD-Win32IDE.exe"
        Write-TestInfo "Size: $([math]::Round($fileInfo.Length / 1MB, 2)) MB"
        Write-TestInfo "Modified: $($fileInfo.LastWriteTime)"
    } else {
        Write-TestWarn "Executable not in build directory"
    }
} else {
    Write-TestWarn "Build directory not found (may be normal for fresh build)"
}

# ============================================================================
# Test 8: CMake Build System
# ============================================================================
Write-TestHeader "CMake Build System Tests"

@(
    "d:\RawrXD\CMakeLists.txt",
    "d:\rxdn\cmake_install.cmake",
    "d:\rxdn\CMakeCache.txt"
) | ForEach-Object {
    if (Test-Path $_) {
        Write-TestPass "Build artifact exists: $_"
    } else {
        Write-TestWarn "Build artifact missing: $_"
    }
}

# ============================================================================
# Test 9: Settings and Configuration
# ============================================================================
Write-TestHeader "Configuration Tests"

$settingsPath = "d:\RawrXD\.rawrxd"
if (Test-Path $settingsPath) {
    Write-TestPass "Settings directory exists"
} else {
    Write-TestInfo "Settings directory not created yet (normal for fresh build)"
}

$configPath = "d:\RawrXD\.rawrxd_agent_config.json"
if (Test-Path $configPath) {
    Write-TestPass "Agent config file exists"
    try {
        $cfg = Get-Content $configPath -Raw | ConvertFrom-Json
        Write-TestInfo "Config sections: $(($cfg | Get-Member -MemberType NoteProperty).Count) defined"
    } catch {
        Write-TestWarn "Could not parse agent config: $_"
    }
} else {
    Write-TestInfo "Agent config not generated yet (normal for fresh build)"
}

# ============================================================================
# Test 10: Docs and Help
# ============================================================================
Write-TestHeader "Documentation Tests"

@(
    "d:\RawrXD\README.md",
    "d:\RawrXD\docs",
    "d:\RawrXD\bigdaddyg-ide\README.md"
) | ForEach-Object {
    if (Test-Path $_) {
        Write-TestPass "Documentation/docs found: $_"
    }
}

$docIndex = Get-ChildItem "d:\RawrXD\docs" -Filter "*.md" -ErrorAction SilentlyContinue | Measure-Object | Select-Object -ExpandProperty Count
if ($docIndex -gt 0) {
    Write-TestInfo "Documentation files: $docIndex .md files"
}

# ============================================================================
# Test 11: Asset Files
# ============================================================================
Write-TestHeader "Asset Files Tests"

$publicPath = "d:\RawrXD\bigdaddyg-ide\public"
if (Test-Path $publicPath) {
    $assets = Get-ChildItem $publicPath -File | Measure-Object | Select-Object -ExpandProperty Count
    Write-TestPass "Public assets directory has $assets files"
    
    @("index.html", "favicon.ico") | ForEach-Object {
        $assetPath = Join-Path $publicPath $_
        if (Test-Path $assetPath) {
            Write-TestPass "Asset exists: $_"
        }
    }
}

# ============================================================================
# Summary
# ============================================================================
Write-Host "`n" + ("=" * 70) -ForegroundColor $CYAN
Write-Host "SMOKE TEST SUMMARY" -ForegroundColor $CYAN
Write-Host "=" * 70 -ForegroundColor $CYAN

$elapsed = (Get-Date) - $startTime
Write-Host "Duration: $([math]::Round($elapsed.TotalSeconds, 2)) seconds"
Write-Host ""
Write-Host "✓ Passed: $passed" -ForegroundColor $GREEN
Write-Host "✗ Failed: $failed" -ForegroundColor $(if ($failed -gt 0) { $RED } else { $GREEN })
Write-Host "⚠ Warnings: $warnings" -ForegroundColor $(if ($warnings -gt 0) { $YELLOW } else { $GRAY })
Write-Host ""

$total = $passed + $failed + $warnings
$passRate = if ($total -gt 0) { [math]::Round(($passed / $total) * 100, 1) } else { 0 }
Write-Host "Pass Rate: $passRate% ($passed/$total tests)" -ForegroundColor $(if ($passRate -ge 80) { $GREEN } else { $YELLOW })

Write-Host ""
Write-Host "=" * 70 -ForegroundColor $CYAN

if ($failed -eq 0) {
    Write-Host "✓ ALL CRITICAL TESTS PASSED" -ForegroundColor $GREEN
    exit 0
} else {
    Write-Host "✗ SOME TESTS FAILED - REVIEW ABOVE" -ForegroundColor $RED
    exit 1
}
