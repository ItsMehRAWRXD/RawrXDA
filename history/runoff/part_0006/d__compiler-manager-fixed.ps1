#!/usr/bin/env pwsh
<#
.SYNOPSIS
RawrXD IDE - Universal Compiler Manager for Windows
Manages compiler detection, configuration, and build orchestration

.DESCRIPTION
Provides comprehensive compiler management for RawrXD IDE including:
- Compiler detection (MSVC, GCC, Clang, Qt)
- Build environment validation
- CMake configuration and building
- Test execution
- System audit and diagnostics

.PARAMETER Audit
Run full system audit of compiler and build environment

.PARAMETER Status
Show current compiler and build system status

.PARAMETER Setup
Setup and configure compiler environment

.PARAMETER Build
Build the project using CMake

.PARAMETER Test
Run test suite

.PARAMETER Compiler
Specify compiler to use: auto, msvc, gcc, clang (default: auto)

.PARAMETER Config
Build configuration: Debug, Release (default: Debug)

.PARAMETER BuildDir
Custom build directory (default: ./build)

.EXAMPLE
./compiler-manager-fixed.ps1 -Audit
# Performs full system audit

.EXAMPLE
./compiler-manager-fixed.ps1 -Build -Config Release
# Builds project in Release configuration

#>

param(
    [switch]$Audit,
    [switch]$Status,
    [switch]$Setup,
    [switch]$Build,
    [switch]$Test,
    [string]$Compiler = "auto",
    [string]$Config = "Debug",
    [string]$BuildDir = "./build"
)

# ============================================================
# Helper Functions
# ============================================================

function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Status {
    param([string]$Message, [string]$Status = "INFO")
    $colors = @{
        "INFO" = "White"
        "SUCCESS" = "Green"
        "WARN" = "Yellow"
        "ERROR" = "Red"
    }
    $icon = @{
        "INFO" = "ℹ"
        "SUCCESS" = "✓"
        "WARN" = "⚠"
        "ERROR" = "✗"
    }
    Write-Host "  $($icon[$Status]) $Message" -ForegroundColor $colors[$Status]
}

# ============================================================
# Compiler Detection
# ============================================================

function Find-Compiler {
    param([string]$CompilerName)
    
    $compilers = @{}
    
    # MSVC paths
    if ($CompilerName -eq "auto" -or $CompilerName -eq "msvc") {
        $msvc_paths = @(
            "C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
            "C:\Program Files (x86)\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
            "C:\Program Files\LLVM\bin\clang-cl.exe"
        )
        
        foreach ($pattern in $msvc_paths) {
            $matches = Get-Item $pattern -ErrorAction SilentlyContinue
            if ($matches) {
                $matches | ForEach-Object { $compilers["msvc"] = $_.FullName }
            }
        }
    }
    
    # GCC/MinGW paths
    if ($CompilerName -eq "auto" -or $CompilerName -eq "gcc") {
        $gcc_paths = @(
            "C:\mingw64\bin\g++.exe",
            "C:\mingw\bin\g++.exe",
            "C:\Program Files\MinGW\bin\g++.exe",
            "C:\Program Files (x86)\MinGW\bin\g++.exe",
            "C:\ProgramData\mingw64\mingw64\bin\g++.exe"
        )
        
        foreach ($path in $gcc_paths) {
            if (Test-Path $path) {
                $compilers["gcc"] = $path
            }
        }
    }
    
    # Clang paths
    if ($CompilerName -eq "auto" -or $CompilerName -eq "clang") {
        $clang_paths = @(
            "C:\Program Files\LLVM\bin\clang++.exe",
            "C:\Program Files (x86)\LLVM\bin\clang++.exe"
        )
        
        foreach ($path in $clang_paths) {
            if (Test-Path $path) {
                $compilers["clang"] = $path
            }
        }
    }
    
    # Check PATH environment variable
    $env:PATH -split ";" | ForEach-Object {
        if ($_ -and (Test-Path $_)) {
            if (Test-Path "$_\cl.exe" -and -not $compilers.ContainsKey("msvc")) {
                $compilers["msvc"] = "$_\cl.exe"
            }
            if (Test-Path "$_\g++.exe" -and -not $compilers.ContainsKey("gcc")) {
                $compilers["gcc"] = "$_\g++.exe"
            }
            if (Test-Path "$_\clang++.exe" -and -not $compilers.ContainsKey("clang")) {
                $compilers["clang"] = "$_\clang++.exe"
            }
        }
    }
    
    return $compilers
}

function Get-CompilerInfo {
    param([string]$CompilerPath)
    
    try {
        if ($CompilerPath -match "cl.exe") {
            # MSVC version
            $output = & $CompilerPath 2>&1
            if ($output -match "(\d+\.\d+)") {
                return @{ 
                    Name = "MSVC"
                    Version = $matches[1]
                    Path = $CompilerPath
                }
            }
        }
        elseif ($CompilerPath -match "g\+\+") {
            # GCC version
            $output = & $CompilerPath --version 2>&1 | Select-Object -First 1
            if ($output -match "(\d+\.\d+\.\d+)") {
                return @{ 
                    Name = "GCC"
                    Version = $matches[1]
                    Path = $CompilerPath
                }
            }
        }
        elseif ($CompilerPath -match "clang") {
            # Clang version
            $output = & $CompilerPath --version 2>&1 | Select-Object -First 1
            if ($output -match "version (\d+\.\d+\.\d+)") {
                return @{ 
                    Name = "Clang"
                    Version = $matches[1]
                    Path = $CompilerPath
                }
            }
        }
    }
    catch {
        # Compiler not accessible
    }
    
    return @{ 
        Name = "Unknown"
        Version = "Unknown"
        Path = $CompilerPath
    }
}

# ============================================================
# Build Environment Validation
# ============================================================

function Test-BuildEnvironment {
    Write-Header "Build Environment Validation"
    
    $env_ok = $true
    
    # Check CMake
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        $version = & cmake --version 2>&1 | Select-Object -First 1
        Write-Status "CMake: $version" "SUCCESS"
    } else {
        Write-Status "CMake: NOT FOUND" "ERROR"
        $env_ok = $false
    }
    
    # Check compiler
    $compilers = Find-Compiler "auto"
    if ($compilers.Count -gt 0) {
        foreach ($key in $compilers.Keys) {
            Write-Status "Compiler detected: $key at $($compilers[$key])" "SUCCESS"
        }
    } else {
        Write-Status "No compilers detected" "ERROR"
        $env_ok = $false
    }
    
    # Check project files
    if (Test-Path "CMakeLists.txt") {
        Write-Status "CMakeLists.txt found" "SUCCESS"
    } else {
        Write-Status "CMakeLists.txt not found" "WARN"
    }
    
    # Check build directory
    if (Test-Path $BuildDir) {
        Write-Status "Build directory exists: $BuildDir" "SUCCESS"
    } else {
        Write-Status "Build directory will be created" "INFO"
    }
    
    # Check Git
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        Write-Status "Git available" "SUCCESS"
    } else {
        Write-Status "Git not available" "WARN"
    }
    
    return $env_ok
}

# ============================================================
# System Audit
# ============================================================

function Run-FullAudit {
    Write-Header "RawrXD IDE - Full System Audit"
    
    Write-Status "Windows Version: $([System.Environment]::OSVersion.VersionString)" "INFO"
    Write-Status "PowerShell Version: $($PSVersionTable.PSVersion)" "INFO"
    Write-Status "Current Directory: $(Get-Location)" "INFO"
    Write-Host ""
    
    # Environment validation
    $env_ok = Test-BuildEnvironment
    
    Write-Host ""
    Write-Header "Compiler Details"
    
    $compilers = Find-Compiler "auto"
    if ($compilers.Count -eq 0) {
        Write-Status "No compilers found in system PATH" "ERROR"
    } else {
        foreach ($key in $compilers.Keys) {
            $info = Get-CompilerInfo $compilers[$key]
            Write-Status "$($info.Name) $($info.Version) - $($info.Path)" "SUCCESS"
        }
    }
    
    Write-Host ""
    Write-Header "Project Information"
    
    if (Test-Path "CMakeLists.txt") {
        $content = Get-Content "CMakeLists.txt" -TotalCount 20
        $project_name = $content | Where-Object { $_ -match "project\(" } | Select-Object -First 1
        if ($project_name) {
            Write-Status "Project: $project_name" "INFO"
        }
    }
    
    $cpp_files = @(Get-ChildItem -Path "src" -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue)
    $h_files = @(Get-ChildItem -Path "src" -Filter "*.h" -Recurse -ErrorAction SilentlyContinue)
    
    Write-Status "C++ Source Files: $($cpp_files.Count)" "INFO"
    Write-Status "Header Files: $($h_files.Count)" "INFO"
    
    Write-Host ""
    Write-Header "System Summary"
    
    if ($env_ok) {
        Write-Status "Build environment: READY" "SUCCESS"
    } else {
        Write-Status "Build environment: ISSUES DETECTED" "WARN"
    }
    
    Write-Host ""
}

# ============================================================
# Build Management
# ============================================================

function Invoke-Build {
    param(
        [string]$Configuration = "Debug"
    )
    
    Write-Header "Building RawrXD IDE ($Configuration)"
    
    # Configure
    Write-Status "Configuring CMake..." "INFO"
    $cmake_args = @(
        "-B", $BuildDir,
        "-DCMAKE_BUILD_TYPE=$Configuration"
    )
    
    & cmake @cmake_args
    
    if ($LASTEXITCODE -eq 0) {
        Write-Status "CMake configuration successful" "SUCCESS"
    } else {
        Write-Status "CMake configuration failed" "ERROR"
        return $false
    }
    
    # Build
    Write-Status "Building..." "INFO"
    $build_args = @(
        "--build", $BuildDir,
        "--config", $Configuration,
        "--parallel"
    )
    
    & cmake @build_args
    
    if ($LASTEXITCODE -eq 0) {
        Write-Status "Build successful" "SUCCESS"
        return $true
    } else {
        Write-Status "Build failed" "ERROR"
        return $false
    }
}

function Get-CompilerStatus {
    Write-Header "Compiler Status"
    
    $compilers = Find-Compiler "auto"
    
    if ($compilers.Count -eq 0) {
        Write-Status "No compilers detected" "ERROR"
        return
    }
    
    foreach ($key in $compilers.Keys) {
        $info = Get-CompilerInfo $compilers[$key]
        Write-Status "$($info.Name) $($info.Version)" "SUCCESS"
    }
}

# ============================================================
# Main Entry Point
# ============================================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD IDE - Universal Compiler Manager          ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

if ($Audit) {
    Run-FullAudit
}
elseif ($Status) {
    Get-CompilerStatus
}
elseif ($Setup) {
    Write-Header "Setting Up Compiler Environment"
    Write-Status "Compiler environment setup" "INFO"
    $env_ok = Test-BuildEnvironment
    if ($env_ok) {
        Write-Status "Environment ready for building" "SUCCESS"
    }
}
elseif ($Build) {
    $success = Invoke-Build -Configuration $Config
    if ($success) {
        Write-Status "Build completed successfully" "SUCCESS"
    }
}
elseif ($Test) {
    Write-Header "Running Tests"
    Write-Status "Test execution not yet implemented" "WARN"
}
else {
    Write-Host @"
RawrXD IDE - Universal Compiler Manager

USAGE:
    ./compiler-manager-fixed.ps1 -Audit
    ./compiler-manager-fixed.ps1 -Status
    ./compiler-manager-fixed.ps1 -Setup
    ./compiler-manager-fixed.ps1 -Build -Config Release

PARAMETERS:
    -Audit                  Run complete system audit
    -Status                 Show compiler status
    -Setup                  Setup compiler environment
    -Build                  Build the project
    -Test                   Run tests
    -Compiler <name>        Compiler: auto, msvc, gcc, clang (default: auto)
    -Config <name>          Configuration: Debug, Release (default: Debug)
    -BuildDir <path>        Custom build directory (default: ./build)

EXAMPLES:
    # Full system audit
    ./compiler-manager-fixed.ps1 -Audit
    
    # Show status
    ./compiler-manager-fixed.ps1 -Status
    
    # Build Release configuration
    ./compiler-manager-fixed.ps1 -Build -Config Release
"@ -ForegroundColor White
}

Write-Host ""
