# RawrXD Turnkey Environment Setup
# Automatically detects and configures build environment
# Usage: .\scripts\turnkey\Setup-Environment.ps1 [-InstallIfMissing]

[CmdletBinding()]
param(
    [switch]$InstallIfMissing,
    [switch]$ForceReinstall,
    [string]$LogPath = "$env:TEMP\rawrxd-env-setup.log"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "Continue"

# Configuration
$script:RequiredComponents = @(
    @{ Name = "MSVC v143 - VS 2022 C++ x64/x86 build tools"; ID = "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"; Required = $true },
    @{ Name = "Windows 10/11 SDK"; ID = "Microsoft.VisualStudio.Component.Windows10SDK"; Required = $true },
    @{ Name = "C++ CMake tools for Windows"; ID = "Microsoft.VisualStudio.Component.VC.CMake.Project"; Required = $false },
    @{ Name = "C++ AddressSanitizer"; ID = "Microsoft.VisualStudio.Component.VC.ASAN"; Required = $false }
)

$script:MinWindowsSDK = "10.0.19041.0"
$script:LogFile = $LogPath

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"
    Add-Content -Path $script:LogFile -Value $logEntry -ErrorAction SilentlyContinue
    switch ($Level) {
        "ERROR" { Write-Host $logEntry -ForegroundColor Red }
        "WARN"  { Write-Host $logEntry -ForegroundColor Yellow }
        "SUCCESS" { Write-Host $logEntry -ForegroundColor Green }
        default { Write-Host $logEntry }
    }
}

function Test-Administrator {
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Find-VisualStudio {
    Write-Log "Searching for Visual Studio installations..."
    
    $vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWherePath)) {
        $vsWherePath = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    }
    
    if (-not (Test-Path $vsWherePath)) {
        Write-Log "vswhere.exe not found - VS may not be installed" "WARN"
        return $null
    }
    
    # Find VS 2022 or later with C++ workload
    $vsInstallations = & $vsWherePath -version "[17.0,18.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -format json 2>$null | ConvertFrom-Json
    
    if (-not $vsInstallations) {
        Write-Log "No VS 2022 with C++ tools found" "WARN"
        return $null
    }
    
    # Return the first installation (prefer Enterprise/Professional over Community)
    $preferredInstall = $vsInstallations | Sort-Object { 
        $edition = $_ -replace '.*\\(Community|Professional|Enterprise)$', '$1'
        switch ($edition) {
            "Enterprise" { return 1 }
            "Professional" { return 2 }
            default { return 3 }
        }
    } | Select-Object -First 1
    
    Write-Log "Found VS installation: $preferredInstall" "SUCCESS"
    return $preferredInstall
}

function Get-VSBuildToolsPath {
    param([string]$VSPath)
    
    $vcvarsPath = Join-Path $VSPath "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $vcvarsPath) {
        return $vcvarsPath
    }
    
    # Try to find ml64.exe directly
    $ml64Paths = @(
        "$VSPath\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
        "$VSPath\VC\bin\amd64\ml64.exe"
    )
    
    foreach ($path in $ml64Paths) {
        $found = Resolve-Path $path -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            return $found.Path
        }
    }
    
    return $null
}

function Test-BuildTools {
    Write-Log "Testing build tools availability..."
    
    $results = @{
        Ml64 = $false
        Link = $false
        Cl = $false
        CMake = $false
        WindowsSDK = $false
        Paths = @{}
    }
    
    # Check ml64.exe
    $ml64 = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if ($ml64) {
        $results.Ml64 = $true
        $results.Paths.Ml64 = $ml64.Source
        Write-Log "  ✓ ml64.exe: $($ml64.Source)" "SUCCESS"
    } else {
        Write-Log "  ✗ ml64.exe: NOT FOUND" "ERROR"
    }
    
    # Check link.exe
    $link = Get-Command link.exe -ErrorAction SilentlyContinue
    if ($link) {
        $results.Link = $true
        $results.Paths.Link = $link.Source
        Write-Log "  ✓ link.exe: $($link.Source)" "SUCCESS"
    } else {
        Write-Log "  ✗ link.exe: NOT FOUND" "ERROR"
    }
    
    # Check cl.exe
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($cl) {
        $results.Cl = $true
        $results.Paths.Cl = $cl.Source
        Write-Log "  ✓ cl.exe: $($cl.Source)" "SUCCESS"
    } else {
        Write-Log "  ✗ cl.exe: NOT FOUND" "ERROR"
    }
    
    # Check cmake
    $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($cmake) {
        $results.CMake = $true
        $results.Paths.CMake = $cmake.Source
        Write-Log "  ✓ cmake.exe: $($cmake.Source)" "SUCCESS"
    } else {
        Write-Log "  ✗ cmake.exe: NOT FOUND (optional)" "WARN"
    }
    
    # Check Windows SDK
    $sdkPaths = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\Include\10.0.*",
        "${env:ProgramFiles}\Windows Kits\10\Include\10.0.*"
    )
    
    foreach ($sdkPath in $sdkPaths) {
        $foundSdk = Get-Item $sdkPath -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
        if ($foundSdk) {
            $results.WindowsSDK = $true
            $results.Paths.WindowsSDK = $foundSdk.FullName
            Write-Log "  ✓ Windows SDK: $($foundSdk.FullName)" "SUCCESS"
            break
        }
    }
    
    if (-not $results.WindowsSDK) {
        Write-Log "  ✗ Windows SDK: NOT FOUND" "ERROR"
    }
    
    return $results
}

function Install-BuildTools {
    Write-Log "Installing Visual Studio Build Tools..."
    Write-Log "This requires administrator privileges and may take 15-30 minutes" "WARN"
    
    if (-not (Test-Administrator)) {
        throw "Administrator privileges required to install build tools. Please run as Administrator."
    }
    
    $installerUrl = "https://aka.ms/vs/17/release/vs_buildtools.exe"
    $installerPath = "$env:TEMP\vs_buildtools.exe"
    
    try {
        Write-Log "Downloading VS Build Tools installer..."
        Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath -UseBasicParsing
        
        $workloadArgs = @(
            "--quiet",
            "--wait",
            "--add", "Microsoft.VisualStudio.Workload.VCTools",
            "--add", "Microsoft.VisualStudio.Component.Windows10SDK.19041",
            "--includeRecommended"
        )
        
        Write-Log "Installing components (this will take a while)..."
        $process = Start-Process -FilePath $installerPath -ArgumentList $workloadArgs -Wait -PassThru
        
        if ($process.ExitCode -ne 0) {
            throw "Installation failed with exit code: $($process.ExitCode)"
        }
        
        Write-Log "Build Tools installation completed" "SUCCESS"
    }
    finally {
        if (Test-Path $installerPath) {
            Remove-Item $installerPath -Force -ErrorAction SilentlyContinue
        }
    }
}

function Set-EnvironmentVariables {
    param([string]$VSPath)
    
    Write-Log "Configuring environment variables..."
    
    # Import VS environment
    $vcvarsPath = Join-Path $VSPath "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $vcvarsPath) {
        # Parse the batch file to get environment variables
        $tempFile = [System.IO.Path]::GetTempFileName()
        cmd /c "`"$vcvarsPath`" && set > `"$tempFile`"" 2>$null
        
        if (Test-Path $tempFile) {
            Get-Content $tempFile | ForEach-Object {
                if ($_ -match "^(\w+)=(.*)$") {
                    $name = $matches[1]
                    $value = $matches[2]
                    [Environment]::SetEnvironmentVariable($name, $value, "Process")
                }
            }
            Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
            Write-Log "  ✓ Imported VS environment" "SUCCESS"
        }
    }
    
    # Set RAWRXD_ROOT if not set
    if (-not $env:RAWRXD_ROOT) {
        $scriptPath = $PSScriptRoot
        $rawrxdRoot = Resolve-Path "$scriptPath\..\.." -ErrorAction SilentlyContinue
        if ($rawrxdRoot) {
            [Environment]::SetEnvironmentVariable("RAWRXD_ROOT", $rawrxdRoot, "User")
            Write-Log "  ✓ Set RAWRXD_ROOT=$rawrxdRoot" "SUCCESS"
        }
    }
}

function New-EnvironmentScript {
    param([string]$VSPath)
    
    $scriptContent = @"
@echo off
REM RawrXD Build Environment Setup
REM Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

set "VS_PATH=$VSPath"
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"

if errorlevel 1 (
    echo Failed to initialize VS environment
    exit /b 1
)

echo RawrXD build environment ready
echo.
echo Available tools:
where ml64.exe && echo   - ml64.exe (MASM assembler)
where link.exe && echo   - link.exe (Linker)
where cl.exe && echo   - cl.exe (C++ compiler)
where cmake.exe && echo   - cmake.exe (Build system)
"@
    
    $scriptPath = Join-Path $PSScriptRoot "..\..\Setup-BuildEnv.bat"
    $scriptContent | Out-File -FilePath $scriptPath -Encoding ASCII
    Write-Log "  ✓ Created: $scriptPath" "SUCCESS"
    
    return $scriptPath
}

# Main execution
Write-Log "=== RawrXD Turnkey Environment Setup ==="
Write-Log "Log file: $LogFile"

$exitCode = 0
try {
    # Check admin for installation
    if ($InstallIfMissing -and -not (Test-Administrator)) {
        Write-Log "Administrator privileges required for installation" "ERROR"
        Write-Log "Please run: Start-Process PowerShell -Verb RunAs -ArgumentList '-File `"$PSCommandPath`" -InstallIfMissing'"
        exit 1
    }
    
    # Step 1: Find Visual Studio
    $vsPath = Find-VisualStudio
    
    if (-not $vsPath -and $InstallIfMissing) {
        Install-BuildTools
        $vsPath = Find-VisualStudio
    }
    
    if (-not $vsPath) {
        throw "Visual Studio with C++ tools not found. Use -InstallIfMissing to install."
    }
    
    # Step 2: Test build tools
    $toolStatus = Test-BuildTools
    
    $requiredTools = @($toolStatus.Ml64, $toolStatus.Link, $toolStatus.Cl)
    $missingRequired = $requiredTools | Where-Object { -not $_ }
    
    if ($missingRequired) {
        Write-Log "Some required tools are missing" "WARN"
        
        # Try to set up environment
        Set-EnvironmentVariables -VSPath $vsPath
        
        # Re-test
        $toolStatus = Test-BuildTools
        $requiredTools = @($toolStatus.Ml64, $toolStatus.Link, $toolStatus.Cl)
        $missingRequired = $requiredTools | Where-Object { -not $_ }
        
        if ($missingRequired) {
            throw "Required build tools still missing after environment setup"
        }
    }
    
    # Step 3: Create environment script
    $envScript = New-EnvironmentScript -VSPath $vsPath
    
    # Step 4: Summary
    Write-Log ""
    Write-Log "=== Environment Setup Complete ===" "SUCCESS"
    Write-Log "Visual Studio: $vsPath"
    Write-Log "ml64.exe: $($toolStatus.Paths.Ml64)"
    Write-Log "link.exe: $($toolStatus.Paths.Link)"
    Write-Log "cl.exe: $($toolStatus.Paths.Cl)"
    if ($toolStatus.CMake) {
        Write-Log "cmake.exe: $($toolStatus.Paths.CMake)"
    }
    Write-Log "Windows SDK: $($toolStatus.Paths.WindowsSDK)"
    Write-Log ""
    Write-Log "Quick start:"
    Write-Log "  1. Run: $envScript"
    Write-Log "  2. Or use: .\Turnkey-Deploy.ps1"
    Write-Log ""
    Write-Log "Environment is ready for RawrXD build!" "SUCCESS"
    
} catch {
    Write-Log "ERROR: $_" "ERROR"
    Write-Log "Stack Trace: $($_.ScriptStackTrace)" "ERROR"
    $exitCode = 1
}

exit $exitCode
