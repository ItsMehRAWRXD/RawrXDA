# ============================================================================
# RawrXD Production Subagent: TypeFixer
# Version: 1.0.0 | License: MIT
# Part of the RawrXD Autonomous Build System
# ============================================================================
#Requires -Version 7.0
<#
.SYNOPSIS
    Subagent: Type Fixer - Resolves type conversion and mismatch errors.

.DESCRIPTION
    Production-hardened subagent that parses MSVC compile error/warning logs
    for type conversion issues (C4244, etc.) and applies targeted fixes.
    Supports DWORD/int, LRESULT, WPARAM/LPARAM casts, backup creation,
    fix-location auditing, and structured reporting.

.PARAMETER ErrorLogPath
    Path to the compile error/warning log. Required.

.PARAMETER AutoFix
    Automatically apply type fixes. Without this, only a report is generated.

.EXAMPLE
    .\Subagent-TypeFixer.ps1 -ErrorLogPath .\compile_errors.txt -AutoFix -Verbose
    .\Subagent-TypeFixer.ps1 -ErrorLogPath .\build.log -WhatIf
#>

[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'Medium')]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]$ErrorLogPath,

    [switch]$AutoFix,

    [ValidateSet('Text', 'JSON')]
    [string]$OutputFormat = 'Text',

    [string]$ReportPath
)

# ── Bootstrap ────────────────────────────────────────────────────────────────
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ── Type-fix definitions ─────────────────────────────────────────────────────
$script:TypeFixes = @(
    # ── C4244: size_t → int ──────────────────────────────────────────────────
    @{
        Name    = 'size_t to int (C4244)'
        Pattern = "(?m)^(.+?)\((\d+)\).*warning C4244.*'size_t'.*'int'"
        Fix     = {
            param([string]$file, [int]$line)
            $content = Get-Content $file
            $lc = $content[$line - 1]
            if ($lc -match '\bint\s+(\w+)\s*=\s*(\w+)\.size\(\)') {
                $content[$line - 1] = $lc -replace '\bint\s+', 'size_t '
                Set-Content $file $content
                return $true
            }
            return $false
        }
    }

    # ── C4244: double → int ──────────────────────────────────────────────────
    @{
        Name    = 'double to int (C4244)'
        Pattern = "(?m)^(.+?)\((\d+)\).*warning C4244.*'double'.*'int'"
        Fix     = {
            param([string]$file, [int]$line)
            $content = Get-Content $file
            $lc = $content[$line - 1]
            if ($lc -match '\bint\s+(\w+)\s*=\s*([^;]+)') {
                $var  = $Matches[1]
                $content[$line - 1] = $lc -replace "\bint\s+$var\s*=\s*([^;]+);", "int $var = static_cast<int>(`$1);"
                Set-Content $file $content
                return $true
            }
            return $false
        }
    }

    # ── bool → std::string ───────────────────────────────────────────────────
    @{
        Name    = 'bool to std::string'
        Pattern = "(?m)^(.+?)\((\d+)\).*cannot convert.*'bool'.*'std::string'"
        Fix     = {
            param([string]$file, [int]$line)
            $content = Get-Content $file
            $lc = $content[$line - 1]
            if ($lc -match 'return\s+(true|false)\s*;') {
                $content[$line - 1] = $lc -replace 'return\s+(true|false)', 'return std::to_string($1)'
                Set-Content $file $content
                return $true
            }
            return $false
        }
    }

    # ── DWORD ↔ int ──────────────────────────────────────────────────────────
    @{
        Name    = 'DWORD to int conversion'
        Pattern = "(?m)^(.+?)\((\d+)\).*(?:warning C4244|cannot convert).*'DWORD'.*'int'"
        Fix     = {
            param([string]$file, [int]$line)
            $content = Get-Content $file
            $lc = $content[$line - 1]
            if ($lc -match '\bint\s+(\w+)\s*=\s*([^;]+);') {
                $var = $Matches[1]; $expr = $Matches[2]
                $content[$line - 1] = $lc -replace "\bint\s+$var\s*=\s*([^;]+);", "int $var = static_cast<int>(`$1);"
                Set-Content $file $content
                return $true
            }
            return $false
        }
    }
    @{
        Name    = 'int to DWORD conversion'
        Pattern = "(?m)^(.+?)\((\d+)\).*(?:warning C4244|cannot convert).*'int'.*'DWORD'"
        Fix     = {
            param([string]$file, [int]$line)
            $content = Get-Content $file
            $lc = $content[$line - 1]
            if ($lc -match '\bDWORD\s+(\w+)\s*=\s*([^;]+);') {
                $var = $Matches[1]
                $content[$line - 1] = $lc -replace "\bDWORD\s+$var\s*=\s*([^;]+);", "DWORD $var = static_cast<DWORD>(`$1);"
                Set-Content $file $content
                return $true
            }
            return $false
        }
    }

    # ── LRESULT → int ────────────────────────────────────────────────────────
    @{
        Name    = 'LRESULT to int'
        Pattern = "(?m)^(.+?)\((\d+)\).*(?:warning|cannot convert).*'LRESULT'.*'int'"
        Fix     = {
            param([string]$file, [int]$line)
            $content = Get-Content $file
            $lc = $content[$line - 1]
            if ($lc -match '\bint\s+(\w+)\s*=\s*([^;]+);') {
                $var = $Matches[1]
                $content[$line - 1] = $lc -replace "\bint\s+$var\s*=\s*([^;]+);", "int $var = static_cast<int>(`$1);"
                Set-Content $file $content
                return $true
            }
            return $false
        }
    }

    # ── WPARAM / LPARAM casts ─────────────────────────────────────────────────
    @{
        Name    = 'WPARAM cast'
        Pattern = "(?m)^(.+?)\((\d+)\).*(?:warning|cannot convert).*'WPARAM'"
        Fix     = {
            param([string]$file, [int]$line)
            $content = Get-Content $file
            $lc = $content[$line - 1]
            # Add explicit cast for WPARAM mismatches
            if ($lc -match '\b(int|unsigned|UINT|DWORD)\s+(\w+)\s*=\s*([^;]+);') {
                $type = $Matches[1]; $var = $Matches[2]
                $content[$line - 1] = $lc -replace "=\s*([^;]+);", "= static_cast<$type>(`$1);"
                Set-Content $file $content
                return $true
            }
            return $false
        }
    }
    @{
        Name    = 'LPARAM cast'
        Pattern = "(?m)^(.+?)\((\d+)\).*(?:warning|cannot convert).*'LPARAM'"
        Fix     = {
            param([string]$file, [int]$line)
            $content = Get-Content $file
            $lc = $content[$line - 1]
            if ($lc -match '\b(int|unsigned|UINT|DWORD|LONG_PTR)\s+(\w+)\s*=\s*([^;]+);') {
                $type = $Matches[1]; $var = $Matches[2]
                $content[$line - 1] = $lc -replace "=\s*([^;]+);", "= static_cast<$type>(`$1);"
                Set-Content $file $content
                return $true
            }
            return $false
        }
    }
)

# ── Main ──────────────────────────────────────────────────────────────────────
Write-Verbose "[TypeFixer] Error log: $ErrorLogPath"

$logContent = Get-Content $ErrorLogPath -Raw -ErrorAction Stop

$report = [PSCustomObject]@{
    ErrorsParsed  = 0
    FixesApplied  = 0
    FixLocations  = [System.Collections.Generic.List[string]]::new()   # file:line audit trail
    Skipped       = 0
    Errors        = [System.Collections.Generic.List[string]]::new()
}

$backedUp = [System.Collections.Generic.HashSet[string]]::new()

$totalPatterns = $script:TypeFixes.Count
$patIdx = 0

foreach ($fixDef in $script:TypeFixes) {
    $patIdx++
    Write-Progress -Activity 'TypeFixer — Scanning' `
        -Status "Pattern $patIdx / $totalPatterns : $($fixDef.Name)" `
        -PercentComplete ([math]::Floor(($patIdx / $totalPatterns) * 100))

    $matchList = [regex]::Matches($logContent, $fixDef.Pattern)
    $report.ErrorsParsed += $matchList.Count

    foreach ($match in $matchList) {
        $file = $match.Groups[1].Value.Trim()
        $line = [int]$match.Groups[2].Value

        # Skip .bak files
        if ($file -match '\.bak$') {
            $report.Skipped++
            continue
        }

        if (-not (Test-Path $file)) {
            Write-Verbose "  File not found, skipping: $file"
            $report.Skipped++
            continue
        }

        $location = "${file}:${line}"
        Write-Verbose "  Match: $($fixDef.Name) at $location"

        if ($AutoFix -and $PSCmdlet.ShouldProcess($location, $fixDef.Name)) {
            try {
                # Backup once per file
                if (-not $backedUp.Contains($file)) {
                    $bakPath = "${file}.bak"
                    Copy-Item $file $bakPath -Force -ErrorAction Stop
                    [void]$backedUp.Add($file)
                    Write-Verbose "  Backup → $bakPath"
                }

                $result = & $fixDef.Fix $file $line
                if ($result) {
                    $report.FixesApplied++
                    $report.FixLocations.Add($location)
                    Write-Verbose "  Fixed: $location"
                }
                else {
                    Write-Verbose "  Could not auto-fix: $location"
                    $report.Skipped++
                }
            }
            catch {
                $msg = "Error fixing $location : $_"
                Write-Warning $msg
                $report.Errors.Add($msg)
            }
        }
    }
}

Write-Progress -Activity 'TypeFixer — Scanning' -Completed

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host ''
Write-Host '╔══════════════════════════════════════════════╗' -ForegroundColor Cyan
Write-Host '║  TypeFixer — Summary                         ║' -ForegroundColor Cyan
Write-Host '╠══════════════════════════════════════════════╣' -ForegroundColor Cyan
Write-Host "║  Errors parsed   : $($report.ErrorsParsed)" -ForegroundColor Cyan
Write-Host "║  Fixes applied   : $($report.FixesApplied)" -ForegroundColor $(if ($report.FixesApplied) { 'Green' } else { 'Cyan' })
Write-Host "║  Skipped         : $($report.Skipped)" -ForegroundColor Cyan
Write-Host "║  Errors          : $($report.Errors.Count)" -ForegroundColor $(if ($report.Errors.Count) { 'Red' } else { 'Cyan' })
Write-Host '╚══════════════════════════════════════════════╝' -ForegroundColor Cyan

if ($report.FixLocations.Count -gt 0) {
    Write-Host "`nAudit trail (file:line):" -ForegroundColor Green
    $report.FixLocations | ForEach-Object { Write-Host "  • $_" -ForegroundColor Green }
}

if ($report.Errors.Count -gt 0) {
    Write-Host "`nErrors:" -ForegroundColor Red
    $report.Errors | ForEach-Object { Write-Host "  • $_" -ForegroundColor Red }
}

# ── JSON report ───────────────────────────────────────────────────────────────
if ($ReportPath) {
    try {
        $report | ConvertTo-Json -Depth 4 | Set-Content $ReportPath -Encoding utf8
        Write-Verbose "Report written → $ReportPath"
    }
    catch { Write-Warning "Could not write report: $_" }
}

if ($OutputFormat -eq 'JSON') {
    $report | ConvertTo-Json -Depth 4
}
else {
    Write-Output $report
}

if ($report.Errors.Count -gt 0) { exit 1 }
exit 0
