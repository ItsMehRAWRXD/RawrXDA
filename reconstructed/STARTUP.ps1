#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD Complete System Launcher - All-in-One Startup
    
.DESCRIPTION
    Launches RawrXD IDE with integrated model digestion system
    No external dependencies beyond built executables
    
.PARAMETER Mode
    'ide' - Launch IDE only (default)
    'digest' - Run model digestion pipeline
    'complete' - IDE + Automatic digestion
    'test' - Run system verification tests
    
.EXAMPLE
    .\STARTUP.ps1 -Mode ide
    .\STARTUP.ps1 -Mode digest -ModelPath "d:\models\llama.gguf" -OutputDir "d:\digested"
#>

param(
    [ValidateSet('ide', 'digest', 'complete', 'test')]
    [string]$Mode = 'ide',
    
    [string]$ModelPath = '',
    [string]$OutputDir = 'd:\digested-models',
    [string]$ModelName = '',
    [switch]$SkipValidation
)

$ErrorActionPreference = 'Stop'

# ============================================================================
# CONFIGURATION
# ============================================================================

$PROJECT_ROOT = 'd:\rawrxd'
$IDE_EXE = "$PROJECT_ROOT\build\bin\Release\RawrXD-Win32IDE.exe"
$DIGEST_SCRIPT = 'd:\digest.py'
$PYTHON_EXE = 'python'

# Colors for output
$Colors = @{
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
    Header = 'Magenta'
}

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

function Write-Status {
    param([string]$Message, [string]$Type = 'Info')
    $Color = $Colors[$Type]
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $Message" -ForegroundColor $Color
}

function Test-Prerequisites {
    Write-Status "🔍 Checking prerequisites..." 'Header'
    
    # Check Python
    $pythonTest = & python --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Status "❌ Python not found. Install from python.org" 'Error'
        return $false
    }
    Write-Status "✅ Python: $pythonTest" 'Success'
    
    # Check IDE executable
    if (-not (Test-Path $IDE_EXE)) {
        Write-Status "❌ IDE executable not found at $IDE_EXE" 'Error'
        Write-Status "   Run: cd $PROJECT_ROOT && cmake --build build --config Release" 'Warning'
        return $false
    }
    Write-Status "✅ IDE: $IDE_EXE" 'Success'
    
    # Check digest script
    if (-not (Test-Path $DIGEST_SCRIPT)) {
        Write-Status "⚠️  Digest script not found: $DIGEST_SCRIPT" 'Warning'
        Write-Status "   Model digestion disabled" 'Warning'
    } else {
        Write-Status "✅ Digest: $DIGEST_SCRIPT" 'Success'
    }
    
    Write-Status "✅ All prerequisites met" 'Success'
    return $true
}

function Start-IDE {
    Write-Status "🚀 Launching RawrXD IDE..." 'Header'
    Write-Status "Executable: $IDE_EXE" 'Info'
    
    try {
        & $IDE_EXE

        # For native executables, PowerShell sets $LASTEXITCODE.
        $code = $LASTEXITCODE
        if ($code -ne 0) {
            Write-Status "❌ IDE exited with code $code" 'Error'

            # Crash containment writes dumps under ./crash_dumps (relative to current dir)
            if (Test-Path "crash_dumps") {
                $latest = Get-ChildItem "crash_dumps" -Filter "*.dmp" -ErrorAction SilentlyContinue |
                    Sort-Object LastWriteTime -Descending |
                    Select-Object -First 1
                if ($latest) {
                    Write-Status "Latest dump: $($latest.FullName)" 'Warning'
                } else {
                    Write-Status "Crash dump folder exists but no .dmp found" 'Warning'
                }
            } else {
                Write-Status "No crash_dumps folder found in current directory" 'Warning'
            }
            return $false
        }

        Write-Status "✅ IDE closed successfully" 'Success'
        return $true
    } catch {
        Write-Status "❌ Failed to launch IDE: $_" 'Error'
        return $false
    }
}

function Start-ModelDigestion {
    param(
        [string]$InputModel,
        [string]$OutputDir,
        [string]$ModelName
    )
    
    Write-Status "🔄 Starting Model Digestion Pipeline..." 'Header'
    
    if (-not (Test-Path $DIGEST_SCRIPT)) {
        Write-Status "❌ Digest script not found" 'Error'
        return $false
    }
    
    if (-not $InputModel -or -not (Test-Path $InputModel)) {
        Write-Status "❌ Model file not found: $InputModel" 'Error'
        return $false
    }
    
    $OutputDir | ForEach-Object {
        New-Item -ItemType Directory -Force -Path $_ | Out-Null
    }
    
    $Args = @("-i", $InputModel, "-o", $OutputDir)
    if ($ModelName) {
        $Args += "-n", $ModelName
    }
    
    Write-Status "Input:  $InputModel" 'Info'
    Write-Status "Output: $OutputDir" 'Info'
    
    try {
        Write-Status "⏳ Processing (this may take a few minutes)..." 'Info'
        & $PYTHON_EXE $DIGEST_SCRIPT @Args
        
        if ($LASTEXITCODE -eq 0) {
            Write-Status "✅ Digestion complete" 'Success'
            Write-Status "Output files:" 'Info'
            Get-ChildItem $OutputDir | ForEach-Object {
                Write-Status "  - $($_.Name)" 'Info'
            }
            return $true
        } else {
            Write-Status "❌ Digestion failed with exit code $LASTEXITCODE" 'Error'
            return $false
        }
    } catch {
        Write-Status "❌ Digestion error: $_" 'Error'
        return $false
    }
}

function Show-Usage {
    Write-Host @"
╔════════════════════════════════════════════════════════════════╗
║                  RawrXD COMPLETE SYSTEM LAUNCHER              ║
║                      v1.0 - Production Ready                   ║
╚════════════════════════════════════════════════════════════════╝

MODES:
  ide        Launch IDE only (default)
  digest     Run model digestion pipeline
  complete   Launch IDE + digest selected model
  test       Run system verification tests

EXAMPLES:
  
  Launch IDE:
    .\STARTUP.ps1
    .\STARTUP.ps1 -Mode ide
  
  Digest a model:
    .\STARTUP.ps1 -Mode digest -ModelPath "e:\model.gguf" -OutputDir "d:\digested"
  
  IDE + Digestion:
    .\STARTUP.ps1 -Mode complete -ModelPath "d:\models\llama.gguf"
  
  System test:
    .\STARTUP.ps1 -Mode test

PREREQUISITES:
  - Python 3.8+ installed
  - MSVC 2022 (for IDE build)
  - Windows 10/11 x64

FOR FIRST USE:
  1. .\STARTUP.ps1 -Mode test    (verify everything works)
  2. .\STARTUP.ps1 -Mode ide     (launch IDE)
  3. .\STARTUP.ps1 -Mode digest -ModelPath <path> (digest models)

"@
}

function Run-Tests {
    Write-Status "🧪 Running System Tests..." 'Header'
    
    $results = @{
        Python = $false
        IDE = $false
        Digest = $false
        Overall = $true
    }
    
    # Test Python
    Write-Status "Testing Python..." 'Info'
    $pythonTest = & python --version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Status "✅ Python: $pythonTest" 'Success'
        $results.Python = $true
    } else {
        Write-Status "❌ Python test failed" 'Error'
        $results.Overall = $false
    }
    
    # Test IDE executable
    Write-Status "Testing IDE executable..." 'Info'
    if (Test-Path $IDE_EXE) {
        Write-Status "✅ IDE exists: $IDE_EXE" 'Success'
        $results.IDE = $true
    } else {
        Write-Status "❌ IDE executable not found" 'Error'
        $results.Overall = $false
    }
    
    # Test digest script
    Write-Status "Testing digest script..." 'Info'
    if (Test-Path $DIGEST_SCRIPT) {
        Write-Status "✅ Digest exists: $DIGEST_SCRIPT" 'Success'
        $results.Digest = $true
    } else {
        Write-Status "⚠️  Digest script not found (optional)" 'Warning'
    }
    
    # Summary
    Write-Status "════════════════════════════════" 'Header'
    Write-Status "TEST RESULTS:" 'Header'
    Write-Status "  Python:    $(if ($results.Python) {'✅'} else {'❌'})" 'Info'
    Write-Status "  IDE:       $(if ($results.IDE) {'✅'} else {'❌'})" 'Info'
    Write-Status "  Digest:    $(if ($results.Digest) {'✅'} else {'⚠️ '} )" 'Info'
    Write-Status "  Overall:   $(if ($results.Overall) {'✅ PASS'} else {'❌ FAIL'})" $(if ($results.Overall) {'Success'} else {'Error'})
    Write-Status "════════════════════════════════" 'Header'
    
    return $results.Overall
}

# ============================================================================
# MAIN PROGRAM
# ============================================================================

function Main {
    Clear-Host
    Write-Status "╔════════════════════════════════════════════════════════════════╗" 'Header'
    Write-Status "║              RawrXD Complete System v1.0                       ║" 'Header'
    Write-Status "║     Model Digestion + IDE + Encryption in One Package          ║" 'Header'
    Write-Status "╚════════════════════════════════════════════════════════════════╝" 'Header'
    Write-Status ""
    
    if (-not $SkipValidation) {
        if (-not (Test-Prerequisites)) {
            Write-Status ""
            Write-Status "❌ Please fix the above issues before continuing" 'Error'
            exit 1
        }
        Write-Status ""
    }
    
    # Switch based on mode
    switch ($Mode) {
        'ide' {
            Write-Status "Starting IDE..." 'Info'
            $result = Start-IDE
            exit $(if ($result) {0} else {1})
        }
        
        'digest' {
            if (-not $ModelPath) {
                Write-Status "❌ -ModelPath required for digest mode" 'Error'
                Show-Usage
                exit 1
            }
            $result = Start-ModelDigestion -InputModel $ModelPath -OutputDir $OutputDir -ModelName $ModelName
            exit $(if ($result) {0} else {1})
        }
        
        'complete' {
            if (-not $ModelPath) {
                Write-Status "❌ -ModelPath required for complete mode" 'Error'
                Show-Usage
                exit 1
            }
            Write-Status "Running complete workflow: Digest → IDE" 'Info'
            $digestionResult = Start-ModelDigestion -InputModel $ModelPath -OutputDir $OutputDir -ModelName $ModelName
            if ($digestionResult) {
                Write-Status ""
                $ideResult = Start-IDE
                exit $(if ($ideResult) {0} else {1})
            }
            exit 1
        }
        
        'test' {
            $result = Run-Tests
            exit $(if ($result) {0} else {1})
        }
        
        default {
            Show-Usage
            exit 1
        }
    }
}

Main
