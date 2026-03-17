#!/usr/bin/env powershell
# Build and Deploy GPU DLSS System
# Comprehensive build script for Windows x64 MASM compilation

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("build", "test", "clean", "deploy", "full")]
    [string]$Action = "build",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "./bin",
    
    [Parameter(Mandatory=$false)]
    [switch]$NoTest = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$Verbose = $false
)

# Enable strict error handling
$ErrorActionPreference = "Stop"
$InformationPreference = "Continue"

# Configuration
$MASM_DIR = "d:\RawrXD-production-lazy-init\src\masm\final-ide"
$CONFIG_DIR = "d:\RawrXD-production-lazy-init\config"
$TEST_DIR = "d:\RawrXD-production-lazy-init\tests"
$BUILD_LOG = "$OutputDir\build.log"
$TIMESTAMP = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"

# Ensure output directory exists
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

function Write-Status {
    param([string]$Message, [string]$Level = "INFO")
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARN" { "Yellow" }
        "SUCCESS" { "Green" }
        default { "White" }
    }
    Write-Host "[$TIMESTAMP] [$Level] $Message" -ForegroundColor $color
    Add-Content -Path $BUILD_LOG -Value "[$TIMESTAMP] [$Level] $Message"
}

function Test-MASMTools {
    Write-Status "Checking MASM x64 tools..."
    
    $ml64 = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if (-not $ml64) {
        Write-Status "ERROR: ml64.exe not found in PATH" "ERROR"
        Write-Status "Install MASM32 SDK or add to PATH" "ERROR"
        exit 1
    }
    
    $link = Get-Command link.exe -ErrorAction SilentlyContinue
    if (-not $link) {
        Write-Status "ERROR: link.exe not found in PATH" "ERROR"
        exit 1
    }
    
    Write-Status "Found ml64.exe: $(($ml64).Source)" "SUCCESS"
    Write-Status "Found link.exe: $(($link).Source)" "SUCCESS"
}

function Compile-MASM {
    param(
        [string]$SourceFile,
        [string]$OutputFile
    )
    
    Write-Status "Compiling: $SourceFile"
    
    if (-not (Test-Path $SourceFile)) {
        Write-Status "ERROR: Source file not found: $SourceFile" "ERROR"
        return $false
    }
    
    # Compile with debug info
    $compileCmd = "ml64.exe /c /Zd /I`"$MASM_DIR`" /Fo`"$OutputFile`" `"$SourceFile`""
    
    if ($Verbose) {
        Write-Status "Command: $compileCmd"
    }
    
    $output = & cmd.exe /c $compileCmd 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Status "Compilation FAILED: $SourceFile" "ERROR"
        Write-Status $output "ERROR"
        return $false
    }
    
    if (Test-Path $OutputFile) {
        $size = (Get-Item $OutputFile).Length
        Write-Status "OK: $OutputFile ($size bytes)" "SUCCESS"
        return $true
    } else {
        Write-Status "ERROR: Output file not created: $OutputFile" "ERROR"
        return $false
    }
}

function Create-LinkResponse {
    param([string]$ResponseFile)
    
    Write-Status "Creating link response file: $ResponseFile"
    
    $content = @"
/SUBSYSTEM:CONSOLE
/MACHINE:X64
kernel32.lib
advapi32.lib
user32.lib
ws2_32.lib
/DEBUG
/DEBUGTYPE:cv
"@
    
    Set-Content -Path $ResponseFile -Value $content -Encoding ASCII
    Write-Status "Response file created" "SUCCESS"
}

function Link-Objects {
    param(
        [string[]]$ObjectFiles,
        [string]$OutputLib
    )
    
    Write-Status "Linking objects to: $OutputLib"
    
    # Create response file
    $responseFile = "$OutputDir\link.rsp"
    Create-LinkResponse $responseFile
    
    # Build object file list
    $objList = $ObjectFiles -join " "
    
    # Build link command
    $linkCmd = "link.exe @`"$responseFile`" $objList /OUT:`"$OutputLib`""
    
    if ($Verbose) {
        Write-Status "Command: $linkCmd"
    }
    
    $output = & cmd.exe /c $linkCmd 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Status "Linking FAILED" "ERROR"
        Write-Status $output "ERROR"
        return $false
    }
    
    if (Test-Path $OutputLib) {
        $size = (Get-Item $OutputLib).Length
        Write-Status "OK: $OutputLib ($size bytes)" "SUCCESS"
        return $true
    } else {
        Write-Status "ERROR: Output library not created: $OutputLib" "ERROR"
        return $false
    }
}

function Build-GPU-DLSS {
    Write-Status "========== BUILD GPU DLSS SYSTEM ==========="
    
    # Test tools first
    Test-MASMTools
    
    # Source files
    $sourceFiles = @(
        "gpu_dlss_abstraction.asm",
        "gpu_model_loader_optimized.asm",
        "gpu_observability.asm",
        "gpu_dlss_tests.asm"
    )
    
    # Object files (will be created)
    $objFiles = @()
    
    # Compile each source
    Write-Status "Starting compilation phase..."
    foreach ($src in $sourceFiles) {
        $srcPath = Join-Path $MASM_DIR $src
        $objPath = Join-Path $OutputDir $($src -replace '\.asm$', '.obj')
        
        if (Compile-MASM $srcPath $objPath) {
            $objFiles += $objPath
        } else {
            Write-Status "Build FAILED at: $src" "ERROR"
            return $false
        }
    }
    
    Write-Status "Compilation complete. Linking phase..."
    
    # Link all objects
    $outputLib = Join-Path $OutputDir "gpu_dlss_runtime.lib"
    if (Link-Objects $objFiles $outputLib) {
        Write-Status "Build SUCCESSFUL!" "SUCCESS"
        return $true
    } else {
        Write-Status "Build FAILED during linking" "ERROR"
        return $false
    }
}

function Run-Tests {
    Write-Status "========== RUNNING TESTS ==========="
    
    $testExe = Join-Path $OutputDir "gpu_dlss_tests.exe"
    
    if (-not (Test-Path $testExe)) {
        Write-Status "Test executable not found: $testExe" "WARN"
        Write-Status "Skipping tests (run build action first)" "WARN"
        return $true
    }
    
    Write-Status "Executing: $testExe"
    
    $testOutput = & $testExe 2>&1
    $testResult = $LASTEXITCODE
    
    Write-Status $testOutput
    
    if ($testResult -eq 0) {
        Write-Status "All tests PASSED" "SUCCESS"
        return $true
    } else {
        Write-Status "Tests FAILED (exit code: $testResult)" "ERROR"
        return $false
    }
}

function Validate-Build {
    Write-Status "========== VALIDATING BUILD ==========="
    
    $requiredFiles = @(
        "gpu_dlss_runtime.lib"
    )
    
    $allValid = $true
    foreach ($file in $requiredFiles) {
        $filePath = Join-Path $OutputDir $file
        if (Test-Path $filePath) {
            $fileSize = (Get-Item $filePath).Length
            Write-Status "✓ $file ($fileSize bytes)" "SUCCESS"
        } else {
            Write-Status "✗ $file NOT FOUND" "ERROR"
            $allValid = $false
        }
    }
    
    return $allValid
}

function Clean-Build {
    Write-Status "========== CLEANING BUILD ==========="
    
    $patterns = @("*.obj", "*.lib", "*.exe", "*.pdb", "*.ilk")
    
    foreach ($pattern in $patterns) {
        Get-ChildItem -Path $OutputDir -Filter $pattern -ErrorAction SilentlyContinue | 
            Remove-Item -Force -ErrorAction SilentlyContinue
        Write-Status "Cleaned: $pattern"
    }
    
    Write-Status "Clean complete" "SUCCESS"
}

function Deploy-Docker {
    Write-Status "========== DEPLOYING DOCKER IMAGE ==========="
    
    # Check if Docker is available
    $docker = Get-Command docker -ErrorAction SilentlyContinue
    if (-not $docker) {
        Write-Status "Docker not found - cannot build image" "WARN"
        return $false
    }
    
    Write-Status "Building Docker image..."
    $dockerCmd = "docker build -f Dockerfile -t rawrxd-gpu:$TIMESTAMP ."
    
    if ($Verbose) {
        Write-Status "Command: $dockerCmd"
    }
    
    $output = & cmd.exe /c $dockerCmd 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Status "Docker build FAILED" "ERROR"
        Write-Status $output "ERROR"
        return $false
    }
    
    Write-Status "Docker image built successfully" "SUCCESS"
    Write-Status "Tag: rawrxd-gpu:$TIMESTAMP" "SUCCESS"
    
    return $true
}

function Show-Summary {
    Write-Status "========== BUILD SUMMARY ==========="
    Write-Status "Output Directory: $OutputDir"
    Write-Status "Build Log: $BUILD_LOG"
    
    # Show generated files
    if (Test-Path $OutputDir) {
        Write-Status "Generated files:"
        Get-ChildItem -Path $OutputDir -File | ForEach-Object {
            $size = $_.Length
            Write-Status "  $($_.Name) - $size bytes"
        }
    }
}

# Main execution
Write-Status "========== GPU DLSS BUILD SYSTEM ==========="
Write-Status "Action: $Action"
Write-Status "Output: $OutputDir"
Write-Status "Verbose: $Verbose"

$buildSuccess = $true

switch ($Action) {
    "build" {
        $buildSuccess = Build-GPU-DLSS
    }
    
    "test" {
        $buildSuccess = Build-GPU-DLSS
        if ($buildSuccess -and -not $NoTest) {
            $buildSuccess = Run-Tests
        }
    }
    
    "clean" {
        Clean-Build
    }
    
    "deploy" {
        $buildSuccess = Build-GPU-DLSS
        if ($buildSuccess) {
            $buildSuccess = Deploy-Docker
        }
    }
    
    "full" {
        $buildSuccess = Build-GPU-DLSS
        if ($buildSuccess) {
            $buildSuccess = Validate-Build
        }
        if ($buildSuccess -and -not $NoTest) {
            $buildSuccess = Run-Tests
        }
        if ($buildSuccess) {
            $buildSuccess = Deploy-Docker
        }
    }
    
    default {
        Write-Status "Unknown action: $Action" "ERROR"
        exit 1
    }
}

# Show summary
Show-Summary

# Final result
if ($buildSuccess) {
    Write-Status "========== BUILD COMPLETE ==========" "SUCCESS"
    exit 0
} else {
    Write-Status "========== BUILD FAILED ==========" "ERROR"
    exit 1
}
