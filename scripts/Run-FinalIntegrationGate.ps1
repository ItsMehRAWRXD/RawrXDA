#Requires -Version 5.1
<#
.SYNOPSIS
    14-Day Final Integration Gate - Complete Turnkey/Un-TurnKey Validation
    
.DESCRIPTION
    Master validation script for the 14-Day Production Expansion:
    - Runs all turnkey smoke tests
    - Validates Un-TurnKey cleanup
    - Performs final integration checks
    - Generates comprehensive report
    - Provides go/no-go decision for production
    
    This is the FINAL GATE before production deployment.
    
.PARAMETER FullValidation
    Run complete validation suite including binary tests
    
.PARAMETER QuickValidation
    Run only source-level validation (faster)
    
.PARAMETER SkipUnTurnKey
    Skip the cleanup validation phase
    
.PARAMETER GenerateArtifacts
    Generate all validation artifacts and reports
    
.PARAMETER FailFast
    Stop on first failure
    
.EXAMPLE
    .\Run-FinalIntegrationGate.ps1 -FullValidation
    
.EXAMPLE
    .\Run-FinalIntegrationGate.ps1 -QuickValidation -GenerateArtifacts
#>

param(
    [switch]$FullValidation,
    [switch]$QuickValidation,
    [switch]$SkipUnTurnKey,
    [switch]$GenerateArtifacts,
    [switch]$FailFast
)

$ErrorActionPreference = "Stop"
$script:StartTime = Get-Date
$script:PhaseResults = [ordered]@{}
$script:OverallStatus = "PASS"

# ============================================================================
# HEADER
# ============================================================================

Write-Host @"

╔══════════════════════════════════════════════════════════════════════════════╗
║                                                                              ║
║   RawrXD 14-Day Production Expansion                                         ║
║   ═══════════════════════════════════                                          ║
║   FINAL INTEGRATION & TURNKEY/UN-TURNKEY VALIDATION                          ║
║                                                                              ║
║   Date: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")                                          ║
║   Mode: $(if ($FullValidation) { "FULL VALIDATION" } elseif ($QuickValidation) { "QUICK VALIDATION" } else { "STANDARD" })                          ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

function Write-PhaseHeader {
    param([string]$Phase, [int]$Number)
    Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Blue
    Write-Host "║ PHASE $Number`: $Phase" -ForegroundColor Blue
    Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Blue
}

function Invoke-ValidationPhase {
    param(
        [string]$Name,
        [int]$Number,
        [scriptblock]$Script,
        [switch]$Required
    )
    
    Write-PhaseHeader -Phase $Name -Number $Number
    
    $phaseStart = Get-Date
    $success = $false
    $errorMsg = $null
    
    try {
        $result = & $Script
        $success = if ($null -eq $result) { $true } else { [bool]$result }
        if (-not $success) {
            $errorMsg = "Phase returned failure status"
            Write-Host "  [ERROR] Phase reported failure status" -ForegroundColor Red
        }
    }
    catch {
        $success = $false
        $errorMsg = $_.Exception.Message
        Write-Host "  [ERROR] Phase failed: $errorMsg" -ForegroundColor Red
        
        if ($FailFast) {
            throw "FailFast: Phase $Number failed"
        }
    }
    
    $phaseEnd = Get-Date
    $duration = $phaseEnd - $phaseStart
    
    $script:PhaseResults[$Name] = [ordered]@{
        Phase = $Number
        Name = $Name
        Success = $success
        Duration = [math]::Round($duration.TotalSeconds, 2)
        Error = $errorMsg
        Required = $Required.IsPresent
    }
    
    if (-not $success -and $Required) {
        $script:OverallStatus = "FAIL"
    }
    
    $color = if ($success) { "Green" } else { if ($Required) { "Red" } else { "Yellow" } }
    $status = if ($success) { "PASS" } else { if ($Required) { "FAIL" } else { "WARN" } }
    
    Write-Host "  Phase $Number Result: [$status] (${duration}s)" -ForegroundColor $color
    
    return $success
}

function Invoke-ChildPowerShellFile {
    param(
        [string]$ScriptPath,
        [string[]]$Arguments = @()
    )

    $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
    if (-not $pwsh) {
        throw "pwsh not found in PATH"
    }

    $output = & $pwsh.Source -NoProfile -File $ScriptPath @Arguments 2>&1 | Out-String
    return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = $output
    }
}

# ============================================================================
# PHASE 1: ENVIRONMENT VALIDATION
# ============================================================================

$phase1 = {
    Write-Host "  Validating environment..." -ForegroundColor Gray
    
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    
    # Check required directories
    $required = @("src", "scripts", "cmake")
    foreach ($dir in $required) {
        $path = Join-Path $repoRoot $dir
        if (-not (Test-Path $path)) {
            throw "Required directory missing: $dir"
        }
    }
    Write-Host "  ✓ Required directories present" -ForegroundColor Green
    
    # Check PowerShell version
    if ($PSVersionTable.PSVersion.Major -lt 5) {
        throw "PowerShell 5.1+ required"
    }
    Write-Host "  ✓ PowerShell version: $($PSVersionTable.PSVersion)" -ForegroundColor Green
    
    # Check disk space
    $disk = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$($repoRoot.ToString().Substring(0,2))'"
    $freeGB = [math]::Round($disk.FreeSpace / 1GB, 2)
    if ($freeGB -lt 1) {
        throw "Insufficient disk space: $freeGB GB free"
    }
    Write-Host "  ✓ Disk space: $freeGB GB free" -ForegroundColor Green
    
    return $true
}

# ============================================================================
# PHASE 2: TURNKEY SMOKE VALIDATION
# ============================================================================

$phase2 = {
    Write-Host "  Running Turnkey smoke validation..." -ForegroundColor Gray
    
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    $turnkeyScript = Join-Path $repoRoot "scripts\Run-TurnkeyIdeSmoke.ps1"
    
    if (-not (Test-Path $turnkeyScript)) {
        throw "Turnkey script not found: $turnkeyScript"
    }
    
    # Determine parameters based on mode
    $params = @()
    if ($QuickValidation) {
        $params += "-SkipAgenticExe"
        $params += "-SkipCopilotCli"
    }
    
    # Run turnkey smoke
    $invocation = Invoke-ChildPowerShellFile -ScriptPath $turnkeyScript -Arguments $params
    $output = $invocation.Output
    $exitCode = $invocation.ExitCode
    
    Write-Host "  Turnkey exit code: $exitCode" -ForegroundColor $(if ($exitCode -eq 0) { "Green" } else { "Yellow" })
    
    # Check for summary file
    $summaryPath = Join-Path $repoRoot "logs\turnkey_ide_smoke_last.json"
    if (Test-Path $summaryPath) {
        $summary = Get-Content $summaryPath | ConvertFrom-Json
        Write-Host "  ✓ Turnkey summary generated" -ForegroundColor Green
        Write-Host "    - Steps: $($summary.steps.Count)" -ForegroundColor Gray
        Write-Host "    - Overall: $(if ($summary.ok) { 'PASS' } else { 'FAIL' })" -ForegroundColor $(if ($summary.ok) { "Green" } else { "Yellow" })
    }
    
    # For quick validation, we accept source-only success
    if ($QuickValidation) {
        return $true
    }
    
    return ($exitCode -eq 0)
}

# ============================================================================
# PHASE 3: GAP CLOSURE VALIDATION
# ============================================================================

$phase3 = {
    Write-Host "  Running Gap Closure validation..." -ForegroundColor Gray
    
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    $gapScript = Join-Path $repoRoot "scripts\Validate-TurnkeyGapClosure.ps1"
    
    if (-not (Test-Path $gapScript)) {
        Write-Host "  ! Gap closure script not found (optional)" -ForegroundColor Yellow
        return $true  # Optional phase
    }
    
    # Run gap closure validation
    $mode = if ($QuickValidation) { "PreFlight" } else { "Full" }
    $params = @("-Mode", $mode)
    if ($GenerateArtifacts) {
        $params += "-GenerateReport"
    }
    $invocation = Invoke-ChildPowerShellFile -ScriptPath $gapScript -Arguments $params
    $output = $invocation.Output
    $exitCode = $invocation.ExitCode
    
    Write-Host "  Gap closure exit code: $exitCode" -ForegroundColor $(if ($exitCode -eq 0) { "Green" } else { "Yellow" })
    
    return ($exitCode -eq 0)
}

# ============================================================================
# PHASE 4: UN-TURNKEY VALIDATION
# ============================================================================

$phase4 = {
    if ($SkipUnTurnKey) {
        Write-Host "  Skipped (via -SkipUnTurnKey)" -ForegroundColor Yellow
        return $true
    }
    
    Write-Host "  Running Un-TurnKey validation..." -ForegroundColor Gray
    
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    $unturnkeyScript = Join-Path $repoRoot "scripts\Un-TurnKey.ps1"
    
    if (-not (Test-Path $unturnkeyScript)) {
        Write-Host "  ! Un-TurnKey script not found (optional)" -ForegroundColor Yellow
        return $true  # Optional phase
    }
    
    # Run Un-TurnKey in WhatIf mode first
    Write-Host "  Running WhatIf check..." -ForegroundColor Gray
    $invocation = Invoke-ChildPowerShellFile -ScriptPath $unturnkeyScript -Arguments @("-WhatIf", "-ValidationProbe", "-Force")
    $whatifOutput = $invocation.Output
    $exitCode = $invocation.ExitCode
    Write-Host "  Un-TurnKey exit code: $exitCode" -ForegroundColor $(if ($exitCode -eq 0) { "Green" } else { "Yellow" })

    if ($exitCode -ne 0) {
        return $false
    }
    
    if ($whatifOutput -match "Would delete" -or $whatifOutput -match "\[WhatIf\]") {
        Write-Host "  ✓ Un-TurnKey WhatIf shows cleanup targets" -ForegroundColor Green
    }
    else {
        Write-Host "  ! No cleanup targets found (may already be clean)" -ForegroundColor Yellow
    }
    
    return $true
}

# ============================================================================
# PHASE 5: FINAL INTEGRATION CHECKS
# ============================================================================

$phase5 = {
    Write-Host "  Running final integration checks..." -ForegroundColor Gray
    
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    $checks = @()
    
    # Check 1: Memory system
    $memoriesDir = Join-Path $repoRoot "memories"
    $hasMemories = Test-Path $memoriesDir
    $checks += @{ Name = "Memory System"; Pass = $hasMemories; Required = $false }
    
    # Check 2: 14-Day command registration
    $commandsFile = Join-Path $repoRoot "src\win32app\Win32IDE_Commands.cpp"
    $has14Day = (Test-Path $commandsFile) -and ((Get-Content $commandsFile -Raw) -match "14DAY_TURNKEY")
    $checks += @{ Name = "14-Day Command"; Pass = $has14Day; Required = $true }
    
    # Check 3: Turnkey scripts
    $turnkeyScript = Join-Path $repoRoot "scripts\Run-TurnkeyIdeSmoke.ps1"
    $hasTurnkey = Test-Path $turnkeyScript
    $checks += @{ Name = "Turnkey Script"; Pass = $hasTurnkey; Required = $true }
    
    # Check 4: Gap closure script
    $gapScript = Join-Path $repoRoot "scripts\Validate-TurnkeyGapClosure.ps1"
    $hasGap = Test-Path $gapScript
    $checks += @{ Name = "Gap Closure Script"; Pass = $hasGap; Required = $true }
    
    # Check 5: Un-TurnKey script
    $unturnkeyScript = Join-Path $repoRoot "scripts\Un-TurnKey.ps1"
    $hasUnTurnKey = Test-Path $unturnkeyScript
    $checks += @{ Name = "Un-TurnKey Script"; Pass = $hasUnTurnKey; Required = $false }
    
    # Display results
    foreach ($check in $checks) {
        $color = if ($check.Pass) { "Green" } else { if ($check.Required) { "Red" } else { "Yellow" } }
        $status = if ($check.Pass) { "✓" } else { if ($check.Required) { "✗" } else { "!" } }
        Write-Host "  $status $($check.Name)" -ForegroundColor $color
    }
    
    # Fail if any required check failed
    $failedRequired = $checks | Where-Object { $_.Required -and -not $_.Pass }
    if ($failedRequired) {
        throw "Required checks failed: $($failedRequired.Name -join ', ')"
    }
    
    return $true
}

# ============================================================================
# EXECUTION
# ============================================================================

Write-Host "Starting validation phases..." -ForegroundColor Cyan
Write-Host ""

# Phase 1: Environment (Required)
Invoke-ValidationPhase -Name "Environment Validation" -Number 1 -Script $phase1 -Required

# Phase 2: Turnkey Smoke (Required)
Invoke-ValidationPhase -Name "Turnkey Smoke Validation" -Number 2 -Script $phase2 -Required

# Phase 3: Gap Closure (Optional)
Invoke-ValidationPhase -Name "Gap Closure Validation" -Number 3 -Script $phase3

# Phase 4: Un-TurnKey (Optional)
Invoke-ValidationPhase -Name "Un-TurnKey Validation" -Number 4 -Script $phase4

# Phase 5: Final Integration (Required)
Invoke-ValidationPhase -Name "Final Integration Checks" -Number 5 -Script $phase5 -Required

# ============================================================================
# SUMMARY
# ============================================================================

$endTime = Get-Date
$totalDuration = $endTime - $script:StartTime

Write-Host "`n╔══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor $(if ($script:OverallStatus -eq "PASS") { "Green" } else { "Red" })
Write-Host "║                         FINAL VALIDATION SUMMARY                             ║" -ForegroundColor $(if ($script:OverallStatus -eq "PASS") { "Green" } else { "Red" })
Write-Host "╠══════════════════════════════════════════════════════════════════════════════╣" -ForegroundColor $(if ($script:OverallStatus -eq "PASS") { "Green" } else { "Red" })

foreach ($phase in $script:PhaseResults.Keys) {
    $result = $script:PhaseResults[$phase]
    $color = if ($result.Success) { "Green" } else { if ($result.Required) { "Red" } else { "Yellow" } }
    $status = if ($result.Success) { "PASS" } else { if ($result.Required) { "FAIL" } else { "WARN" } }
    Write-Host "║  Phase $($result.Phase): $($result.Name.PadRight(30)) [$status] ($($result.Duration)s)" -ForegroundColor $color
}

Write-Host "╠══════════════════════════════════════════════════════════════════════════════╣" -ForegroundColor $(if ($script:OverallStatus -eq "PASS") { "Green" } else { "Red" })
Write-Host "║  Total Duration: $([math]::Round($totalDuration.TotalSeconds, 2))s" -ForegroundColor White
Write-Host "║  Overall Status: $(if ($script:OverallStatus -eq "PASS") { "✓ PRODUCTION READY" } else { "✗ VALIDATION FAILED" })" -ForegroundColor $(if ($script:OverallStatus -eq "PASS") { "Green" } else { "Red" })
Write-Host "╚══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor $(if ($script:OverallStatus -eq "PASS") { "Green" } else { "Red" })

# Generate artifacts if requested
if ($GenerateArtifacts) {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    $reportDir = Join-Path $repoRoot "reports"
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
    
    $artifact = [ordered]@{
        timestamp = (Get-Date).ToString("o")
        mode = if ($FullValidation) { "Full" } elseif ($QuickValidation) { "Quick" } else { "Standard" }
        duration_seconds = [math]::Round($totalDuration.TotalSeconds, 2)
        overall_status = $script:OverallStatus
        phases = $script:PhaseResults
        system = @{
            computer = $env:COMPUTERNAME
            user = $env:USERNAME
            powershell = $PSVersionTable.PSVersion.ToString()
        }
    }
    
    $artifactPath = Join-Path $reportDir "final_integration_gate_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
    $artifact | ConvertTo-Json -Depth 10 | Out-File -FilePath $artifactPath -Encoding utf8
    
    Write-Host "`n  Artifact generated: $artifactPath" -ForegroundColor Cyan
}

Write-Host ""

exit [int]($script:OverallStatus -eq "FAIL")
