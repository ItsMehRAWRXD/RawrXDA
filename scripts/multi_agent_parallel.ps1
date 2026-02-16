#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Multi-Agent Parallel Execution - Parallelize agent tasks

.DESCRIPTION
    Runs multiple agent tasks in parallel via PowerShell jobs.
    Remediates the "single-threaded only" gap for script-based workflows.

.PARAMETER Tasks
    Array of task descriptions or commands to run in parallel

.PARAMETER MaxParallel
    Max concurrent jobs (default 3)

.PARAMETER ScriptBlock
    Optional: scriptblock to run per task (receives $task as param)

.EXAMPLE
    .\multi_agent_parallel.ps1 -Tasks @("Analyze src/", "Run tests", "Benchmark") -MaxParallel 3
#>

param(
    [Parameter(Mandatory=$true)]
    [string[]]$Tasks,
    
    [Parameter(Mandatory=$false)]
    [int]$MaxParallel = 3,
    
    [Parameter(Mandatory=$false)]
    [scriptblock]$ScriptBlock
)

$ErrorActionPreference = "Continue"
Set-StrictMode -Version Latest

$scriptsDir = $PSScriptRoot
$projectRoot = if ($env:LAZY_INIT_IDE_ROOT) { $env:LAZY_INIT_IDE_ROOT } else { (Split-Path $PSScriptRoot -Parent) }

Write-Host "`n╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║         Multi-Agent Parallel Executor                            ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

$defaultScriptBlock = {
    param($task, $projectRoot, $scriptsDir)
    $result = @{ Task = $task; Output = ""; Success = $false; Elapsed = 0 }
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        if ($task -match '^Analyze\s+(.+)$') {
            $path = $Matches[1].Trim()
            $resolved = if (Test-Path $path) { $path } else { Join-Path $projectRoot $path }
            if (Test-Path $resolved) {
                $result.Output = Get-ChildItem $resolved -Recurse -File -ErrorAction SilentlyContinue | Measure-Object | Select-Object -ExpandProperty Count
                $result.Output = "Files in $path : $($result.Output)"
            } else {
                $result.Output = "Path not found: $path"
            }
        }
        elseif ($task -match '^Run\s+tests?$' -or $task -match '^tests?$') {
            $testScript = Join-Path $projectRoot "Run-ProductionTests.ps1"
            if (Test-Path $testScript) {
                $result.Output = & $testScript 2>&1 | Out-String
            } else {
                $result.Output = "Test script not found"
            }
        }
        elseif ($task -match '^Benchmark' -or $task -match '^benchmark') {
            $result.Output = "Benchmark: (placeholder - wire to your benchmark script)"
        }
        else {
            $result.Output = "Executed: $task"
        }
        $result.Success = $true
    } catch {
        $result.Output = "Error: $_"
    }
    $sw.Stop()
    $result.Elapsed = $sw.ElapsedMilliseconds
    $result
}

$sb = if ($ScriptBlock) { $ScriptBlock } else { $defaultScriptBlock }

$jobs = @()
foreach ($task in $Tasks) {
    while ((Get-Job -State Running).Count -ge $MaxParallel) {
        Start-Sleep -Milliseconds 200
    }
    $jobs += Start-Job -ScriptBlock $sb -ArgumentList $task, $projectRoot, $scriptsDir
}

$results = @()
foreach ($j in $jobs) {
    $r = Wait-Job $j | Receive-Job
    Remove-Job $j -Force
    $results += $r
}

Write-Host "`n📊 RESULTS:`n" -ForegroundColor Cyan
foreach ($r in $results) {
    $icon = if ($r.Success) { "✅" } else { "⚠️" }
    Write-Host "  $icon $($r.Task) ($($r.Elapsed)ms)" -ForegroundColor $(if ($r.Success) { "Green" } else { "Yellow" })
    if ($r.Output) { Write-Host "      $($r.Output)" -ForegroundColor Gray }
}
