#!/usr/bin/env pwsh
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

param(
    [string]$Remote = "origin",
    [string]$MainBranch = "main",
    [string]$Prefix = "cursor/",
    [switch]$Push
)

function Invoke-Git {
    param([Parameter(Mandatory=$true)][string[]]$Args)
    $p = & git @Args 2>&1
    if ($LASTEXITCODE -ne 0) { throw "git $($Args -join ' ') failed: $p" }
    return $p
}

function Get-DeletedFilesInRange {
    param([string]$From, [string]$To)
    $out = & git diff --name-status "$From..$To"
    if ($LASTEXITCODE -ne 0) { throw "git diff failed" }
    $deleted = @()
    foreach ($line in ($out -split "`n")) {
        $t = $line.Trim()
        if (-not $t) { continue }
        if ($t.StartsWith("D`t")) {
            $deleted += $t.Substring(2).Trim()
        }
    }
    return $deleted
}

function Restore-DeletedFiles {
    param([string]$FromCommit, [string[]]$Files)
    if (-not $Files -or $Files.Count -eq 0) { return }
    foreach ($f in $Files) {
        & git checkout $FromCommit -- $f | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to restore $f from $FromCommit" }
    }
    Invoke-Git @("add","--") + $Files
    Invoke-Git @("commit","-m","restore: keep files (no deletions policy)")
}

# Ensure clean
$status = & git status --porcelain
if ($LASTEXITCODE -ne 0) { throw "git status failed" }
if ($status.Trim()) { throw "Working tree not clean. Commit/stash first." }

# Update refs
Invoke-Git @("fetch",$Remote,"--prune")

# Ensure main checked out and up to date
Invoke-Git @("checkout",$MainBranch)
Invoke-Git @("pull","--no-rebase",$Remote,$MainBranch)

# Enumerate remote branches
$refs = & git for-each-ref "refs/remotes/$Remote/$Prefix*" --format="%(refname:short)"
if ($LASTEXITCODE -ne 0) { throw "for-each-ref failed" }

$branches = @($refs | Where-Object { $_ -and $_.Trim() } | ForEach-Object { $_.Trim() })
foreach ($ref in $branches) {
    # Skip symbolic HEAD refs if present
    if ($ref -match "/HEAD$") { continue }

    # Check if branch head is already in main
    & git merge-base --is-ancestor $ref $MainBranch
    if ($LASTEXITCODE -eq 0) { continue }

    $before = (Invoke-Git @("rev-parse","HEAD")).Trim()

    try {
        Invoke-Git @("merge","--no-ff",$ref,"-m","merge: intake $ref into $MainBranch")
    } catch {
        # If merge fails, stop immediately (manual resolution required).
        throw
    }

    $after = (Invoke-Git @("rev-parse","HEAD")).Trim()
    $deleted = Get-DeletedFilesInRange -From $before -To $after
    if ($deleted.Count -gt 0) {
        Restore-DeletedFiles -FromCommit $before -Files $deleted
    }
}

if ($Push) {
    Invoke-Git @("push","-u",$Remote,$MainBranch)
}

