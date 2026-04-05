param(
    [string]$LogPath = "D:\dist\v1.2.5-fused\golden_phi3_strict_native.log",
    [string]$StatusPath = "D:\dist\v1.2.5-fused\golden_watch_status.json",
    [string]$SealStatusPath = "D:\dist\v1.2.5-fused\golden_auto_seal_status.json",
    [int]$PollSeconds = 2,
    [int]$GateBatchUs = 310,
    [int]$MaxIterations = 0,
    [string]$ReconcileScriptPath = "D:\scripts\reconcile_v125_evidence_state.ps1",
    [string]$StageScriptPath = "D:\scripts\stage_v125_release.ps1",
    [string]$VerifyIntegrityScriptPath = "D:\dist\v1.2.5-fused\verify_integrity.ps1",
    [switch]$AutoSeal,
    [switch]$AllowParityWaivers,
    [switch]$StopOnSealable
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedScript {
    param(
        [string]$ScriptPath,
        [string[]]$Arguments = @()
    )

    if (-not (Test-Path -LiteralPath $ScriptPath)) {
        throw "Required script not found: $ScriptPath"
    }

    & powershell -NoProfile -ExecutionPolicy Bypass -File $ScriptPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Script failed with exit code ${LASTEXITCODE}: $ScriptPath"
    }
}

function Write-SealStatus {
    param(
        [string]$State,
        [string]$Message,
        [object]$Detail = $null
    )

    $payload = [ordered]@{
        observed_utc = (Get-Date).ToUniversalTime().ToString('o')
        state = $State
        message = $Message
        detail = $Detail
    }

    $payload | ConvertTo-Json -Depth 6 | Set-Content -Path $SealStatusPath -Encoding ascii
}

function Invoke-AutoSealSequence {
    Write-SealStatus -State 'RUNNING' -Message 'Auto-seal sequence started.'

    Invoke-CheckedScript -ScriptPath $ReconcileScriptPath

    $stageArgs = @()
    if ($AllowParityWaivers) {
        $stageArgs += '-AllowParityWaivers'
    }
    Invoke-CheckedScript -ScriptPath $StageScriptPath -Arguments $stageArgs
    Invoke-CheckedScript -ScriptPath $VerifyIntegrityScriptPath

    Write-SealStatus -State 'COMPLETED' -Message 'Auto-seal sequence completed successfully.'
}

function Get-WatchState {
    param(
        [string]$Content,
        [int]$GateBatchUs
    )

    $runEnd = $Content -match '\[headless\] run_end'
    $hasMock = $Content -match '\[MOCK\]|rxdn_mock_tok_'
    $batchUs = $null
    if ($Content -match 'batch_us\s*[=:]\s*(\d+)') {
        $batchUs = [int]$Matches[1]
    }

    $state = 'PENDING'
    if ($hasMock) {
        $state = 'REJECTED_MOCK_PATH'
    }
    elseif ($runEnd -and $null -ne $batchUs -and $batchUs -le $GateBatchUs) {
        $state = 'SEALABLE'
    }
    elseif ($runEnd -and $null -eq $batchUs) {
        $state = 'RUN_END_NO_BATCH_US'
    }
    elseif ($runEnd -and $batchUs -gt $GateBatchUs) {
        $state = 'RUN_END_GATE_FAIL'
    }
    elseif ($null -ne $batchUs -and $batchUs -gt $GateBatchUs) {
        $state = 'METRIC_PRESENT_GATE_FAIL'
    }
    elseif ($null -ne $batchUs) {
        $state = 'METRIC_PRESENT_WAITING_RUN_END'
    }

    [pscustomobject]@{
        observed_utc = (Get-Date).ToUniversalTime().ToString('o')
        log_path = $LogPath
        state = $state
        run_end = $runEnd
        has_mock = $hasMock
        batch_us = $batchUs
        gate_batch_us = $GateBatchUs
    }
}

Write-Host "Watching strict-native golden log: $LogPath"
Write-Host "Status file: $StatusPath"
if ($AutoSeal) {
    Write-Host "Auto-seal status file: $SealStatusPath"
    Write-SealStatus -State 'ARMED' -Message 'Auto-seal watcher armed and waiting for a sealable strict-native log.'
}

$lastSignature = $null
$iteration = 0
$autoSealAttempted = $false
while ($true) {
    $iteration += 1
    if (-not (Test-Path $LogPath)) {
        $status = [pscustomobject]@{
            observed_utc = (Get-Date).ToUniversalTime().ToString('o')
            log_path = $LogPath
            state = 'LOG_MISSING'
            run_end = $false
            has_mock = $false
            batch_us = $null
            gate_batch_us = $GateBatchUs
        }
    }
    else {
        $content = Get-Content $LogPath -Raw
        $status = Get-WatchState -Content $content -GateBatchUs $GateBatchUs
    }

    $statusJson = $status | ConvertTo-Json -Depth 3
    Set-Content -Path $StatusPath -Value $statusJson -Encoding ascii

    $signature = "{0}|{1}|{2}|{3}" -f $status.state, $status.run_end, $status.has_mock, $status.batch_us
    if ($signature -ne $lastSignature) {
        Write-Host "[$($status.observed_utc)] state=$($status.state) run_end=$($status.run_end) has_mock=$($status.has_mock) batch_us=$($status.batch_us)"
        $lastSignature = $signature
    }

    if ($StopOnSealable -and $status.state -eq 'SEALABLE') {
        Write-Host "Sealable strict-native log detected."
        break
    }

    if ($AutoSeal -and -not $autoSealAttempted -and $status.state -eq 'SEALABLE') {
        $autoSealAttempted = $true
        try {
            Invoke-AutoSealSequence
            Write-Host "Auto-seal sequence completed successfully."
            break
        }
        catch {
            Write-SealStatus -State 'FAILED' -Message $_.Exception.Message
            Write-Host "Auto-seal sequence failed: $($_.Exception.Message)" -ForegroundColor Red
            break
        }
    }

    if ($MaxIterations -gt 0 -and $iteration -ge $MaxIterations) {
        Write-Host "Watcher validation complete after $iteration iteration(s)."
        break
    }

    Start-Sleep -Seconds $PollSeconds
}
