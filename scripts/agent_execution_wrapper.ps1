# agent_execution_wrapper.ps1
# 
# Wraps agent execution with completion contract enforcement.
# Ensures every agent run ends with machine-verifiable completion signaling.

param(
    [string] $AgentScript,
    [string[]] $Arguments = @(),
    [string] $LogDir = "D:\agent_logs",
    [switch] $Strict,
    [switch] $JsonOutput,
    [ValidateSet("default", "implementation", "refactor", "contracts")]
    [string] $PromptProfile = "default"
)

# Import completion validator
. D:\RawrXD\scripts\agent_completion_validator.ps1
. D:\RawrXD\scripts\Get-AgentExecutionKernel.ps1

function Write-ExecutionLog {
    param([string] $Message, [string] $Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    Write-Host "[$timestamp] [$Level] $Message" -ForegroundColor $(switch ($Level) {
        "INFO" { "White" }
        "WARN" { "Yellow" }
        "ERROR" { "Red" }
        "SUCCESS" { "Green" }
        default { "Gray" }
    })
}

# Validate parameters
if (-not $AgentScript) {
    Write-ExecutionLog "ERROR: No agent script specified" "ERROR"
    exit 1
}

if (-not (Test-Path $AgentScript)) {
    Write-ExecutionLog "ERROR: Agent script not found: $AgentScript" "ERROR"
    exit 2
}

# Create log directory
if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = Join-Path $LogDir "agent_$(Split-Path $AgentScript -Leaf)_$timestamp.log"
$errorLogFile = Join-Path $LogDir "agent_error_$timestamp.log"

Write-ExecutionLog "Starting agent execution wrapper" "INFO"
Write-ExecutionLog "Agent: $AgentScript" "INFO"
Write-ExecutionLog "Arguments: $($Arguments -join ' ')" "INFO"
Write-ExecutionLog "Log file: $logFile" "INFO"
Write-ExecutionLog "Strict mode: $Strict" "INFO"

$promptPack = Get-AgentExecutionKernelPack -ProfileName $PromptProfile
$env:RAWRXD_AGENT_EXECUTION_KERNEL = $promptPack.kernel
$env:RAWRXD_AGENT_EXECUTION_OVERLAY = $promptPack.overlay
$env:RAWRXD_AGENT_EXECUTION_PROFILE = $promptPack.profile
if ($PromptProfile -ne "default") {
    Write-ExecutionLog "Prompt profile: $PromptProfile" "INFO"
}

# Start agent execution
$startTime = Get-Date
$allArgs = @("-NoProfile", "-File", $AgentScript) + $Arguments
$process = Start-Process -FilePath "pwsh" `
    -ArgumentList $allArgs `
    -RedirectStandardOutput $logFile `
    -RedirectStandardError $errorLogFile `
    -PassThru `
    -NoNewWindow

Write-ExecutionLog "Agent process started (PID: $($process.Id))" "INFO"

# Wait for completion with timeout
$timeout = New-TimeSpan -Minutes 30
$process | Wait-Process -Timeout $timeout.TotalSeconds -ErrorAction SilentlyContinue

if (-not $process.HasExited) {
    Write-ExecutionLog "ERROR: Agent execution timed out after 30 minutes" "ERROR"
    $process | Stop-Process -Force
    exit 3
}

if (Test-Path $errorLogFile) {
    $stderrContent = Get-Content $errorLogFile -Raw
    if (-not [string]::IsNullOrWhiteSpace($stderrContent)) {
        Add-Content -Path $logFile -Value "`n[STDERR]`n$stderrContent"
    }
}

$endTime = Get-Date
$duration = $endTime - $startTime

Write-ExecutionLog "Agent process exited with code: $($process.ExitCode)" "INFO"
Write-ExecutionLog "Execution duration: $($duration.TotalSeconds.ToString('0.0'))s" "INFO"

# Validate completion contract
Write-ExecutionLog "Validating completion contract..." "INFO"

if (-not (Test-Path $logFile)) {
    Write-ExecutionLog "ERROR: Log file not created: $logFile" "ERROR"
    exit 4
}

$logContent = Get-Content $logFile -Raw

# Check for AGENT_DONE block
$contract = Parse-AgentDoneBlock $logContent

if (-not $contract) {
    Write-ExecutionLog "FAIL: No AGENT_DONE completion block found" "ERROR"
    
    if ($Strict) {
        Write-ExecutionLog "STRICT MODE: Failing due to missing completion contract" "ERROR"
        exit 7
    }
    
    Write-ExecutionLog "WARN: Continuing without completion contract (non-strict mode)" "WARN"
    exit $process.ExitCode
}

# Validate contract
$validation = Validate-CompletionContract $contract

if (-not $validation.IsValid) {
    Write-ExecutionLog "FAIL: Completion contract validation errors:" "ERROR"
    foreach ($err in $validation.Errors) {
        Write-ExecutionLog "  ERROR: $err" "ERROR"
    }
    
    if ($Strict) {
        exit 8
    }
    
    Write-ExecutionLog "WARN: Continuing despite contract validation issues (non-strict mode)" "WARN"
    exit $process.ExitCode
}

if ($validation.Warnings.Count -gt 0) {
    Write-ExecutionLog "WARN: Completion contract warnings:" "WARN"
    foreach ($warning in $validation.Warnings) {
        Write-ExecutionLog "  WARN: $warning" "WARN"
    }
}

# Check status
$status = $contract["status"].ToLower()

if ($status -in @("success", "completed", "done")) {
    Write-ExecutionLog "SUCCESS: Agent completed with status: $status" "SUCCESS"
    Write-ExecutionLog "Phase: $($contract['phase'])" "INFO"
    Write-ExecutionLog "Tasks: $($contract['tasks_completed'])/$($contract['tasks_total'])" "INFO"
    
    if ($contract.ContainsKey("artifacts")) {
        Write-ExecutionLog "Artifacts: $($contract['artifacts'])" "INFO"
    }
    
    if ($contract.ContainsKey("next")) {
        Write-ExecutionLog "Next: $($contract['next'])" "INFO"
    }
    
    exit 0
}
elseif ($status -eq "partial") {
    Write-ExecutionLog "PARTIAL: Agent completed with partial success: $status" "WARN"
    Write-ExecutionLog "Tasks: $($contract['tasks_completed'])/$($contract['tasks_total'])" "WARN"
    
    if ($contract.ContainsKey("error")) {
        Write-ExecutionLog "Error: $($contract['error'])" "WARN"
    }
    
    exit 1
}
else {
    Write-ExecutionLog "FAIL: Agent reported failure status: $status" "ERROR"
    
    if ($contract.ContainsKey("error")) {
        Write-ExecutionLog "Error: $($contract['error'])" "ERROR"
    }
    
    exit 9
}

# JSON output option
if ($JsonOutput) {
    $result = @{
        agent_script = $AgentScript
        exit_code = $process.ExitCode
        duration_seconds = [math]::Round($duration.TotalSeconds, 2)
        completion_contract = $contract
        validation = $validation
        timestamp = (Get-Date -Format "o")
    }
    
    $result | ConvertTo-Json -Depth 3 | Out-File (Join-Path $LogDir "execution_result_$timestamp.json") -Encoding UTF8
}