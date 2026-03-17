# ============================================================================
# RawrXD Production Orchestrator: Subagent Pipeline Controller
# Version: 2.0.0 | License: MIT
# ============================================================================
#Requires -Version 7.0
<#
.SYNOPSIS
    Master orchestrator — runs all subagents in convergence loops until saturation.

.DESCRIPTION
    Production-hardened orchestrator that launches IncludeResolver, SyntaxHealer,
    TypeFixer, and SymbolLinker in priority order (or parallel). Properly consumes
    each subagent's structured [PSCustomObject] output, tracks cumulative metrics,
    can auto-detect error logs from prior builds, and exports a JSON report.

.PARAMETER MaxIterations
    Maximum fix-build-analyze cycles. Default: 15.

.PARAMETER AutoFix
    Pass -AutoFix into every subagent.

.PARAMETER RunBuildAfterEachCycle
    Invoke RawrXD-Build.ps1 between cycles to regenerate error/linker logs.

.PARAMETER ErrorLogPath
    Explicit path to compile error log (fed to TypeFixer). Auto-detected if omitted.

.PARAMETER LinkerLogPath
    Explicit path to linker error log (fed to SymbolLinker). Auto-detected if omitted.

.PARAMETER ScanPath
    Source tree root for SyntaxHealer and IncludeResolver. Default: .\src

.PARAMETER Parallel
    Run subagents on separate runspaces concurrently within each iteration.

.PARAMETER OutputFormat
    Console output format: Text (default) or JSON.

.PARAMETER ReportPath
    Write final JSON report to this path. Default: .\orchestrator_report.json

.EXAMPLE
    .\Orchestrate-AllSubagents.ps1 -AutoFix -RunBuildAfterEachCycle
    .\Orchestrate-AllSubagents.ps1 -AutoFix -Parallel -OutputFormat JSON
    .\Orchestrate-AllSubagents.ps1 -ErrorLogPath .\build.log -LinkerLogPath .\link.log -AutoFix
#>

[CmdletBinding()]
param(
    [ValidateRange(1, 100)]
    [int]$MaxIterations = 15,

    [switch]$AutoFix,
    [switch]$RunBuildAfterEachCycle,

    [string]$ErrorLogPath,
    [string]$LinkerLogPath,
    [string]$ScanPath,

    [switch]$Parallel,

    [ValidateSet('Text', 'JSON')]
    [string]$OutputFormat = 'Text',

    [string]$ReportPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $ScanPath) { $ScanPath = Join-Path $PSScriptRoot 'src' }
if (-not $ReportPath) { $ReportPath = Join-Path $PSScriptRoot 'orchestrator_report.json' }

# ── Auto-detect logs ─────────────────────────────────────────────────────────
function Find-LatestLog {
    param([string]$Pattern)
    Get-ChildItem $PSScriptRoot -Filter $Pattern -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (-not $ErrorLogPath) {
    $ErrorLogPath = Find-LatestLog 'build_errors*.log'
    if (-not $ErrorLogPath) { $ErrorLogPath = Find-LatestLog 'compile_errors*.txt' }
    if (-not $ErrorLogPath) { $ErrorLogPath = Find-LatestLog '*build*.log' }
}
if (-not $LinkerLogPath) {
    $LinkerLogPath = Find-LatestLog 'linker_*.log'
    if (-not $LinkerLogPath) { $LinkerLogPath = Find-LatestLog '*link*.log' }
    if (-not $LinkerLogPath) { $LinkerLogPath = $ErrorLogPath }   # many builds emit both to one log
}

# ── Subagent registry ────────────────────────────────────────────────────────
$subagents = @(
    @{
        Name     = 'IncludeResolver'
        Script   = Join-Path $PSScriptRoot 'Subagent-IncludeResolver.ps1'
        Priority = 1
        Params   = { @{ ScanPath = $ScanPath } }
        FixKey   = 'IncludesAdded'
    }
    @{
        Name     = 'SyntaxHealer'
        Script   = Join-Path $PSScriptRoot 'Subagent-SyntaxHealer.ps1'
        Priority = 2
        Params   = { @{ ScanPath = $ScanPath } }
        FixKey   = 'TotalFixes'
    }
    @{
        Name     = 'TypeFixer'
        Script   = Join-Path $PSScriptRoot 'Subagent-TypeFixer.ps1'
        Priority = 3
        Params   = { if ($ErrorLogPath -and (Test-Path $ErrorLogPath)) { @{ ErrorLogPath = $ErrorLogPath } } else { $null } }
        FixKey   = 'FixesApplied'
    }
    @{
        Name     = 'SymbolLinker'
        Script   = Join-Path $PSScriptRoot 'Subagent-SymbolLinker.ps1'
        Priority = 4
        Params   = { if ($LinkerLogPath -and (Test-Path $LinkerLogPath)) { @{ LogPath = $LinkerLogPath } } else { $null } }
        FixKey   = 'StubsGenerated'
    }
)

# ── Helpers ───────────────────────────────────────────────────────────────────
function Extract-FixCount {
    param($AgentOutput, [string]$FixKey)
    # Subagents emit [PSCustomObject] via Write-Output — extract the property
    if ($AgentOutput -is [PSCustomObject] -and $AgentOutput.PSObject.Properties[$FixKey]) {
        $val = $AgentOutput.$FixKey
        if ($val -is [int]) { return $val }
        if ($val -is [hashtable] -or $val -is [System.Collections.Specialized.OrderedDictionary]) {
            return ($val.Values | Measure-Object -Sum).Sum
        }
        return [int]$val
    }
    # Fallback: search stringified output for the key or "fixes"
    $str = $AgentOutput | Out-String
    if ($str -match "$FixKey\s*[:=]\s*(\d+)") { return [int]$Matches[1] }
    if ($str -match 'Fixes applied\s*:\s*(\d+)') { return [int]$Matches[1] }
    if ($str -match 'Total fixes\s*:\s*(\d+)') { return [int]$Matches[1] }
    return 0
}

function Invoke-SubagentSerial {
    param([array]$Agents, [switch]$Fix)
    $results = [System.Collections.Generic.List[hashtable]]::new()
    foreach ($agent in ($Agents | Sort-Object { $_.Priority })) {
        $r = Invoke-SingleSubagent -Agent $agent -Fix:$Fix
        $results.Add($r)
    }
    return $results
}

function Invoke-SubagentParallel {
    param([array]$Agents, [switch]$Fix)
    $jobs = @()
    foreach ($agent in $Agents) {
        $sb = {
            param($ScriptPath, $Params)
            if ($Params) { & $ScriptPath @Params }
            else { & $ScriptPath }
        }
        $p = & $agent.Params
        if ($null -eq $p -and $agent.Name -in @('TypeFixer','SymbolLinker')) { continue }
        if ($Fix -and $p) { $p['AutoFix'] = $true }
        $jobs += @{
            Agent = $agent
            Job   = Start-ThreadJob -ScriptBlock $sb -ArgumentList $agent.Script, $p
        }
    }
    $results = [System.Collections.Generic.List[hashtable]]::new()
    foreach ($j in $jobs) {
        try {
            $output = Receive-Job -Job $j.Job -Wait -AutoRemoveJob -ErrorAction Stop
            $fixCount = Extract-FixCount $output $j.Agent.FixKey
            $results.Add(@{ Name = $j.Agent.Name; Fixes = $fixCount; Output = $output; Error = $null })
        }
        catch {
            $results.Add(@{ Name = $j.Agent.Name; Fixes = 0; Output = $null; Error = $_.ToString() })
        }
    }
    return $results
}

function Invoke-SingleSubagent {
    param([hashtable]$Agent, [switch]$Fix)
    if (-not (Test-Path $Agent.Script)) {
        Write-Warning "Subagent script not found: $($Agent.Script)"
        return @{ Name = $Agent.Name; Fixes = 0; Output = $null; Error = 'Script not found' }
    }
    $p = & $Agent.Params
    if ($null -eq $p -and $Agent.Name -in @('TypeFixer','SymbolLinker')) {
        Write-Verbose "  Skipping $($Agent.Name) — no log file available"
        return @{ Name = $Agent.Name; Fixes = 0; Output = $null; Error = 'No log file' }
    }
    if ($Fix -and $p) { $p['AutoFix'] = $true }
    try {
        $output = if ($p) { & $Agent.Script @p } else { & $Agent.Script }
        $fixCount = Extract-FixCount $output $Agent.FixKey
        return @{ Name = $Agent.Name; Fixes = $fixCount; Output = $output; Error = $null }
    }
    catch {
        Write-Warning "Subagent $($Agent.Name) failed: $_"
        return @{ Name = $Agent.Name; Fixes = 0; Output = $null; Error = $_.ToString() }
    }
}

# ── Main Loop ─────────────────────────────────────────────────────────────────
Write-Host '╔══════════════════════════════════════════════════════════╗' -ForegroundColor Magenta
Write-Host '║   SUBAGENT ORCHESTRATOR v2.0 — PRODUCTION ENGINE        ║' -ForegroundColor Magenta
Write-Host '╚══════════════════════════════════════════════════════════╝' -ForegroundColor Magenta
Write-Host "  Mode       : $(if ($Parallel) { 'PARALLEL' } else { 'SERIAL' })"
Write-Host "  Scan path  : $ScanPath"
Write-Host "  Error log  : $(if ($ErrorLogPath) { $ErrorLogPath } else { '(none)' })"
Write-Host "  Linker log : $(if ($LinkerLogPath) { $LinkerLogPath } else { '(none)' })"

$globalStats = [PSCustomObject]@{
    StartTime       = (Get-Date).ToString('o')
    Iterations      = 0
    TotalFixes      = 0
    Converged       = $false
    ByAgent         = [ordered]@{}
    IterationDetail = [System.Collections.Generic.List[object]]::new()
    Errors          = [System.Collections.Generic.List[string]]::new()
}

for ($i = 1; $i -le $MaxIterations; $i++) {
    Write-Host "`n═══ ITERATION $i / $MaxIterations ═══" -ForegroundColor Cyan
    $globalStats.Iterations = $i
    $cycleFixCount = 0

    $results = if ($Parallel) {
        Invoke-SubagentParallel -Agents $subagents -Fix:$AutoFix
    }
    else {
        Invoke-SubagentSerial -Agents $subagents -Fix:$AutoFix
    }

    $iterDetail = [ordered]@{ Iteration = $i; Agents = @{} }
    foreach ($r in $results) {
        Write-Host "  $($r.Name): $($r.Fixes) fix$(if ($r.Fixes -ne 1) { 'es' })" -ForegroundColor $(if ($r.Fixes) { 'Green' } else { 'DarkGray' })
        if ($r.Error) {
            Write-Host "    ⚠ $($r.Error)" -ForegroundColor Yellow
            $globalStats.Errors.Add("Iter $i / $($r.Name): $($r.Error)")
        }
        $cycleFixCount += $r.Fixes
        if (-not $globalStats.ByAgent.Contains($r.Name)) { $globalStats.ByAgent[$r.Name] = 0 }
        $globalStats.ByAgent[$r.Name] += $r.Fixes
        $iterDetail.Agents[$r.Name] = $r.Fixes
    }
    $iterDetail['CycleFixes'] = $cycleFixCount
    $globalStats.IterationDetail.Add($iterDetail)
    $globalStats.TotalFixes += $cycleFixCount

    Write-Host "  ── Cycle total: $cycleFixCount" -ForegroundColor Cyan

    if ($cycleFixCount -eq 0) {
        Write-Host "`n[CONVERGENCE] No fixes applied — subagents saturated." -ForegroundColor Green
        $globalStats.Converged = $true
        break
    }

    if ($RunBuildAfterEachCycle) {
        Write-Host "`n  Rebuilding to regenerate logs..." -ForegroundColor DarkCyan
        try {
            $buildScript = Join-Path $PSScriptRoot 'RawrXD-Build.ps1'
            if (Test-Path $buildScript) { & $buildScript -Config Debug *>&1 | Out-Null }
            else { Write-Verbose 'Build script not found — skipping rebuild' }
        }
        catch { Write-Warning "Build failed (expected during fix cycles): $_" }
    }

    Start-Sleep -Milliseconds 250
}

$globalStats | Add-Member -NotePropertyName EndTime -NotePropertyValue (Get-Date).ToString('o')

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host ''
Write-Host '╔══════════════════════════════════════════════════════════╗' -ForegroundColor Green
Write-Host '║            ORCHESTRATION COMPLETE                        ║' -ForegroundColor Green
Write-Host '╠══════════════════════════════════════════════════════════╣' -ForegroundColor Green
Write-Host "║  Iterations : $($globalStats.Iterations)" -ForegroundColor Green
Write-Host "║  Total fixes: $($globalStats.TotalFixes)" -ForegroundColor $(if ($globalStats.TotalFixes) { 'Green' } else { 'Cyan' })
Write-Host "║  Converged  : $($globalStats.Converged)" -ForegroundColor $(if ($globalStats.Converged) { 'Green' } else { 'Yellow' })
Write-Host "║  Errors     : $($globalStats.Errors.Count)" -ForegroundColor $(if ($globalStats.Errors.Count) { 'Red' } else { 'Green' })
Write-Host '╠──────────────────────────────────────────────────────────╣' -ForegroundColor Green
foreach ($name in $globalStats.ByAgent.Keys) {
    Write-Host "║  $($name.PadRight(20)): $($globalStats.ByAgent[$name])" -ForegroundColor Yellow
}
Write-Host '╚══════════════════════════════════════════════════════════╝' -ForegroundColor Green

# ── JSON report ───────────────────────────────────────────────────────────────
try {
    $globalStats | ConvertTo-Json -Depth 5 | Set-Content $ReportPath -Encoding utf8 -ErrorAction Stop
    Write-Host "`nReport → $ReportPath" -ForegroundColor DarkGray
}
catch { Write-Warning "Could not write report: $_" }

if ($OutputFormat -eq 'JSON') {
    $globalStats | ConvertTo-Json -Depth 5
}
else {
    Write-Output $globalStats
}

if ($globalStats.Errors.Count -gt 0) { exit 1 }
exit 0
