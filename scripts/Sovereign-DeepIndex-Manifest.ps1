<#
.SYNOPSIS
  Read-only filesystem manifest for large trees (e.g. multi-TB archives).

.DESCRIPTION
  Walks roots with configurable depth, skips heavy/irrelevant directories, and
  exports metadata only (FullName, Length, LastWriteTimeUtc). No writes to
  indexed paths. PowerShell 5.1 compatible.

  This is NOT a vector database; it is the safe first step. Pair the CSV with
  your own embedding or LLM pipeline offline.

.PARAMETER RootPaths
  One or more roots to scan (default: RAWRXD_REPO_ROOT or D:\RawrXD).

.PARAMETER OutCsv
  Output CSV path (UTF-8).

.PARAMETER MaxDepth
  Recursion depth (-1 = unlimited; use with care on 11TB).

.PARAMETER MaxFiles
  Hard stop after this many files (safety).

.PARAMETER ExcludeDirNames
  Directory names to skip anywhere in the path (case-insensitive).
#>
[CmdletBinding()]
param(
    [string[]]$RootPaths = @(),
    [Parameter(Mandatory = $false)]
    [string]$OutCsv = "",
    [int]$MaxDepth = 8,
    [long]$MaxFiles = 500000,
    [string[]]$ExcludeDirNames = @(
        ".git", "node_modules", "build", "build-ninja", "build-win32", "out", ".vs",
        "__pycache__", "3rdparty", "dist", "Full Source", "history", "_deps"
    )
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Continue"

if (-not $OutCsv -or $OutCsv.Trim().Length -eq 0) {
    $repo = $env:RAWRXD_REPO_ROOT
    if (-not $repo) { $repo = "D:\RawrXD" }
    $logDir = Join-Path $repo "logs"
    if (-not (Test-Path -LiteralPath $logDir)) {
        New-Item -ItemType Directory -Path $logDir -Force | Out-Null
    }
    $OutCsv = Join-Path $logDir ("sovereign_index_manifest_{0:yyyyMMdd_HHmmss}.csv" -f (Get-Date))
}

if ($RootPaths.Count -eq 0) {
    $r = $env:RAWRXD_REPO_ROOT
    if (-not $r) { $r = "D:\RawrXD" }
    $RootPaths = @($r)
}

$excludeLookup = @{}
foreach ($n in $ExcludeDirNames) {
    $excludeLookup[$n.ToLowerInvariant()] = $true
}

function Test-ExcludedPath {
    param([string]$FullPath)
    $parts = $FullPath -split '[\\/]'
    foreach ($p in $parts) {
        if ($p.Length -eq 0) { continue }
        $key = $p.ToLowerInvariant()
        if ($excludeLookup.ContainsKey($key)) { return $true }
    }
    return $false
}

$count = 0
$rows = New-Object System.Collections.Generic.List[object]

foreach ($root in $RootPaths) {
    if (-not (Test-Path -LiteralPath $root)) {
        Write-Warning "Skip missing root: $root"
        continue
    }
    $rootFull = (Resolve-Path -LiteralPath $root).Path
    Write-Host "Scanning: $rootFull (MaxDepth=$MaxDepth MaxFiles=$MaxFiles)"

    $gciParams = @{
        Path        = $rootFull
        File        = $true
        Force       = $true
        ErrorAction = "SilentlyContinue"
    }
    if ($MaxDepth -ge 0) {
        $gciParams["Depth"] = $MaxDepth
    }

    Get-ChildItem @gciParams -Recurse | ForEach-Object {
        if ($count -ge $MaxFiles) { return }
        $fp = $_.FullName
        if (Test-ExcludedPath $fp) { return }
        $rows.Add([pscustomobject]@{
                FullName         = $fp
                Length           = $_.Length
                LastWriteTimeUtc = $_.LastWriteTimeUtc
            })
        $count++
        if (($count % 50000) -eq 0) { Write-Host "  ... $count files" }
    }
}

$rows | Export-Csv -LiteralPath $OutCsv -NoTypeInformation -Encoding UTF8
Write-Host "Wrote $count rows -> $OutCsv"
Write-Host "Next: run your embedding or llama-cli step offline; do not pipe destructive commands from model output."
