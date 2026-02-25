#!/usr/bin/env pwsh
Set-StrictMode -Version Latest

function Get-RawrXDRoot {
    <#
      Returns the single canonical project root.
      Priority:
        1) $env:LAZY_INIT_IDE_ROOT
        2) repo root relative to scripts/ (..)
        3) current directory
    #>
    if ($env:LAZY_INIT_IDE_ROOT -and $env:LAZY_INIT_IDE_ROOT.Trim()) {
        return $env:LAZY_INIT_IDE_ROOT.Trim()
    }
    $root = (Resolve-Path (Join-Path $PSScriptRoot "..") -ErrorAction SilentlyContinue).Path
    if ($root) { return $root }
    return (Get-Location).Path
}

function Resolve-RawrXDPath {
    <#
      Normalizes legacy multi-drive paths into the single canonical root.
      This does NOT require the legacy path to exist; it rewrites by prefix.
    #>
    param([Parameter(Mandatory=$true)][string]$Path)

    $root = Get-RawrXDRoot
    $p = $Path

    # Normalize common legacy repo-root prefixes.
    $legacyPrefixes = @(
        "D:\lazy init ide",
        "D:\rawrxd",
        "D:\RawrXD",
        "E:\rawrxd",
        "E:\RawrXD"
    )
    foreach ($lp in $legacyPrefixes) {
        if ($p -like "$lp*") {
            $suffix = $p.Substring($lp.Length).TrimStart('\','/')
            return (Join-Path $root $suffix)
        }
    }

    # If it's already absolute, leave it.
    if ([System.IO.Path]::IsPathRooted($p)) { return $p }
    return (Join-Path $root $p)
}

