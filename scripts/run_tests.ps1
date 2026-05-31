#!/bin/bash
# ============================================================================
# RawrXD Test Runner - PowerShell Version for Windows
# ============================================================================

$ErrorActionPreference = "Stop"

# Colors
$Red = "`e[31m"
$Green = "`e[32m"
$Yellow = "`e[33m"
$Blue = "`e[34m"
$Reset = "`e[0m"

# Paths
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Join-Path $ScriptDir ".."
$BuildDir = Join-Path $ProjectRoot "build"
$TestDir = Join-Path $BuildDir "tests"

# Defaults
$TestCategory = ""
$Reporter = "console"
$Verbosity = 2
$BenchmarkSamples = 100
$ParallelJobs = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors

# Help function
function Show-Help {
    Write-Host @"
RawrXD Test Runner

Usage: .\run_tests.ps1 [OPTIONS]

OPTIONS:
    -Help                   Show this help message
    -Category CAT           Run tests by category: unit, integration, performance, all
    -Reporter REP           Test reporter: console, compact, junit, xml
    -Verbosity LEVEL        Verbosity level (0-3)
    -Jobs N                 Parallel jobs (default: $ParallelJobs)
    -Build                  Build tests before running
    -Samples N              Benchmark samples (default: $BenchmarkSamples)
    -Coverage               Generate coverage report
    -ListTests              List all available tests
    -ListTags               List all available tags
    -Tag TAG                Run tests with specific tag
    -CI                     CI mode (minimal output, exit codes only)
    -OutputFile FILE        Output file for test results

EXAMPLES:
    .\run_tests.ps1                        Run all tests
    .\run_tests.ps1 -Category unit         Run only unit tests
    .\run_tests.ps1 -Category performance  Run performance benchmarks
    .\run_tests.ps1 -Tag "[search]"       Run tests tagged with [search]
    .\run_tests.ps1 -ListTests             List all tests
    .\run_tests.ps1 -Reporter junit -OutputFile tests.xml
"@
}

# Parse arguments
param(
    [switch]$Help,
    [string]$Category,
    [string]$Reporter = "console",
    [int]$Verbosity = 2,
    [int]$Jobs = $ParallelJobs,
    [switch]$Build,
    [int]$Samples = 100,
    [switch]$Coverage,
    [switch]$ListTests,
    [switch]$ListTags,
    [string]$Tag,
    [switch]$CI,
    [string]$OutputFile
)

if ($Help) {
    Show-Help
    exit 0
}

# Set CI mode options
if ($CI) {
    $Reporter = "compact"
    $Verbosity = 0
}

# Build function
function Build-Tests {
    Write-Host "${Blue}Building RawrXD tests...${Reset}"
    
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    Push-Location $BuildDir
    
    try {
        Write-Host "${Yellow}Configuring...${Reset}"
        cmake .. -DBUILD_TESTING=ON `
                 -DCMAKE_BUILD_TYPE=Release `
                 -DCMAKE_CXX_STANDARD=17 `
                 -G "Visual Studio 17 2022" -A x64
        
        Write-Host "${Yellow}Building with $ParallelJobs jobs...${Reset}"
        cmake --build . --target RawrXD_Tests --config Release -j$ParallelJobs
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "${Red}Build failed!${Reset}"
            exit 1
        }
        
        Write-Host "${Green}Build completed successfully!${Reset}"
    }
    finally {
        Pop-Location
    }
}

# Find test executable
function Find-TestExecutable {
    $testExe = $null
    
    $possiblePaths = @(
        (Join-Path $TestDir "RawrXD_Tests.exe"),
        (Join-Path $TestDir "Release\RawrXD_Tests.exe"),
        (Join-Path $BuildDir "Release\RawrXD_Tests.exe"),
        (Join-Path $BuildDir "tests\Release\RawrXD_Tests.exe")
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $testExe = $path
            break
        }
    }
    
    if (-not $testExe) {
        Write-Host "${Red}Test executable not found!${Reset}"
        return $null
    }
    
    return $testExe
}

# Run tests
function Run-Tests {
    $testExe = Find-TestExecutable
    if (-not $testExe) {
        return 1
    }
    
    Write-Host "${Blue}Running tests: $testExe${Reset}"
    
    # Build command arguments
    $cmdArgs = @()
    
    # Add category/tag filter
    if ($Tag) {
        $cmdArgs += $Tag
    }
    elseif ($Category) {
        switch ($Category) {
            "unit" { $cmdArgs += '"[unit]"' }
            "integration" { $cmdArgs += '"[integration]"' }
            "performance" { 
                $cmdArgs += '"[performance]"'
                $cmdArgs += "--benchmark-samples"
                $cmdArgs += $BenchmarkSamples
            }
            default { /* all tests */ }
        }
    }
    
    # Add reporter
    $cmdArgs += "--reporter"
    $cmdArgs += $Reporter
    
    # Add verbosity
    $cmdArgs += "--verbosity"
    $cmdArgs += $Verbosity
    
    # Add output file
    if ($OutputFile) {
        $cmdArgs += "--out"
        $cmdArgs += $OutputFile
    }
    
    # Execute
    if (-not $CI) {
        Write-Host "${Yellow}Command: $testExe $cmdArgs${Reset}"
    }
    
    & $testExe $cmdArgs
    return $LASTEXITCODE
}

# List tests
function List-Tests {
    $testExe = Find-TestExecutable
    if (-not $testExe) {
        return 1
    }
    
    Write-Host "${Blue}Available tests:${Reset}"
    & $testExe --list-tests
}

# List tags
function List-Tags {
    $testExe = Find-TestExecutable
    if (-not $testExe) {
        return 1
    }
    
    Write-Host "${Blue}Available tags:${Reset}"
    & $testExe --list-tags
}

# Main execution
function Main {
    # Build if requested
    if ($Build) {
        Build-Tests
    }
    
    # List mode
    if ($ListTests) {
        List-Tests
        exit 0
    }
    
    if ($ListTags) {
        List-Tags
        exit 0
    }
    
    # Run tests
    $exitCode = Run-Tests
    
    # Summary
    if (-not $CI) {
        if ($exitCode -eq 0) {
            Write-Host "${Green}✅ All tests passed!${Reset}"
        }
        else {
            Write-Host "${Red}❌ Some tests failed!${Reset}"
        }
    }
    
    exit $exitCode
}

# Run main
Main
