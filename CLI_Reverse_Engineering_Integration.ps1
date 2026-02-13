# CLI REVERSE ENGINEERING INTEGRATION
# Seamlessly integrates hand-made 32/64-bit CLI with reverse engineering toolkit

<#
.SYNOPSIS
    Universal CLI Integration for Reverse Engineering Toolkit
.DESCRIPTION
    Provides flawless integration between hand-made CLI tools and the reverse engineering suite.
    Automatically detects architecture (32/64-bit) and routes to appropriate tools.
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("build", "analyze", "reverse", "extract", "deobfuscate", "all")]
    [string]$Action,
    
    [Parameter(Mandatory=$true)]
    [string]$Target,
    
    [ValidateSet("32", "64", "auto")]
    [string]$Architecture = "auto",
    
    [switch]$Verbose,
    [switch]$ShowProgress
)

# Color codes for output
$Colors = @{
    Info = "Cyan"
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Header = "Magenta"
    Detail = "White"
    Reverse = "DarkGreen"
    CLI = "Blue"
}

function Write-ColorOutput {
    param([string]$Message, [string]$Type = "Info")
    Write-Host $Message -ForegroundColor $Colors[$Type]
}

function Get-Architecture {
    if ($Architecture -eq "auto") {
        # Detect target architecture
        if (Test-Path $Target) {
            $item = Get-Item $Target
            if ($item.Extension -eq ".asm") {
                # Check for 64-bit indicators in assembly file
                $content = Get-Content $Target -Raw
                if ($content -match "ml64|WIN64|RAX|RBX|RCX|RDX") {
                    return "64"
                } elseif ($content -match "\.386|\.model flat|EAX|EBX") {
                    return "32"
                }
            } elseif ($item.Extension -eq ".exe" -or $item.Extension -eq ".dll") {
                # Use dumpbin to check architecture
                $dumpbin = Get-Command "dumpbin.exe" -ErrorAction SilentlyContinue
                if ($dumpbin) {
                    $output = & $dumpbin /headers $Target 2>$null
                    if ($output -match "machine \(x64\)") {
                        return "64"
                    } elseif ($output -match "machine \(x86\)") {
                        return "32"
                    }
                }
            }
        }
        # Default to 64-bit for modern systems
        return "64"
    }
    return $Architecture
}

function Invoke-CLIBuild {
    param([string]$Arch)
    
    Write-ColorOutput "=== CLI BUILD DETECTED ===" "Header"
    Write-ColorOutput "Architecture: $Arch-bit" "Info"
    Write-ColorOutput "Target: $Target" "Detail"
    
    if ($Arch -eq "64") {
        # Use ml64 for 64-bit
        $ml = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
        $link = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
        $flags = "/c /nologo"
    } else {
        # Use ml for 32-bit
        $ml = "C:\masm32\bin\ml.exe"
        $link = "C:\masm32\bin\link.exe"
        $flags = "/c /coff /Cp /nologo"
    }
    
    if (-not (Test-Path $ml)) {
        Write-ColorOutput "⚠ Assembler not found: $ml" "Warning"
        Write-ColorOutput "→ Falling back to reverse engineering toolkit" "Detail"
        return $false
    }
    
    Write-ColorOutput "Building with $Arch-bit assembler..." "Info"
    
    # Assemble
    & $ml $flags $Target
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput "✗ Assembly failed" "Error"
        return $false
    }
    
    # Link
    $objFile = [System.IO.Path]::ChangeExtension($Target, ".obj")
    & $link /SUBSYSTEM:CONSOLE /OPT:NOWIN98 $objFile
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput "✗ Linking failed" "Error"
        return $false
    }
    
    Write-ColorOutput "✓ CLI build successful!" "Success"
    return $true
}

function Invoke-ReverseEngineering {
    param([string]$Arch)
    
    Write-ColorOutput "=== REVERSE ENGINEERING TOOLKIT ===" "Header"
    Write-ColorOutput "Architecture: $Arch-bit" "Info"
    Write-ColorOutput "Target: $Target" "Detail"
    
    # Route to appropriate tool based on action
    switch ($Action) {
        "build" {
            if ($Arch -eq "64") {
                . "D:\lazy init ide\build_codex_vs2022.bat"
            } else {
                . "D:\lazy init ide\build_omega_pro_final.bat"
            }
        }
        "analyze" {
            if (Test-Path $Target) {
                Write-ColorOutput "Analyzing $Target..." "Info"
                if ($Target -match "\.asm$") {
                    # Analyze assembly file
                    $lines = (Get-Content $Target).Count
                    $size = (Get-Item $Target).Length
                    Write-ColorOutput "Lines: $lines" "Detail"
                    Write-ColorOutput "Size: $size bytes" "Detail"
                    
                    # Check for 32/64-bit patterns
                    $content = Get-Content $Target -Raw
                    if ($content -match "ml64|WIN64|RAX") {
                        Write-ColorOutput "Detected: 64-bit assembly" "Success"
                    } elseif ($content -match "\.386|EAX") {
                        Write-ColorOutput "Detected: 32-bit assembly" "Success"
                    }
                } elseif ($Target -match "\.exe$|\.dll$") {
                    # Use OMEGA-POLYGLOT for PE analysis
                    . "D:\lazy init ide\omega_pro.exe" $Target
                }
            }
        }
        "reverse" {
            Write-ColorOutput "Using Fully-Reverse-Engineer..." "Reverse"
            . "D:\lazy init ide\Fully-Reverse-Engineer.ps1" -SourceDirectory $Target -ExtractAll -ShowProgress
        }
        "extract" {
            Write-ColorOutput "Using cursor_dump..." "Reverse"
            . "D:\lazy init ide\cursor_dump.ps1"
        }
        "deobfuscate" {
            Write-ColorOutput "Using Reverse-Engineer-Advanced..." "Reverse"
            . "D:\lazy init ide\Reverse-Engineer-Advanced.ps1" -InputDirectory $Target -ReverseObfuscated -DecompileJavaScript
        }
        "all" {
            Write-ColorOutput "Running complete analysis suite..." "Header"
            . "D:\lazy init ide\ReverseEngineeringEngine.ps1" -TargetPath $Target -AnalyzeBinaries -ExtractAPIs -MapDependencies -GenerateDocumentation
        }
    }
}

# Main execution
Write-ColorOutput "═══════════════════════════════════════════════════" "Header"
Write-ColorOutput "  CLI REVERSE ENGINEERING INTEGRATION SUITE" "Header"
Write-ColorOutput "═══════════════════════════════════════════════════" "Header"
Write-ColorOutput ""

$arch = Get-Architecture
Write-ColorOutput "Detected Architecture: $arch-bit" "Info"
Write-ColorOutput "Action: $Action" "Info"
Write-ColorOutput "Target: $Target" "Info"
Write-ColorOutput ""

# Try CLI build first, fall back to reverse engineering
$success = $false
if ($Action -eq "build") {
    $success = Invoke-CLIBuild -Arch $arch
}

if (-not $success) {
    Invoke-ReverseEngineering -Arch $arch
}

Write-ColorOutput ""
Write-ColorOutput "═══════════════════════════════════════════════════" "Header"
Write-ColorOutput "  OPERATION COMPLETE" "Success"
Write-ColorOutput "═══════════════════════════════════════════════════" "Header"
