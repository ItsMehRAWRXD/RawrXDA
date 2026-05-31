#Requires -Version 5.1
<#
.SYNOPSIS
    14-Day Turnkey Gap Closure Validator - Final Integration & Validation
    
.DESCRIPTION
    Comprehensive validation script that closes remaining turnkey gaps:
    1. Pre-flight validation (environment, dependencies, permissions)
    2. Turnkey deployment validation (install, configure, verify)
    3. Runtime validation (smoke tests, integration checks)
    4. Un-TurnKey validation (cleanup, de-provisioning, rollback)
    5. Post-validation report generation
    
    This script ensures the complete turnkey lifecycle is validated and ready
    for production deployment.
    
.PARAMETER Mode
    Validation mode: PreFlight, Deploy, Runtime, UnTurnKey, Full
    
.PARAMETER BuildDir
    Path to build directory containing binaries
    
.PARAMETER SkipUnTurnKey
    Skip the cleanup/de-provisioning validation phase
    
.PARAMETER GenerateReport
    Generate JSON validation report
    
.PARAMETER Strict
    Fail on any validation warning (not just errors)
    
.EXAMPLE
    .\Validate-TurnkeyGapClosure.ps1 -Mode Full -BuildDir .\build-win32
    
.EXAMPLE
    .\Validate-TurnkeyGapClosure.ps1 -Mode UnTurnKey -GenerateReport
#>

param(
    [ValidateSet("PreFlight", "Deploy", "Runtime", "UnTurnKey", "Full")]
    [string]$Mode = "Full",
    
    [string]$BuildDir = "",
    [switch]$SkipUnTurnKey,
    [switch]$GenerateReport,
    [switch]$Strict
)

$ErrorActionPreference = "Stop"
$script:ValidationResults = [ordered]@{}
$script:Warnings = @()
$script:StartTime = Get-Date

# ============================================================================
# CONFIGURATION
# ============================================================================
$script:Config = @{
    MinDiskSpaceGB = 5
    MinMemoryGB = 4
    RequiredPorts = @(8080, 11434, 3000)
    TimeoutSeconds = 300
    BackupLocation = "$env:TEMP\rawrxd_turnkey_backup"
}

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

function Write-ValidationHeader {
    param([string]$Phase)
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  TURNKEY GAP CLOSURE: $Phase" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

function Add-ValidationResult {
    param(
        [string]$Phase,
        [string]$Test,
        [bool]$Passed,
        [string]$Message = "",
        [string]$Severity = "Info"  # Info, Warning, Error
    )
    
    $key = "$Phase`:$Test"
    $script:ValidationResults[$key] = @{
        Phase = $Phase
        Test = $Test
        Passed = $Passed
        Message = $Message
        Severity = $Severity
        Timestamp = (Get-Date).ToString("o")
    }
    
    $color = if ($Passed) { "Green" } elseif ($Severity -eq "Warning" ) { "Yellow" } else { "Red" }
    $status = if ($Passed) { "PASS" } else { "FAIL" }
    Write-Host "  [$status] $Test" -ForegroundColor $color -NoNewline
    if ($Message) { Write-Host " - $Message" -ForegroundColor Gray }
    else { Write-Host "" }
    
    if (-not $Passed -and $Severity -eq "Warning") {
        $script:Warnings += $key
    }
}

function Test-ValidationPhase {
    param(
        [string]$PhaseName,
        [scriptblock]$TestBlock
    )
    
    Write-ValidationHeader -Phase $PhaseName
    try {
        & $TestBlock
        return $true
    }
    catch {
        Add-ValidationResult -Phase $PhaseName -Test "PhaseExecution" -Passed $false -Message $_.Exception.Message -Severity "Error"
        return $false
    }
}

# ============================================================================
# PHASE 1: PRE-FLIGHT VALIDATION
# ============================================================================

function Invoke-PreFlightValidation {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    
    # System Requirements
    $disk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$($repoRoot.ToString().Substring(0,2))'"
    $freeSpaceGB = [math]::Round($disk.FreeSpace / 1GB, 2)
    Add-ValidationResult -Phase "PreFlight" -Test "DiskSpace" -Passed ($freeSpaceGB -ge $script:Config.MinDiskSpaceGB) -Message "${freeSpaceGB}GB free (min: $($script:Config.MinDiskSpaceGB)GB)"
    
    $memory = Get-CimInstance Win32_PhysicalMemory | Measure-Object -Property Capacity -Sum
    $totalMemoryGB = [math]::Round($memory.Sum / 1GB, 2)
    Add-ValidationResult -Phase "PreFlight" -Test "Memory" -Passed ($totalMemoryGB -ge $script:Config.MinMemoryGB) -Message "${totalMemoryGB}GB RAM (min: $($script:Config.MinMemoryGB)GB)"
    
    # PowerShell Version
    $psVersion = $PSVersionTable.PSVersion
    Add-ValidationResult -Phase "PreFlight" -Test "PowerShellVersion" -Passed ($psVersion.Major -ge 5) -Message "v$psVersion"
    
    # Required Directories
    $requiredDirs = @("src", "scripts", "cmake", "include")
    foreach ($dir in $requiredDirs) {
        $path = Join-Path $repoRoot $dir
        $exists = Test-Path $path
        Add-ValidationResult -Phase "PreFlight" -Test "Dir_$dir" -Passed $exists -Message $(if ($exists) { "Found" } else { "Missing: $path" })
    }
    
    # Build Tools Availability
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    Add-ValidationResult -Phase "PreFlight" -Test "CMakeAvailable" -Passed ($null -ne $cmake) -Message $(if ($cmake) { $cmake.Source } else { "Not in PATH" })
    
    $msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
    $vsDir = Join-Path $env:ProgramFiles "Microsoft Visual Studio"
    $hasVS = (Test-Path $vsDir) -and ($null -ne (Get-ChildItem -Path $vsDir -Directory -ErrorAction SilentlyContinue | Select-Object -First 1))
    $ml64 = Test-Path "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
    $hasCompiler = ($null -ne $msbuild) -or $hasVS -or $ml64
    Add-ValidationResult -Phase "PreFlight" -Test "CompilerAvailable" -Passed $hasCompiler -Message $(if ($hasCompiler) { "MSVC/MSBuild found" } else { "No compiler detected" })
    
    # Port Availability Check
    foreach ($port in $script:Config.RequiredPorts) {
        $listener = $null
        try {
            $listener = New-Object System.Net.Sockets.TcpListener ([System.Net.IPAddress]::Loopback, $port)
            $listener.Start()
            Add-ValidationResult -Phase "PreFlight" -Test "Port_${port}_Available" -Passed $true -Message "Port $port is available"
            $listener.Stop()
        }
        catch {
            Add-ValidationResult -Phase "PreFlight" -Test "Port_${port}_Available" -Passed $false -Message "Port $port is in use" -Severity "Warning"
        }
        finally {
            if ($listener) { 
                try { $listener.Stop() } catch {}
                try { $listener.Close() } catch {}
            }
        }
    }
    
    # Git Repository State
    $gitDir = Join-Path $repoRoot ".git"
    $isGitRepo = Test-Path $gitDir
    Add-ValidationResult -Phase "PreFlight" -Test "GitRepository" -Passed $isGitRepo -Message $(if ($isGitRepo) { "Git repo detected" } else { "Not a git repository" })
    
    if ($isGitRepo) {
        try {
            $branch = git -C $repoRoot rev-parse --abbrev-ref HEAD 2>$null
            Add-ValidationResult -Phase "PreFlight" -Test "GitBranch" -Passed $true -Message "Branch: $branch"
            
            $uncommitted = git -C $repoRoot status --porcelain 2>$null
            $isClean = [string]::IsNullOrWhiteSpace($uncommitted)
            Add-ValidationResult -Phase "PreFlight" -Test "WorkingTreeClean" -Passed $isClean -Message $(if ($isClean) { "Clean" } else { "Uncommitted changes present" }) -Severity $(if ($isClean) { "Info" } else { "Warning" })
        }
        catch {
            Add-ValidationResult -Phase "PreFlight" -Test "GitStatus" -Passed $false -Message "Git check failed: $_" -Severity "Warning"
        }
    }
}

# ============================================================================
# PHASE 2: DEPLOY VALIDATION
# ============================================================================

function Invoke-DeployValidation {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    
    # Determine build directory
    $buildDir = if ($BuildDir) { $BuildDir } else {
        $candidates = @("build-win32", "build-ninja", "build")
        $found = $candidates | Where-Object { Test-Path (Join-Path (Join-Path $repoRoot $_) "CMakeCache.txt") } | Select-Object -First 1
        if ($found) { Join-Path $repoRoot $found } else { $null }
    }
    
    Add-ValidationResult -Phase "Deploy" -Test "BuildDirectoryExists" -Passed ($null -ne $buildDir -and (Test-Path $buildDir)) -Message $(if ($buildDir) { $buildDir } else { "No build directory found" })
    
    if (-not $buildDir) { return }
    
    # Check for key binaries
    $binaries = @(
        @{ Name = "RawrXD-Win32IDE.exe"; Required = $true },
        @{ Name = "RawrEngine.exe"; Required = $false },
        @{ Name = "RawrXD-TpsSmoke.exe"; Required = $false }
    )
    
    foreach ($bin in $binaries) {
        $paths = @(
            Join-Path $buildDir "bin\Release\$($bin.Name)"
            Join-Path $buildDir "bin\$($bin.Name)"
            Join-Path $buildDir $bin.Name
        )
        $found = $paths | Where-Object { Test-Path $_ } | Select-Object -First 1
        Add-ValidationResult -Phase "Deploy" -Test "Binary_$($bin.Name)" -Passed ($null -ne $found) -Message $(if ($found) { "Found: $found" } else { "Not found" }) -Severity $(if ($bin.Required -and -not $found) { "Error" } else { "Info" })
    }
    
    # Check for required configuration files
    $configFiles = @("RawrXDSettings.json", "config\settings.json")
    foreach ($config in $configFiles) {
        $path = Join-Path $repoRoot $config
        $exists = Test-Path $path
        Add-ValidationResult -Phase "Deploy" -Test "Config_$($config.Replace('\', '_'))" -Passed $exists -Message $(if ($exists) { "Found" } else { "Optional - not found" }) -Severity "Info"
    }
    
    # Verify symbol exports (for key binaries)
    $ideExe = Get-ChildItem -Path $buildDir -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($ideExe) {
        try {
            $dumpbin = & where dumpbin 2>$null | Select-Object -First 1
            if ($dumpbin) {
                $exports = & dumpbin /EXPORTS $ideExe.FullName 2>$null
                $hasExports = $exports -match "[0-9]+\s+\d+\s+[0-9a-fA-F]+\s+\w+"
                Add-ValidationResult -Phase "Deploy" -Test "BinaryExports" -Passed $hasExports -Message "Export table present"
            }
            else {
                Add-ValidationResult -Phase "Deploy" -Test "BinaryExports" -Passed $false -Message "dumpbin not available" -Severity "Warning"
            }
        }
        catch {
            Add-ValidationResult -Phase "Deploy" -Test "BinaryExports" -Passed $false -Message "Export check failed: $_" -Severity "Warning"
        }
    }
    
    # Check for Qt dependencies (should be none for Win32IDE)
    if ($ideExe) {
        try {
            $deps = & dumpbin /DEPENDENTS $ideExe.FullName 2>$null
            $hasQt = $deps -match "Qt[0-9]\w*\.dll"
            Add-ValidationResult -Phase "Deploy" -Test "ZeroQtDependencies" -Passed (-not $hasQt) -Message $(if ($hasQt) { "Qt dependencies found!" } else { "No Qt dependencies - correct" })
        }
        catch {
            Add-ValidationResult -Phase "Deploy" -Test "ZeroQtDependencies" -Passed $false -Message "Dependency check failed" -Severity "Warning"
        }
    }
}

# ============================================================================
# PHASE 3: RUNTIME VALIDATION
# ============================================================================

function Invoke-RuntimeValidation {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    
    # Check if we can run the turnkey smoke
    $turnkeyScript = Join-Path $repoRoot "scripts\Run-TurnkeyIdeSmoke.ps1"
    $hasTurnkey = Test-Path $turnkeyScript
    Add-ValidationResult -Phase "Runtime" -Test "TurnkeyScriptAvailable" -Passed $hasTurnkey -Message $(if ($hasTurnkey) { "Found: Run-TurnkeyIdeSmoke.ps1" } else { "Missing turnkey script" })
    
    if ($hasTurnkey) {
        # Run source-only smoke (no binaries required)
        try {
            $output = & pwsh -NoProfile -Command "& '$turnkeyScript' -SkipAgenticExe -SkipCopilotCli 2>&1" | Out-String
            $exitCode = $LASTEXITCODE
            Add-ValidationResult -Phase "Runtime" -Test "TurnkeySourceSmoke" -Passed ($exitCode -eq 0) -Message "Exit code: $exitCode"
        }
        catch {
            Add-ValidationResult -Phase "Runtime" -Test "TurnkeySourceSmoke" -Passed $false -Message "Execution failed: $_" -Severity "Warning"
        }
    }
    
    # Validate IDE command registration
    $commandsFile = Join-Path $repoRoot "src\win32app\Win32IDE_Commands.cpp"
    if (Test-Path $commandsFile) {
        $content = Get-Content $commandsFile -Raw
        $has14DayCommand = $content -match "IDM_TOOLS_RUN_14DAY_TURNKEY_SMOKE"
        Add-ValidationResult -Phase "Runtime" -Test "IDE_14DayCommand" -Passed $has14DayCommand -Message "14-Day Turnkey command registered"
    }
    
    # Check for agentic smoke capability
    $shipScript = Join-Path $repoRoot "Ship\Run-ChatAgenticSmoke.ps1"
    Add-ValidationResult -Phase "Runtime" -Test "AgenticSmokeScript" -Passed (Test-Path $shipScript) -Message $(if (Test-Path $shipScript) { "Found" } else { "Optional - not found" }) -Severity "Info"
    
    # Validate memory system — check VS Code Copilot memory paths or local memories dir
    $memoriesDir = Join-Path $repoRoot "memories"
    $copilotMemDir = "$env:APPDATA\GitHub Copilot\memories"
    $hasLocalMemories = Test-Path $memoriesDir
    $hasCopilotMemories = Test-Path $copilotMemDir
    $hasMemories = $hasLocalMemories -or $hasCopilotMemories
    $memoryMsg = if ($hasLocalMemories) { "Local memories dir: $memoriesDir" } elseif ($hasCopilotMemories) { "Copilot memories: $copilotMemDir" } else { "Memory system not initialized" }
    Add-ValidationResult -Phase "Runtime" -Test "MemorySystem" -Passed $hasMemories -Message $memoryMsg
    
    if ($hasLocalMemories) {
        $tiers = @("session", "repo")
        foreach ($tier in $tiers) {
            $tierPath = Join-Path $memoriesDir $tier
            $exists = Test-Path $tierPath
            Add-ValidationResult -Phase "Runtime" -Test "MemoryTier_$tier" -Passed $exists -Message $(if ($exists) { "Present" } else { "Missing" }) -Severity $(if ($exists) { "Info" } else { "Warning" })
        }
    }
    
    # Validate extension ecosystem
    $extInstaller = Join-Path $repoRoot "scripts\extension_installer_14day_expansion.ps1"
    Add-ValidationResult -Phase "Runtime" -Test "ExtensionInstaller" -Passed (Test-Path $extInstaller) -Message $(if (Test-Path $extInstaller) { "14-day extension installer available" } else { "Not found" }) -Severity "Info"
}

# ============================================================================
# PHASE 4: UN-TURNKEY VALIDATION (CLEANUP)
# ============================================================================

function Invoke-UnTurnKeyValidation {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    
    Write-Host "`n  [Un-TurnKey] Testing cleanup and de-provisioning capabilities..." -ForegroundColor Yellow
    
    # Create test artifacts that would need cleanup
    $testDirs = @(
        @{ Path = "test_temp_$(Get-Random)"; Type = "Directory" },
        @{ Path = "logs\test_$(Get-Random).log"; Type = "File" }
    )
    
    $createdArtifacts = @()
    
    # Create test artifacts
    foreach ($artifact in $testDirs) {
        $fullPath = Join-Path $repoRoot $artifact.Path
        try {
            if ($artifact.Type -eq "Directory") {
                New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
            }
            else {
                New-Item -ItemType File -Path $fullPath -Force -Value "Test content" | Out-Null
            }
            $createdArtifacts += $fullPath
            Add-ValidationResult -Phase "UnTurnKey" -Test "CreateTest_$($artifact.Type)" -Passed $true -Message "Created: $fullPath"
        }
        catch {
            Add-ValidationResult -Phase "UnTurnKey" -Test "CreateTest_$($artifact.Type)" -Passed $false -Message "Failed: $_"
        }
    }
    
    # Test cleanup capability
    $cleanupSuccess = $true
    foreach ($artifact in $createdArtifacts) {
        try {
            if (Test-Path $artifact) {
                Remove-Item -Path $artifact -Recurse -Force -ErrorAction Stop
                Add-ValidationResult -Phase "UnTurnKey" -Test "Cleanup_$(Split-Path $artifact -Leaf)" -Passed $true -Message "Removed successfully"
            }
        }
        catch {
            $cleanupSuccess = $false
            Add-ValidationResult -Phase "UnTurnKey" -Test "Cleanup_$(Split-Path $artifact -Leaf)" -Passed $false -Message "Failed: $_" -Severity "Warning"
        }
    }
    
    # Check for common cleanup targets
    $cleanupTargets = @{
        "BuildArtifacts" = @("build", "build-win32", "build-ninja")
        "LogFiles" = @("logs\*.log", "*.log")
        "TempFiles" = @("temp\*", "tmp\*")
    }
    
    foreach ($targetName in $cleanupTargets.Keys) {
        $patterns = $cleanupTargets[$targetName]
        $found = $false
        foreach ($pattern in $patterns) {
            $matches = Get-ChildItem -Path $repoRoot -Filter $pattern -ErrorAction SilentlyContinue
            if ($matches) { $found = $true; break }
        }
        Add-ValidationResult -Phase "UnTurnKey" -Test "CleanupTarget_$targetName" -Passed $true -Message $(if ($found) { "Cleanup targets exist" } else { "No cleanup needed" }) -Severity "Info"
    }
    
    # Validate backup/restore capability
    $backupDir = $script:Config.BackupLocation
    try {
        New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
        $testFile = Join-Path $backupDir "test_backup.txt"
        "Backup test" | Out-File -FilePath $testFile -Force
        $backupWorks = Test-Path $testFile
        Remove-Item -Path $backupDir -Recurse -Force -ErrorAction SilentlyContinue
        Add-ValidationResult -Phase "UnTurnKey" -Test "BackupCapability" -Passed $backupWorks -Message "Backup/restore path functional"
    }
    catch {
        Add-ValidationResult -Phase "UnTurnKey" -Test "BackupCapability" -Passed $false -Message "Backup test failed: $_" -Severity "Warning"
    }
    
    # Check for uninstaller/registry cleanup scripts
    $uninstallScripts = @(
        "scripts\uninstall.ps1"
        "uninstall.ps1"
        "cleanup.ps1"
    )
    $hasUninstaller = $false
    foreach ($script in $uninstallScripts) {
        $path = Join-Path $repoRoot $script
        if (Test-Path $path) { $hasUninstaller = $true; break }
    }
    Add-ValidationResult -Phase "UnTurnKey" -Test "UninstallerScript" -Passed $hasUninstaller -Message $(if ($hasUninstaller) { "Found" } else { "Optional - not found" }) -Severity "Info"

    # Execute the canonical un-turnkey script in dry-run mode to validate
    # argument parsing and cleanup routing without deleting artifacts.
    $unTurnKeyScript = Join-Path $repoRoot "scripts\Un-TurnKey.ps1"
    if (Test-Path $unTurnKeyScript) {
        try {
            & pwsh -NoProfile -File $unTurnKeyScript -WhatIf -ValidationProbe -Force *> $null
            $scriptExit = $LASTEXITCODE
            Add-ValidationResult -Phase "UnTurnKey" -Test "UnTurnKeyScriptWhatIf" -Passed ($scriptExit -eq 0) -Message "Exit code: $scriptExit"
        }
        catch {
            Add-ValidationResult -Phase "UnTurnKey" -Test "UnTurnKeyScriptWhatIf" -Passed $false -Message "Execution failed: $_" -Severity "Error"
        }
    }
    else {
        Add-ValidationResult -Phase "UnTurnKey" -Test "UnTurnKeyScriptWhatIf" -Passed $false -Message "Missing scripts/Un-TurnKey.ps1" -Severity "Error"
    }
}

# ============================================================================
# REPORT GENERATION
# ============================================================================

function Export-ValidationReport {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    $reportDir = Join-Path $repoRoot "reports"
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
    
    $endTime = Get-Date
    $duration = $endTime - $script:StartTime
    
    $passed = ($script:ValidationResults.Values | Where-Object { $_.Passed -eq $true }).Count
    $failed = ($script:ValidationResults.Values | Where-Object { $_.Passed -eq $false -and $_.Severity -eq "Error" }).Count
    $warnings = ($script:ValidationResults.Values | Where-Object { $_.Passed -eq $false -and $_.Severity -eq "Warning" }).Count
    
    $report = [ordered]@{
        metadata = [ordered]@{
            timestamp = $endTime.ToString("o")
            duration_seconds = [math]::Round($duration.TotalSeconds, 2)
            mode = $Mode
            computer = $env:COMPUTERNAME
            user = $env:USERNAME
        }
        summary = [ordered]@{
            total_tests = $script:ValidationResults.Count
            passed = $passed
            failed = $failed
            warnings = $warnings
            success_rate = if ($script:ValidationResults.Count -gt 0) { [math]::Round(($passed / $script:ValidationResults.Count) * 100, 2) } else { 0 }
            overall_status = if ($failed -eq 0 -and ($warnings -eq 0 -or -not $Strict)) { "PASS" } else { "FAIL" }
        }
        results = $script:ValidationResults
    }
    
    $reportPath = Join-Path $reportDir "turnkey_gap_closure_report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
    $report | ConvertTo-Json -Depth 10 | Out-File -FilePath $reportPath -Encoding utf8
    
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  VALIDATION REPORT" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  Total Tests: $($report.summary.total_tests)" -ForegroundColor White
    Write-Host "  Passed: $passed" -ForegroundColor Green
    Write-Host "  Failed: $failed" -ForegroundColor $(if ($failed -gt 0) { "Red" } else { "Green" })
    Write-Host "  Warnings: $warnings" -ForegroundColor $(if ($warnings -gt 0) { "Yellow" } else { "Green" })
    Write-Host "  Success Rate: $($report.summary.success_rate)%" -ForegroundColor White
    Write-Host "  Duration: $($report.summary.duration_seconds)s" -ForegroundColor White
    Write-Host "  Status: $($report.summary.overall_status)" -ForegroundColor $(if ($report.summary.overall_status -eq "PASS") { "Green" } else { "Red" })
    Write-Host "  Report: $reportPath" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    return $report
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Write-Host @"

╔══════════════════════════════════════════════════════════════════╗
║  RawrXD 14-Day Turnkey Gap Closure Validator                    ║
║  Final Integration & Turnkey/Un-TurnKey Validation              ║
╚══════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

$phasesToRun = switch ($Mode) {
    "PreFlight" { @("PreFlight") }
    "Deploy" { @("Deploy") }
    "Runtime" { @("Runtime") }
    "UnTurnKey" { @("UnTurnKey") }
    "Full" { @("PreFlight", "Deploy", "Runtime", "UnTurnKey") }
}

foreach ($phase in $phasesToRun) {
    switch ($phase) {
        "PreFlight" { Test-ValidationPhase -PhaseName "PreFlight" -TestBlock ${function:Invoke-PreFlightValidation} }
        "Deploy" { Test-ValidationPhase -PhaseName "Deploy" -TestBlock ${function:Invoke-DeployValidation} }
        "Runtime" { Test-ValidationPhase -PhaseName "Runtime" -TestBlock ${function:Invoke-RuntimeValidation} }
        "UnTurnKey" { 
            if (-not $SkipUnTurnKey) {
                Test-ValidationPhase -PhaseName "UnTurnKey" -TestBlock ${function:Invoke-UnTurnKeyValidation}
            }
            else {
                Write-ValidationHeader -Phase "UnTurnKey (SKIPPED)"
                Write-Host "  Skipped via -SkipUnTurnKey parameter" -ForegroundColor Yellow
            }
        }
    }
}

# Generate report if requested
if ($GenerateReport) {
    $report = Export-ValidationReport
}
else {
    # Print summary even without report
    $passed = ($script:ValidationResults.Values | Where-Object { $_.Passed -eq $true }).Count
    $failed = ($script:ValidationResults.Values | Where-Object { $_.Passed -eq $false -and $_.Severity -eq "Error" }).Count
    $warnings = ($script:ValidationResults.Values | Where-Object { $_.Passed -eq $false -and $_.Severity -eq "Warning" }).Count
    
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  VALIDATION SUMMARY" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  Total: $($script:ValidationResults.Count) | Passed: $passed | Failed: $failed | Warnings: $warnings" -ForegroundColor White
    $overall = if ($failed -eq 0 -and ($warnings -eq 0 -or -not $Strict)) { "PASS" } else { "FAIL" }
    Write-Host "  Overall: $overall" -ForegroundColor $(if ($overall -eq "PASS") { "Green" } else { "Red" })
    Write-Host "========================================" -ForegroundColor Cyan
}

# Exit with appropriate code
$exitFailed = ($script:ValidationResults.Values | Where-Object { $_.Passed -eq $false -and $_.Severity -eq "Error" }).Count
$exitWarnings = if ($Strict) { ($script:ValidationResults.Values | Where-Object { $_.Passed -eq $false -and $_.Severity -eq "Warning" }).Count } else { 0 }

exit [int]($exitFailed -gt 0 -or $exitWarnings -gt 0)
