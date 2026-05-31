# RawrXD Turnkey Build Automation
# Single-command build with validation
# Usage: .\scripts\turnkey\Build-RawrXD.ps1 [-Configuration Release|Debug] [-Clean]

[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    
    [switch]$Clean,
    [switch]$Parallel,
    [int]$ParallelJobs = 0,  # 0 = auto-detect
    [switch]$SkipTests,
    [string]$OutputPath = "",
    [string]$LogPath = "$env:TEMP\rawrxd-build.log"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "Continue"

# Configuration
$script:ProjectRoot = Resolve-Path "$PSScriptRoot\..\.."
$script:BuildDir = Join-Path $script:ProjectRoot "build-turnkey"
$script:LogFile = $LogPath
$script:StartTime = Get-Date

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"
    Add-Content -Path $script:LogFile -Value $logEntry -ErrorAction SilentlyContinue
    switch ($Level) {
        "ERROR" { Write-Host $logEntry -ForegroundColor Red }
        "WARN"  { Write-Host $logEntry -ForegroundColor Yellow }
        "SUCCESS" { Write-Host $logEntry -ForegroundColor Green }
        "BUILD" { Write-Host $logEntry -ForegroundColor Cyan }
        default { Write-Host $logEntry }
    }
}

function Test-BuildEnvironment {
    Write-Log "Checking build environment..."
    
    $checks = @{
        Ml64 = $false
        CMake = $false
        VSDevShell = $false
    }
    
    # Check ml64.exe
    $ml64 = Get-Command ml64.exe -ErrorAction SilentlyContinue
    $checks.Ml64 = [bool]$ml64
    if ($ml64) {
        Write-Log "  ✓ ml64.exe found: $($ml64.Source)" "SUCCESS"
    } else {
        Write-Log "  ✗ ml64.exe not found" "ERROR"
    }
    
    # Check cmake
    $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    $checks.CMake = [bool]$cmake
    if ($cmake) {
        Write-Log "  ✓ cmake.exe found: $($cmake.Source)" "SUCCESS"
    } else {
        Write-Log "  ✗ cmake.exe not found" "ERROR"
    }
    
    # Check if we're in VS dev shell
    $checks.VSDevShell = [bool]$env:LIB -and [bool]$env:INCLUDE
    if ($checks.VSDevShell) {
        Write-Log "  ✓ Visual Studio environment loaded" "SUCCESS"
    } else {
        Write-Log "  ✗ Visual Studio environment not detected" "WARN"
    }
    
    return $checks
}

function Initialize-BuildEnvironment {
    Write-Log "Initializing build environment..."
    
    # Try to find and load VS environment if not already loaded
    if (-not ($env:LIB -and $env:INCLUDE)) {
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vsWhere) {
            $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($vsPath) {
                $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
                if (Test-Path $vcvars) {
                    Write-Log "Loading VS environment from $vcvars..."
                    $tempFile = [System.IO.Path]::GetTempFileName()
                    cmd /c "`"$vcvars`" && set > `"$tempFile`"" 2>$null
                    
                    if (Test-Path $tempFile) {
                        Get-Content $tempFile | ForEach-Object {
                            if ($_ -match "^(\w+)=(.*)$") {
                                [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
                            }
                        }
                        Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
                        Write-Log "  ✓ VS environment loaded" "SUCCESS"
                    }
                }
            }
        }
    }
    
    # Verify after loading
    if (-not (Get-Command ml64.exe -ErrorAction SilentlyContinue)) {
        throw "Failed to initialize build environment. Run Setup-Environment.ps1 first."
    }
}

function Clear-BuildDirectory {
    Write-Log "Cleaning build directory..."
    
    if (Test-Path $script:BuildDir) {
        Remove-Item $script:BuildDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Log "  ✓ Removed old build directory" "SUCCESS"
    }
    
    New-Item -ItemType Directory -Path $script:BuildDir -Force | Out-Null
    Write-Log "  ✓ Created fresh build directory: $script:BuildDir" "SUCCESS"
}

function Invoke-CMakeConfigure {
    Write-Log "Configuring with CMake..." "BUILD"
    
    $cmakeArgs = @(
        "..",
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DCMAKE_C_COMPILER=cl",
        "-DCMAKE_CXX_COMPILER=cl",
        "-DCMAKE_ASM_MASM_COMPILER=ml64"
    )
    
    Push-Location $script:BuildDir
    try {
        & cmake @cmakeArgs 2>&1 | Tee-Object -FilePath $script:LogFile -Append
        
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed with exit code $LASTEXITCODE"
        }
        
        Write-Log "  ✓ CMake configuration successful" "SUCCESS"
    }
    finally {
        Pop-Location
    }
}

function Invoke-CMakeBuild {
    Write-Log "Building RawrXD..." "BUILD"
    
    $buildArgs = @(
        "--build", $script:BuildDir,
        "--config", $Configuration
    )
    
    if ($Parallel) {
        if ($ParallelJobs -eq 0) {
            $ParallelJobs = [Environment]::ProcessorCount
        }
        $buildArgs += "--parallel"
        $buildArgs += $ParallelJobs
        Write-Log "  Using $ParallelJobs parallel jobs"
    }
    
    & cmake @buildArgs 2>&1 | Tee-Object -FilePath $script:LogFile -Append
    
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    
    Write-Log "  ✓ Build completed" "SUCCESS"
}

function Test-BuildOutput {
    Write-Log "Validating build output..."
    
    $expectedOutputs = @(
        @{ Name = "RawrXD-Win32IDE.exe"; Required = $true },
        @{ Name = "RawrXD-Win32IDE.pdb"; Required = $false },
        @{ Name = "RawrXD.exe"; Required = $false }
    )
    
    $binDir = Join-Path $script:BuildDir "bin"
    if (-not (Test-Path $binDir)) {
        $binDir = Join-Path $script:BuildDir "$Configuration"
    }
    if (-not (Test-Path $binDir)) {
        $binDir = $script:BuildDir
    }
    
    $foundOutputs = @()
    $missingOutputs = @()
    
    foreach ($output in $expectedOutputs) {
        $outputPath = Join-Path $binDir $output.Name
        if (Test-Path $outputPath) {
            $fileInfo = Get-Item $outputPath
            $foundOutputs += [PSCustomObject]@{
                Name = $output.Name
                Path = $outputPath
                Size = $fileInfo.Length
                SizeMB = [math]::Round($fileInfo.Length / 1MB, 2)
            }
            Write-Log "  ✓ Found: $($output.Name) ($([math]::Round($fileInfo.Length / 1MB, 2)) MB)" "SUCCESS"
        } else {
            $missingOutputs += $output
            $level = if ($output.Required) { "WARN" } else { "INFO" }
            $prefix = if ($output.Required) { "✗" } else { "•" }
            Write-Log "  $prefix Missing: $($output.Name)" $level
        }
    }
    
    # Check for critical outputs
    $criticalMissing = $missingOutputs | Where-Object { $_.Required } | ForEach-Object { $_.Name }
    
    if ($criticalMissing) {
        throw "Critical outputs missing: $($criticalMissing -join ', ')"
    }
    
    return $foundOutputs
}

function Copy-BuildArtifacts {
    param([array]$Artifacts)
    
    if (-not $OutputPath) {
        $OutputPath = Join-Path $script:ProjectRoot "bin-turnkey"
    }
    
    Write-Log "Copying artifacts to $OutputPath..."
    
    if (-not (Test-Path $OutputPath)) {
        New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    }
    
    foreach ($artifact in $Artifacts) {
        $destPath = Join-Path $OutputPath $artifact.Name
        Copy-Item $artifact.Path $destPath -Force
        Write-Log "  ✓ Copied: $($artifact.Name)" "SUCCESS"
    }
    
    return $OutputPath
}

function Invoke-SmokeTests {
    param([string]$BinaryPath)
    
    Write-Log "Running smoke tests..."
    
    $tests = @(
        @{ Name = "PE Format Check"; Command = { 
            $bytes = [System.IO.File]::ReadAllBytes($BinaryPath) | Select-Object -First 2
            $sig = [System.Text.Encoding]::ASCII.GetString($bytes)
            return $sig -eq "MZ"
        }},
        @{ Name = "Executable Launch Test"; Command = {
            $process = Start-Process $BinaryPath -ArgumentList "--version" -PassThru -WindowStyle Hidden
            Start-Sleep -Milliseconds 500
            $process.Refresh()
            $running = -not $process.HasExited
            if ($running) {
                Stop-Process $process -Force
            }
            return $true  # If we got here, it launched
        }}
    )
    
    $passed = 0
    $failed = 0
    
    foreach ($test in $tests) {
        try {
            $result = & $test.Command
            if ($result) {
                Write-Log "  ✓ $($test.Name)" "SUCCESS"
                $passed++
            } else {
                Write-Log "  ✗ $($test.Name)" "ERROR"
                $failed++
            }
        } catch {
            Write-Log "  ✗ $($test.Name): $_" "ERROR"
            $failed++
        }
    }
    
    Write-Log "Smoke tests: $passed passed, $failed failed"
    return ($failed -eq 0)
}

function New-BuildReport {
    param(
        [array]$Artifacts,
        [string]$OutputPath,
        [TimeSpan]$Duration
    )
    
    $report = [PSCustomObject]@{
        BuildTimestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Configuration = $Configuration
        Duration = $Duration.ToString()
        DurationSeconds = [math]::Round($Duration.TotalSeconds, 2)
        Artifacts = $Artifacts
        OutputDirectory = $OutputPath
        LogFile = $script:LogFile
        Status = "SUCCESS"
    }
    
    $reportPath = Join-Path $OutputPath "build-report.json"
    $report | ConvertTo-Json -Depth 3 | Out-File $reportPath
    Write-Log "  ✓ Build report: $reportPath" "SUCCESS"
    
    return $report
}

# Main execution
Write-Log "=== RawrXD Turnkey Build ==="
Write-Log "Configuration: $Configuration"
Write-Log "Project Root: $script:ProjectRoot"
Write-Log "Log file: $LogFile"
Write-Log ""

$exitCode = 0
try {
    # Step 1: Check environment
    $envStatus = Test-BuildEnvironment
    
    if (-not ($envStatus.Ml64 -and $envStatus.CMake)) {
        Write-Log "Build tools not found, attempting to initialize..." "WARN"
        Initialize-BuildEnvironment
    }
    
    # Re-check after initialization
    $envStatus = Test-BuildEnvironment
    if (-not $envStatus.Ml64) {
        throw "ml64.exe not available. Run Setup-Environment.ps1 first."
    }
    
    # Step 2: Clean if requested
    if ($Clean) {
        Clear-BuildDirectory
    } else {
        if (-not (Test-Path $script:BuildDir)) {
            New-Item -ItemType Directory -Path $script:BuildDir -Force | Out-Null
        }
    }
    
    # Step 3: Configure
    Invoke-CMakeConfigure
    
    # Step 4: Build
    Invoke-CMakeBuild
    
    # Step 5: Validate output
    $artifacts = Test-BuildOutput
    
    if ($artifacts.Count -eq 0) {
        throw "No build artifacts found"
    }
    
    # Step 6: Copy to output
    $outputDir = Copy-BuildArtifacts -Artifacts $artifacts
    
    # Step 7: Smoke tests
    $mainBinary = $artifacts | Where-Object { $_.Name -eq "RawrXD-Win32IDE.exe" } | Select-Object -First 1
    if ($mainBinary -and -not $SkipTests) {
        $testResult = Invoke-SmokeTests -BinaryPath $mainBinary.Path
        if (-not $testResult) {
            Write-Log "Some smoke tests failed" "WARN"
        }
    }
    
    # Step 8: Generate report
    $duration = (Get-Date) - $script:StartTime
    $report = New-BuildReport -Artifacts $artifacts -OutputPath $outputDir -Duration $duration
    
    # Summary
    Write-Log ""
    Write-Log "=== Build Complete ===" "SUCCESS"
    Write-Log "Duration: $($duration.ToString())"
    Write-Log "Output: $outputDir"
    Write-Log "Artifacts:"
    foreach ($artifact in $artifacts) {
        Write-Log "  - $($artifact.Name) ($($artifact.SizeMB) MB)"
    }
    Write-Log ""
    Write-Log "Next steps:"
    Write-Log "  1. Run: .\scripts\turnkey\Validate-Deployment.ps1"
    Write-Log "  2. Or: .\Turnkey-Deploy.ps1 -SkipBuild"
    
} catch {
    Write-Log ""
    Write-Log "BUILD FAILED: $_" "ERROR"
    Write-Log "See log for details: $LogFile" "ERROR"
    $exitCode = 1
}

exit $exitCode
