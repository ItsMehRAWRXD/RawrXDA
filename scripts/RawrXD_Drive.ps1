#!/usr/bin/env pwsh
param(
    [ValidateSet('enhanced','voice','digest','validate','api','parallel','')]
    [string]$Action = 'enhanced',
    [switch]$EnableVoice,
    [switch]$EnableExternalAPI,
    [switch]$DigestFirst,
    [string]$Prompt = "",
    [string[]]$Tasks = @(),
    [string]$Provider = "openai",
    [int]$MaxParallel = 3
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$script:Root = if ($env:LAZY_INIT_IDE_ROOT) { $env:LAZY_INIT_IDE_ROOT } else { (Resolve-Path (Join-Path $PSScriptRoot "..") -EA SilentlyContinue).Path }
if (-not $script:Root) { $script:Root = (Get-Location).Path }

function Invoke-ExternalAPI {
    param([string]$prompt, [string]$provider, [string]$apiKey, [string]$model)
    $key = $apiKey; if (-not $key -and $provider -eq 'openai') { $key = $env:OPENAI_API_KEY }
    if (-not $key -and ($provider -eq 'anthropic' -or $provider -eq 'claude')) { $key = $env:ANTHROPIC_API_KEY }
    if (-not $key) { return $null }
    $mod = if ($model) { $model } else { if ($provider -eq 'openai') { "gpt-4o-mini" } else { "claude-3-5-sonnet-20241022" } }
    try {
        if ($provider -eq 'openai') {
            $body = @{ model=$mod; messages=@(@{role="user";content=$prompt}) } | ConvertTo-Json -Depth 5
            $r = Invoke-RestMethod -Uri "https://api.openai.com/v1/chat/completions" -Method Post -Body $body -Headers @{"Authorization"="Bearer $key";"Content-Type"="application/json"}
            return $r.choices[0].message.content
        }
        $body = @{ model=$mod; max_tokens=4096; messages=@(@{role="user";content=$prompt}) } | ConvertTo-Json -Depth 5
        $r = Invoke-RestMethod -Uri "https://api.anthropic.com/v1/messages" -Method Post -Body $body -Headers @{"x-api-key"=$key;"anthropic-version"="2023-06-01";"Content-Type"="application/json"}
        return ($r.content | Where-Object { $_.type -eq "text" } | ForEach-Object { $_.text }) -join ""
    } catch { return $null }
}

function Invoke-MultiAgent {
    param([string[]]$tasks, [int]$maxP)
    $jobs = @()
    $sb = { param($t,$root,$sd)
        $r = @{Task=$t;Output="";Success=$false;Elapsed=0}
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            if ($t -match '^Analyze\s+(.+)$') {
                $p = if (Test-Path $Matches[1].Trim()) { $Matches[1].Trim() } else { Join-Path $root $Matches[1].Trim() }
                $r.Output = "Files: $((Get-ChildItem $p -Recurse -File -EA SilentlyContinue | Measure-Object).Count)"
            } elseif ($t -match '^Run\s+tests?|^tests?$') {
                $tp = Join-Path $root "Run-ProductionTests.ps1"
                $r.Output = if (Test-Path $tp) { (& $tp 2>&1 | Out-String) } else { "Test script not found" }
            } else { $r.Output = "Done: $t" }
            $r.Success = $true
        } catch { $r.Output = $_.Exception.Message }
        $sw.Stop(); $r.Elapsed = $sw.ElapsedMilliseconds; $r
    }
    foreach ($t in $tasks) {
        while ((Get-Job -State Running).Count -ge $maxP) { Start-Sleep -Milliseconds 100 }
        $jobs += Start-Job -ScriptBlock $sb -ArgumentList $t,$script:Root,$PSScriptRoot
    }
    foreach ($j in $jobs) {
        $r = Wait-Job $j | Receive-Job; Remove-Job $j -Force
        Write-Host "  $(if($r.Success){'✅'}else{'⚠️'}) $($r.Task) ($($r.Elapsed)ms) $($r.Output)" -ForegroundColor $(if($r.Success){'Green'}else{'Yellow'})
    }
}

$kbPath = Join-Path $script:Root "data" "knowledge_base.json"
if (-not (Test-Path $kbPath) -or $DigestFirst) {
    & "$PSScriptRoot\source_digester.ps1" -Operation digest 2>$null
}

switch ($Action) {
    'enhanced' {
        if ($EnableExternalAPI) { & "$PSScriptRoot\ide_chatbot_enhanced.ps1" -Mode interactive -UseExternalAPIFallback }
        else { & "$PSScriptRoot\ide_chatbot_enhanced.ps1" -Mode interactive }
    }
    'voice' {
        & "$PSScriptRoot\voice_assistant.ps1" -Mode $(if($EnableVoice){'voice'}else{'interactive'}) @(if($EnableVoice){'-EnableVoice'})
    }
    'digest' { & "$PSScriptRoot\source_digester.ps1" -Operation digest }
    'validate' {
        $vp = Join-Path $script:Root "VALIDATE_REVERSE_ENGINEERING.ps1"
        if (Test-Path $vp) { & $vp } else { Write-Host "Validation script not found" -ForegroundColor Yellow }
    }
    'api' {
        if (-not $Prompt) { $Prompt = "Explain std::expected in C++20" }
        $prov = if ($Provider -and $Provider -ne 'openai') { $Provider } else { if ($env:ANTHROPIC_API_KEY) { "anthropic" } else { "openai" } }
        $out = Invoke-ExternalAPI -prompt $Prompt -provider $prov -apiKey $null -model $null
        if ($out) { Write-Output $out } else { Write-Host "API key missing or error" -ForegroundColor Yellow }
    }
    'parallel' {
        if ($Tasks.Count -eq 0) { $Tasks = @("Analyze src/", "Analyze scripts/") }
        Invoke-MultiAgent -tasks $Tasks -maxP $MaxParallel
    }
    '' { & "$PSScriptRoot\ide_chatbot_enhanced.ps1" -Mode interactive }
}
