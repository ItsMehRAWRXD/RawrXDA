# ============================================================================
# Phase 1 Agent Polish: Autonomous Workflow Execution Smoke Test
# 
# This test validates that HeadlessIDE's --autonomous flag:
# 1. Accepts workflow configuration flags (--workflow, --state, --workflow-output)
# 2. Dispatches to runAutonomousWorkflowMode()
# 3. Executes workflow milestones without crashing
# 4. Reports structured output (JSON or verbose)
# 5. Returns appropriate exit codes
# ============================================================================

param(
    [string]$ExePath = "d:\rawrxd\build\bin\RawrXD-Win32IDE.exe",
    [string]$WorkflowName = "default-compile-fix",
    [string]$OutputDir = "d:\rawrxd\test\workflow_outputs\$(Get-Date -Format 'yyyyMMdd_HHmmss')",
    [string]$StateFile = "",
    [bool]$VerboseMode = $true,
    [bool]$JsonMode = $false
)

# ============================================================================
# Test Configuration
# ============================================================================
$testName = "Phase1_AutonomousWorkflow"
$testStart = Get-Date
$testLog = "d:\rawrxd\test\logs\${testName}_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

# Ensure directories exist
$null = New-Item -ItemType Directory -Path (Split-Path $testLog) -Force -ErrorAction SilentlyContinue
$null = New-Item -ItemType Directory -Path $OutputDir -Force -ErrorAction SilentlyContinue

function Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "HH:mm:ss.fff"
    $line = "[$timestamp] [$Level] $Message"
    Write-Host $line
    Add-Content -Path $testLog -Value $line
}

function LogSection {
    param([string]$Section)
    Log ""
    Log "=".PadRight(80, "=")
    Log "  $Section"
    Log "=".PadRight(80, "=")
}

# ============================================================================
# Smoke Test Steps
# ============================================================================

LogSection "Smoke Test: Phase 1 Autonomous Workflow Execution"
Log "Test Start: $testStart"
Log "Workflow: $WorkflowName"
Log "Output Directory: $OutputDir"
Log "Verbose Mode: $VerboseMode"
Log "JSON Mode: $JsonMode"

# ── Step 1: Validate executable exists ────────────────────────────────────
LogSection "Step 1: Validate Executable"
if (!(Test-Path -Path $ExePath)) {
    Log "FAILED: Executable not found at $ExePath" "ERROR"
    exit 1
}
Log "OK: Executable found at $ExePath" "PASS"
$exeInfo = Get-Item -Path $ExePath | Select-Object -Property Length, LastWriteTime
Log "Size: $($exeInfo.Length) bytes, Modified: $($exeInfo.LastWriteTime)"

# ── Step 2: Construct command line arguments ──────────────────────────────
LogSection "Step 2: Construct CLI Arguments"
$args = @(
    "--headless",
    "--autonomous",
    "--workflow", $WorkflowName,
    "--workflow-output", $OutputDir
)

if ($StateFile -and (Test-Path -Path $StateFile)) {
    $args += "--state", $StateFile
    Log "State file will be used for resumption: $StateFile" "DEBUG"
}

if ($VerboseMode) {
    $args += "--workflow-verbose"
}

if ($JsonMode) {
    $args += "--json"
}

Log "Final arguments: $($args -join ' ')" "DEBUG"

# ── Step 3: Execute workflow in autonomous mode ───────────────────────────
LogSection "Step 3: Execute Autonomous Workflow"
Log "Invoking: $ExePath $($args -join ' ')"

$process = New-Object System.Diagnostics.ProcessStartInfo
$process.FileName = $ExePath
$process.UseShellExecute = $false
$process.RedirectStandardOutput = $true
$process.RedirectStandardError = $true
$process.CreateNoWindow = $true

foreach ($arg in $args) {
    $process.ArgumentList.Add($arg) | Out-Null
}

$proc = [System.Diagnostics.Process]::Start($process)
$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$exitCode = $proc.ExitCode
$proc.WaitForExit()

Log "Process exited with code: $exitCode"

# ── Step 4: Parse and validate output ─────────────────────────────────────
LogSection "Step 4: Validate Output"

if ($stdout) {
    Log "STDOUT Output:"
    $stdout | ForEach-Object { Log "  $_" "DEBUG" }
}

if ($stderr) {
    Log "STDERR Output:"
    $stderr | ForEach-Object { Log "  $_" "WARNING" }
}

# ── Check for expected milestone markers ──
$expectedMarkers = @(
    "Phase 1 Agent Polish",
    "Workflow Execution Mode",
    "Workflow execution starting",
    "Workflow execution complete"
)

$firstMissingMarker = $null
foreach ($marker in $expectedMarkers) {
    if ($stdout -notmatch [regex]::Escape($marker)) {
        if ($null -eq $firstMissingMarker) {
            $firstMissingMarker = $marker
        }
        Log "MISSING MARKER: '$marker'" "WARNING"
    } else {
        Log "FOUND MARKER: '$marker'" "PASS"
    }
}

# ── Step 5: Validate exit code ────────────────────────────────────────────
LogSection "Step 5: Validate Exit Code"
if ($exitCode -eq 0) {
    Log "EXIT CODE: $exitCode (Success)" "PASS"
    $exitCodePass = $true
} else {
    Log "EXIT CODE: $exitCode (Failure)" "ERROR"
    $exitCodePass = $false
}

# ── Step 6: Validate output directory structure ──────────────────────────
LogSection "Step 6: Validate Output Directory"
if (Test-Path -Path $OutputDir) {
    $files = Get-ChildItem -Path $OutputDir -Recurse -File
    if ($files) {
        Log "Output directory contains $($files.Count) file(s)" "INFO"
        $files | ForEach-Object { Log "  - $($_. Name) ($($_.Length) bytes)" "DEBUG" }
    } else {
        Log "Output directory exists but is empty" "WARNING"
    }
} else {
    Log "Output directory does not exist (may not have been created)" "WARNING"
}

# ── Step 7: Generate summary report ───────────────────────────────────────
LogSection "Test Summary"
$testEnd = Get-Date
$testDuration = ($testEnd - $testStart).TotalSeconds

$overallPass = $exitCodePass -and ($null -eq $firstMissingMarker)

$summary = @{
    TestName = $testName
    StartTime = $testStart
    EndTime = $testEnd
    DurationSeconds = $testDuration
    ExitCode = $exitCode
    Success = $overallPass
    OutputLogFile = $testLog
    OutputDirectory = $OutputDir
    MarkersFound = ($expectedMarkers.Count - $(if ($firstMissingMarker) { 1 } else { 0 }))
    MarkersTotal = $expectedMarkers.Count
}

Log "Test Duration: $testDuration seconds"
Log "Markers Found: $($summary.MarkersFound) / $($summary.MarkersTotal)"
Log "Overall Result: $(if ($overallPass) { 'PASS' } else { 'FAIL' })" $(if ($overallPass) { "PASS" } else { "ERROR" })
Log "Log File: $testLog"

# Write JSON report
$reportPath = Join-Path -Path $OutputDir -ChildPath "smoke_test_report.json"
$summary | ConvertTo-Json | Out-File -Path $reportPath -Encoding UTF8
Log "Report written to: $reportPath" "DEBUG"

# ============================================================================
# Exit with appropriate code
# ============================================================================
exit $(if ($overallPass) { 0 } else { 1 })
