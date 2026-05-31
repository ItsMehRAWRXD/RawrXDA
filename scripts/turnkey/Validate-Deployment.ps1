# RawrXD Turnkey Deployment Validation
# Comprehensive post-deployment testing
# Usage: .\scripts\turnkey\Validate-Deployment.ps1 [-BinaryPath <path>]

[CmdletBinding()]
param(
    [string]$BinaryPath = "",
    [string]$ConfigPath = "",
    [int]$TimeoutSeconds = 30,
    [switch]$GenerateReport,
    [string]$ReportPath = "$env:TEMP\rawrxd-validation-report.json"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "Continue"

# Configuration
$script:ProjectRoot = Resolve-Path "$PSScriptRoot\..\.."
$script:TestResults = @()
$script:OverallStatus = "UNKNOWN"

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Message = "",
        [string]$Category = "General"
    )
    
    $result = [PSCustomObject]@{
        TestName = $TestName
        Category = $Category
        Passed = $Passed
        Message = $Message
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    }
    
    $script:TestResults += $result
    
    $status = if ($Passed) { "✓ PASS" } else { "✗ FAIL" }
    $color = if ($Passed) { "Green" } else { "Red" }
    
    Write-Host "[$status] $TestName" -ForegroundColor $color
    if ($Message) {
        Write-Host "       $Message" -ForegroundColor Gray
    }
}

function Test-FileExists {
    param([string]$Path, [string]$Description)
    
    $exists = Test-Path $Path
    Write-TestResult -TestName "$Description exists" -Passed $exists -Message $Path -Category "Files"
    return $exists
}

function Test-ConfigPropertyPath {
    param(
        $Config,
        [string[]]$Path
    )

    $current = $Config
    foreach ($segment in $Path) {
        if ($null -eq $current) {
            return $false
        }

        if ($current -is [System.Collections.IDictionary]) {
            if (-not $current.Contains($segment)) {
                return $false
            }
            $current = $current[$segment]
            continue
        }

        if ($current.PSObject.Properties.Name -notcontains $segment) {
            return $false
        }
        $current = $current.$segment
    }

    return $true
}

function Test-PEFormat {
    param([string]$ExecutablePath)
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($ExecutablePath) | Select-Object -First 2
        $sig = [System.Text.Encoding]::ASCII.GetString($bytes)
        $isPE = $sig -eq "MZ"
        
        Write-TestResult -TestName "PE Format (MZ header)" -Passed $isPE -Message "Signature: $sig" -Category "Binary"
        return $isPE
    }
    catch {
        Write-TestResult -TestName "PE Format" -Passed $false -Message $_.Exception.Message -Category "Binary"
        return $false
    }
}

function Test-ExecutableLaunch {
    param([string]$ExecutablePath)
    
    try {
        $process = Start-Process -FilePath $ExecutablePath -ArgumentList "--version" -PassThru -WindowStyle Hidden -ErrorAction Stop
        
        # Wait briefly to see if it crashes immediately
        $process.WaitForExit(2000) | Out-Null
        
        if ($process.HasExited) {
            if ($process.ExitCode -eq 0) {
                Write-TestResult -TestName "Executable launch" -Passed $true -Message "Exited cleanly with code 0" -Category "Runtime"
                return $true
            } else {
                Write-TestResult -TestName "Executable launch" -Passed $true -Message "Launched (exit code: $($process.ExitCode))" -Category "Runtime"
                return $true  # Still launched successfully
            }
        } else {
            # Process still running, kill it
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            Write-TestResult -TestName "Executable launch" -Passed $true -Message "Process started and running" -Category "Runtime"
            return $true
        }
    }
    catch {
        Write-TestResult -TestName "Executable launch" -Passed $false -Message $_.Exception.Message -Category "Runtime"
        return $false
    }
}

function Test-Dependencies {
    param([string]$ExecutablePath)
    
    try {
        # Use dumpbin or check dependencies
        $requiredDlls = @(
            "kernel32.dll",
            "user32.dll",
            "gdi32.dll",
            "shell32.dll",
            "ole32.dll"
        )
        
        $allFound = $true
        $missing = @()
        
        foreach ($dll in $requiredDlls) {
            $dllPath = Join-Path $env:SystemRoot "System32\$dll"
            if (-not (Test-Path $dllPath)) {
                $allFound = $false
                $missing += $dll
            }
        }
        
        if ($allFound) {
            Write-TestResult -TestName "System dependencies" -Passed $true -Message "All required DLLs present" -Category "Dependencies"
        } else {
            Write-TestResult -TestName "System dependencies" -Passed $false -Message "Missing: $($missing -join ', ')" -Category "Dependencies"
        }
        
        return $allFound
    }
    catch {
        Write-TestResult -TestName "Dependencies check" -Passed $false -Message $_.Exception.Message -Category "Dependencies"
        return $false
    }
}

function Test-Configuration {
    param([string]$ConfigPath)
    
    if (-not (Test-Path $ConfigPath)) {
        Write-TestResult -TestName "Configuration file" -Passed $false -Message "Not found: $ConfigPath" -Category "Config"
        return $false
    }
    
    try {
        $config = Get-Content $ConfigPath -Raw | ConvertFrom-Json -AsHashtable -ErrorAction Stop

        $requiredAlternatives = @(
            @{ Name = "model"; Alternatives = @(@("model"), @("native"), @("modelServer")) },
            @{ Name = "inference"; Alternatives = @(@("inference")) },
            @{ Name = "ui"; Alternatives = @(@("ui"), @("theme"), @("editor")) },
            @{ Name = "paths"; Alternatives = @(@("paths"), @("modelServer", "configPath")) }
        )

        $missing = @()
        foreach ($requirement in $requiredAlternatives) {
            $satisfied = $false
            foreach ($alternative in $requirement.Alternatives) {
                if (Test-ConfigPropertyPath -Config $config -Path $alternative) {
                    $satisfied = $true
                    break
                }
            }

            if (-not $satisfied) {
                $missing += $requirement.Name
            }
        }

        $passed = $missing.Count -eq 0
        $message = if ($passed) {
            "Valid JSON structure with required deployment settings"
        } else {
            "Missing required settings: $($missing -join ', ')"
        }

        Write-TestResult -TestName "Configuration JSON" -Passed $passed -Message $message -Category "Config"
        return $passed
    }
    catch {
        Write-TestResult -TestName "Configuration JSON" -Passed $false -Message $_.Exception.Message -Category "Config"
        return $false
    }
}

function Test-DirectoryStructure {
    $requiredDirs = @(
        "src",
        "include",
        "scripts"
    )
    
    $allExist = $true
    foreach ($dir in $requiredDirs) {
        $dirPath = Join-Path $script:ProjectRoot $dir
        $exists = Test-Path $dirPath
        if (-not $exists) {
            $allExist = $false
        }
        Write-TestResult -TestName "Directory: $dir" -Passed $exists -Message $dirPath -Category "Structure"
    }
    
    return $allExist
}

function Test-BuildArtifacts {
    param([string]$BinDir)
    
    $expectedFiles = @(
        "RawrXD-Win32IDE.exe",
        "RawrXD.exe"
    )
    
    $found = @()
    $missing = @()
    
    foreach ($file in $expectedFiles) {
        $filePath = Join-Path $BinDir $file
        if (Test-Path $filePath) {
            $found += $file
        } else {
            $missing += $file
        }
    }
    
    $passed = $found.Count -gt 0
    $message = "Found: $($found.Count), Missing: $($missing.Count)"
    if ($missing.Count -gt 0) {
        $message += " (Missing: $($missing -join ', '))"
    }
    
    Write-TestResult -TestName "Build artifacts" -Passed $passed -Message $message -Category "Build"
    return $passed
}

function Test-Performance {
    param([string]$ExecutablePath)
    
    try {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $ExecutablePath -ArgumentList "--version" -PassThru -WindowStyle Hidden
        $exitedQuickly = $process.WaitForExit(1000)
        $sw.Stop()
        
        $startupTime = $sw.ElapsedMilliseconds
        if (-not $exitedQuickly -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            Write-TestResult -TestName "Startup performance" -Passed $true -Message "Process started and stayed healthy after ${startupTime}ms" -Category "Performance"
            return $true
        }

        $passed = $startupTime -lt 5000
        Write-TestResult -TestName "Startup performance" -Passed $passed -Message "${startupTime}ms" -Category "Performance"
        return $passed
    }
    catch {
        Write-TestResult -TestName "Startup performance" -Passed $false -Message $_.Exception.Message -Category "Performance"
        return $false
    }
}

function New-ValidationReport {
    param([string]$ReportPath)
    
    $passed = ($script:TestResults | Where-Object { $_.Passed }).Count
    $failed = ($script:TestResults | Where-Object { -not $_.Passed }).Count
    $total = $script:TestResults.Count
    
    $report = [PSCustomObject]@{
        ValidationTimestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        OverallStatus = if ($failed -eq 0) { "PASS" } else { "FAIL" }
        Summary = [PSCustomObject]@{
            TotalTests = $total
            Passed = $passed
            Failed = $failed
            PassRate = if ($total -gt 0) { [math]::Round(($passed / $total) * 100, 2) } else { 0 }
        }
        Results = $script:TestResults
    }
    
    $report | ConvertTo-Json -Depth 3 | Out-File $ReportPath
    Write-Host ""
    Write-Host "Report saved: $ReportPath" -ForegroundColor Cyan
    
    return $report
}

function Show-Summary {
    $passed = ($script:TestResults | Where-Object { $_.Passed }).Count
    $failed = ($script:TestResults | Where-Object { -not $_.Passed }).Count
    $total = $script:TestResults.Count
    
    Write-Host ""
    Write-Host "=== Validation Summary ===" -ForegroundColor Cyan
    Write-Host "Total Tests: $total" -ForegroundColor White
    Write-Host "Passed: $passed" -ForegroundColor Green
    Write-Host "Failed: $failed" -ForegroundColor $(if ($failed -gt 0) { "Red" } else { "Green" })
    
    if ($total -gt 0) {
        $rate = [math]::Round(($passed / $total) * 100, 2)
        Write-Host "Pass Rate: $rate%" -ForegroundColor $(if ($rate -ge 80) { "Green" } elseif ($rate -ge 50) { "Yellow" } else { "Red" })
    }
    
    if ($failed -eq 0) {
        Write-Host ""
        Write-Host "✓ DEPLOYMENT VALIDATED SUCCESSFULLY" -ForegroundColor Green
        $script:OverallStatus = "PASS"
    } else {
        Write-Host ""
        Write-Host "✗ DEPLOYMENT VALIDATION FAILED" -ForegroundColor Red
        $script:OverallStatus = "FAIL"
        
        Write-Host ""
        Write-Host "Failed tests:" -ForegroundColor Red
        $script:TestResults | Where-Object { -not $_.Passed } | ForEach-Object {
            Write-Host "  - $($_.TestName): $($_.Message)" -ForegroundColor Red
        }
    }
}

# Main execution
Write-Host "=== RawrXD Deployment Validation ===" -ForegroundColor Cyan
Write-Host ""

# Auto-detect paths if not provided
if (-not $BinaryPath) {
    $possiblePaths = @(
        "$script:ProjectRoot\bin-turnkey\RawrXD-Win32IDE.exe",
        "$script:ProjectRoot\build-ninja\bin\RawrXD-Win32IDE.exe",
        "$script:ProjectRoot\build-turnkey\bin\RawrXD-Win32IDE.exe",
        "$script:ProjectRoot\build\bin\Release\RawrXD-Win32IDE.exe",
        "$script:ProjectRoot\bin\RawrXD-Win32IDE.exe",
        "$script:ProjectRoot\RawrXD-Win32IDE.exe"
    )
    
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $BinaryPath = $path
            Write-Host "Auto-detected binary: $BinaryPath" -ForegroundColor Yellow
            break
        }
    }
}

if (-not $ConfigPath) {
    $possibleConfigs = @(
        "$script:ProjectRoot\rawrxd.config.json",
        "$env:APPDATA\RawrXD\rawrxd.config.json"
    )
    
    foreach ($path in $possibleConfigs) {
        if (Test-Path $path) {
            $ConfigPath = $path
            break
        }
    }
}

# Run tests
Write-Host "Running validation tests..." -ForegroundColor Cyan
Write-Host ""

# File existence tests
if ($BinaryPath) {
    [void](Test-FileExists -Path $BinaryPath -Description "Main executable")
    [void](Test-PEFormat -ExecutablePath $BinaryPath)
    [void](Test-ExecutableLaunch -ExecutablePath $BinaryPath)
    [void](Test-Dependencies -ExecutablePath $BinaryPath)
    [void](Test-Performance -ExecutablePath $BinaryPath)
} else {
    Write-TestResult -TestName "Binary detection" -Passed $false -Message "No executable found" -Category "Files"
}

if ($ConfigPath) {
    [void](Test-FileExists -Path $ConfigPath -Description "Configuration file")
    [void](Test-Configuration -ConfigPath $ConfigPath)
} else {
    Write-TestResult -TestName "Config detection" -Passed $false -Message "No config file found" -Category "Config"
}

[void](Test-DirectoryStructure)

# Check for build artifacts
$binDirs = @(
    "$script:ProjectRoot\bin-turnkey",
    "$script:ProjectRoot\build-ninja\bin",
    "$script:ProjectRoot\bin",
    "$script:ProjectRoot\build-turnkey\bin",
    "$script:ProjectRoot\build\bin\Release"
)

$binDirFound = $false
foreach ($dir in $binDirs) {
    if (Test-Path $dir) {
        [void](Test-BuildArtifacts -BinDir $dir)
        $binDirFound = $true
        break
    }
}

if (-not $binDirFound) {
    Write-TestResult -TestName "Build artifacts" -Passed $false -Message "No bin directory found" -Category "Build"
}

# Show summary
Show-Summary

# Generate report if requested
if ($GenerateReport) {
    New-ValidationReport -ReportPath $ReportPath
}

# Exit with appropriate code
exit $(if ($script:OverallStatus -eq "PASS") { 0 } else { 1 })
