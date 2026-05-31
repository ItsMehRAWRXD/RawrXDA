# AGENT_DONE Completion Contract System
# 
# Enforces machine-verifiable completion signaling for agent execution lifecycle.
# Provides both text and JSON formats for human + machine consumption.

param(
    [string] $LogFile,
    [string] $OutputDir = "D:\agent_contracts",
    [switch] $Strict,
    [switch] $JsonOnly
)

#region Configuration
$RequiredFields = @("status", "phase", "tasks_completed", "tasks_total", "commit")
$OptionalFields = @("artifacts", "next", "duration_ms", "branches", "artifacts_verified")

# Status values that indicate success
$SuccessStatuses = @("success", "completed", "done")

#endregion

#region Utilities
function Get-TimestampMark {
    return (Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff")
}

function Write-ContractLog {
    param([string] $Message)
    Write-Host "[$((Get-TimestampMark))] [CONTRACT] $Message" -ForegroundColor Magenta
}

function Parse-AgentDoneBlock {
    param([string] $Content)
    
    $result = @{}
    $normalizedContent = $Content -replace '\\r\\n', "`r`n" -replace '\\n', "`n"
    
    # Strategy: Try JSON first (more robust), then text, then markdown
    # The agent emits BOTH text [AGENT_DONE] block AND JSON block
    # JSON is the source of truth; text is human-readable marker
    
    # Try JSON format first (source of truth)
    $jsonMatches = [regex]::Matches($normalizedContent, '\{[\s\S]*?"agent_done"[\s\S]*?\}')
    if ($jsonMatches.Count -gt 0) {
        # Try the LAST JSON block (most recent / final)
        $lastJson = $jsonMatches[$jsonMatches.Count - 1].Value
        try {
            $jsonData = $lastJson | ConvertFrom-Json -ErrorAction Stop
            if ($jsonData.agent_done -eq $true) {
                Write-ContractLog "Found JSON AGENT_DONE block (source of truth)"
                $result = @{}
                foreach ($prop in $jsonData.PSObject.Properties) {
                    $result[$prop.Name] = $prop.Value
                }
                $result["format"] = "json"
                return $result
            }
        }
        catch {
            Write-ContractLog "JSON parse failed: $($_.Exception.Message)"
        }
    }
    
    # Fallback: Try text format [AGENT_DONE] key=value pairs
    if ($normalizedContent -match "\[AGENT_DONE\]([\s\S]*?)(?:\r?\n\r?\n|\[|$)") {
        $block = $Matches[1].Trim()
        Write-ContractLog "Found text AGENT_DONE block (fallback)"
        
        foreach ($line in $block -split "\r?\n") {
            if ($line -match "^([^=]+)=(.*)$") {
                $key = $Matches[1].Trim()
                $value = $Matches[2].Trim()
                $result[$key] = $value
            }
        }
        
        $result["format"] = "text"
        return $result
    }
    
    # Try Markdown completion protocol format
    if ($Content -match "## 🎯 Session Complete[\s\S]*?Ready to continue with") {
        Write-ContractLog "Found Markdown completion protocol block"
        $result["format"] = "markdown"
        $result["status"] = "success"
        
        # Extract key information from markdown
        if ($Content -match "### ✅ What's Done[\s\S]*?### 📊") {
            $tasksSection = $Matches[0]
            $taskCount = ([regex]::Matches($tasksSection, "- \*\*\[")).Count
            $result["tasks_completed"] = $taskCount
            $result["tasks_total"] = $taskCount
        }
        
        if ($Content -match "### 📍 Relevant Links[\s\S]*?\- \[.*?\]:") {
            $linksSection = $Matches[0]
            $artifactCount = ([regex]::Matches($linksSection, "- \*\*\[")).Count
            $result["artifacts"] = $artifactCount
        }
        
        return $result
    }
    
    return $null
}

function Validate-CompletionContract {
    param([hashtable] $Contract)
    
    $errors = @()
    $warnings = @()
    
    # Check required fields
    foreach ($field in $RequiredFields) {
        if (-not $Contract.ContainsKey($field)) {
            $errors += "Missing required field: $field"
        }
        elseif ([string]::IsNullOrEmpty($Contract[$field])) {
            $errors += "Empty required field: $field"
        }
    }
    
    # Validate status
    if ($Contract.ContainsKey("status")) {
        $status = $Contract["status"].ToLower()
        if ($status -notin @("success", "completed", "done", "failed", "error", "partial")) {
            $warnings += "Unknown status value: $status"
        }
    }
    
    # Validate tasks completion
    if ($Contract.ContainsKey("tasks_completed") -and $Contract.ContainsKey("tasks_total")) {
        try {
            $completed = [int]$Contract["tasks_completed"]
            $total = [int]$Contract["tasks_total"]
            
            if ($completed -gt $total) {
                $errors += "tasks_completed ($completed) > tasks_total ($total)"
            }
            if ($completed -lt 0 -or $total -lt 0) {
                $errors += "Negative task counts not allowed"
            }
        }
        catch {
            $errors += "Invalid task count format: $($_.Exception.Message)"
        }
    }
    
    # Validate commit hash format (if present)
    if ($Contract.ContainsKey("commit") -and $Contract["commit"].Length -ne 40) {
        $warnings += "Commit hash length unexpected: $($Contract['commit']) (expected 40 chars)"
    }
    
    return @{
        IsValid = ($errors.Count -eq 0)
        Errors = $errors
        Warnings = $warnings
    }
}

function Emit-AgentDone {
    param(
        [string] $Status,
        [string] $Phase,
        [int] $TasksCompleted,
        [int] $TasksTotal,
        [string] $Commit,
        [string[]] $Artifacts = @(),
        [string] $NextPhase,
        [int] $DurationMs,
        [int] $Branches,
        [bool] $ArtifactsVerified,
        [string] $OutputFormat = "both"
    )
    
    $timestamp = Get-TimestampMark
    
    # Text format
    $textLines = @(
        "",
        "[AGENT_DONE]",
        "status=$Status",
        "phase=$Phase",
        "tasks_completed=$TasksCompleted",
        "tasks_total=$TasksTotal",
        "commit=$Commit"
    )
    
    if ($Artifacts.Count -gt 0) {
        $textLines += "artifacts=$($Artifacts.Count)"
    }
    if ($NextPhase) {
        $textLines += "next=$NextPhase"
    }
    if ($DurationMs -gt 0) {
        $textLines += "duration_ms=$DurationMs"
    }
    if ($Branches -gt 0) {
        $textLines += "branches=$Branches"
    }
    if ($ArtifactsVerified) {
        $textLines += "artifacts_verified=true"
    }
    
    $textLines += "timestamp=$timestamp"
    $textBlock = ($textLines -join "`n") + "`n"
    
    # JSON format
    $jsonData = @{
        agent_done = $true
        status = $Status
        phase = $Phase
        tasks_completed = $TasksCompleted
        tasks_total = $TasksTotal
        commit = $Commit
        timestamp = $timestamp
    }
    
    if ($Artifacts.Count -gt 0) {
        $jsonData.artifacts = $Artifacts
    }
    if ($NextPhase) {
        $jsonData.next = $NextPhase
    }
    if ($DurationMs -gt 0) {
        $jsonData.duration_ms = $DurationMs
    }
    if ($Branches -gt 0) {
        $jsonData.branches = $Branches
    }
    if ($ArtifactsVerified) {
        $jsonData.artifacts_verified = $true
    }
    
    $jsonBlock = $jsonData | ConvertTo-Json -Depth 3
    
    # Output based on format preference
    switch ($OutputFormat.ToLower()) {
        "text" { return $textBlock }
        "json" { return $jsonBlock }
        "both" { return "$textBlock$jsonBlock" }
        default { return $textBlock }
    }
}

#endregion

#region Main Execution
if (-not $LogFile) {
    Write-ContractLog "No log file specified. Creating sample completion contract..."
    
    $sample = Emit-AgentDone -Status "success" -Phase "2C" -TasksCompleted 7 -TasksTotal 7 `
        -Commit "7f9ca5ddb" -Artifacts @("run_kernel_ab_sweep.ps1", "inference_latency_breakdown.h") `
        -NextPhase "Phase D kernel sweep" -DurationMs 18234 -Branches 2 -ArtifactsVerified $true
    
    Write-Host $sample
    exit 0
}

if (-not (Test-Path $LogFile)) {
    Write-ContractLog "ERROR: Log file not found: $LogFile"
    exit 1
}

Write-ContractLog "Validating completion contract in: $LogFile"

$content = Get-Content $LogFile -Raw
$contract = Parse-AgentDoneBlock $content

if (-not $contract) {
    Write-ContractLog "FAIL: No AGENT_DONE block found in log"
    if ($Strict) {
        exit 7
    }
    exit 0
}

$validation = Validate-CompletionContract $contract

if (-not $validation.IsValid) {
    Write-ContractLog "FAIL: Contract validation errors:"
    foreach ($err in $validation.Errors) {
        Write-ContractLog "  ERROR: $err"
    }
    exit 8
}

if ($validation.Warnings.Count -gt 0) {
    Write-ContractLog "WARNINGS:"
    foreach ($warn in $validation.Warnings) {
        Write-ContractLog "  WARN: $warn"
    }
}

# Check status
$status = $contract["status"].ToLower()
if ($status -in $SuccessStatuses) {
    Write-ContractLog "SUCCESS: Agent completed successfully"
    Write-ContractLog "Phase: $($contract['phase'])"
    Write-ContractLog "Tasks: $($contract['tasks_completed'])/$($contract['tasks_total'])"
    
    if ($contract.ContainsKey("artifacts")) {
        Write-ContractLog "Artifacts: $($contract['artifacts'])"
    }
    
    exit 0
}
else {
    Write-ContractLog "FAIL: Agent reported status: $status"
    if ($contract.ContainsKey("error")) {
        Write-ContractLog "Error: $($contract['error'])"
    }
    exit 9
}

#endregion