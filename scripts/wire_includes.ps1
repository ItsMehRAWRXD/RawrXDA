#!/usr/bin/env pwsh
# ═══════════════════════════════════════════════════════════════════════════════
# wire_includes.ps1 - Wire rawrxd_master.inc into all 64-bit MASM .asm files
# ═══════════════════════════════════════════════════════════════════════════════
# This script adds `INCLUDE rawrxd_master.inc` to all 64-bit MASM-syntax .asm
# files that don't already have it. It skips 32-bit files (.model flat) and
# NASM-syntax files (section .text, %include, etc.).
#
# The include is inserted AFTER the `OPTION CASEMAP:NONE` line when present,
# or before the first `.code` / `.data` / `PUBLIC` / `EXTERN` directive.
#
# Usage:
#   .\wire_includes.ps1 [-DryRun] [-Verbose]
#
# ═══════════════════════════════════════════════════════════════════════════════

param(
    [switch]$DryRun,
    [switch]$VerboseLog
)

$RootDir = "D:\RawrXD"
$IncludeDir = "D:\RawrXD\include"
$IncludeLine = "INCLUDE rawrxd_master.inc"

$stats = @{
    Total     = 0
    Skipped32 = 0
    SkippedNASM = 0
    AlreadyHas = 0
    Modified  = 0
    Failed    = 0
    Errors    = @()
}

# Get all .asm files recursively
$allFiles = Get-ChildItem -Path $RootDir -Filter "*.asm" -Recurse -File |
    Where-Object { $_.FullName -notmatch '\\node_modules\\|\\\.git\\|\\build\\|\\itsmehrawrxd-master\\|\\OrganizedPiProject\\' }

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host " RawrXD Include Wiring Script" -ForegroundColor Cyan
Write-Host " Found $($allFiles.Count) .asm files under $RootDir" -ForegroundColor Cyan
if ($DryRun) { Write-Host " *** DRY RUN - no files will be modified ***" -ForegroundColor Yellow }
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

foreach ($file in $allFiles) {
    $stats.Total++
    $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) {
        $stats.Failed++
        $stats.Errors += "Could not read: $($file.FullName)"
        continue
    }

    $lines = $content -split "`r?`n"

    # Skip 32-bit files (.model flat)
    if ($content -match '(?im)^\s*\.model\s+flat') {
        $stats.Skipped32++
        if ($VerboseLog) { Write-Host "  [32-bit] $($file.FullName)" -ForegroundColor DarkGray }
        continue
    }

    # Skip NASM-syntax files
    if ($content -match '(?im)(^\s*section\s+\.(text|data|bss)|^\s*%include|^\s*global\s+|^\s*bits\s+(32|64))') {
        $stats.SkippedNASM++
        if ($VerboseLog) { Write-Host "  [NASM]   $($file.FullName)" -ForegroundColor DarkGray }
        continue
    }

    # Skip if already includes rawrxd_master.inc
    if ($content -match '(?im)^\s*INCLUDE\s+rawrxd_master\.inc') {
        $stats.AlreadyHas++
        if ($VerboseLog) { Write-Host "  [OK]     $($file.FullName)" -ForegroundColor DarkGreen }
        continue
    }

    # Find insertion point
    $insertIdx = -1
    $insertAfter = $false  # true = insert AFTER line at $insertIdx, false = insert BEFORE

    # Strategy 1: Insert after OPTION CASEMAP:NONE
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '(?i)^\s*OPTION\s+CASEMAP\s*:\s*NONE') {
            $insertIdx = $i
            $insertAfter = $true
            break
        }
    }

    # Strategy 2: If no OPTION CASEMAP, insert before first .code / .data / PUBLIC / EXTERN
    if ($insertIdx -eq -1) {
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $trimmed = $lines[$i].Trim()
            # Skip comments and blank lines
            if ($trimmed -eq '' -or $trimmed.StartsWith(';')) { continue }
            # Skip title/subtitle directives
            if ($trimmed -match '(?i)^(TITLE|SUBTITLE|COMMENT)\b') { continue }

            if ($trimmed -match '(?i)^(\.code|\.data|\.const|PUBLIC\s|EXTERN\s|EXTERNDEF\s|includelib\s)') {
                $insertIdx = $i
                $insertAfter = $false
                break
            }
        }
    }

    # Strategy 3: If still no good point, insert at line 1 (after any initial comment block)
    if ($insertIdx -eq -1) {
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $trimmed = $lines[$i].Trim()
            if ($trimmed -eq '' -or $trimmed.StartsWith(';')) { continue }
            $insertIdx = $i
            $insertAfter = $false
            break
        }
    }

    if ($insertIdx -eq -1) {
        # File is entirely comments or empty
        $stats.Failed++
        $stats.Errors += "No insertion point found: $($file.FullName)"
        continue
    }

    # Build new content
    $newLines = [System.Collections.Generic.List[string]]::new()

    if ($insertAfter) {
        # Insert AFTER the line at $insertIdx
        for ($i = 0; $i -le $insertIdx; $i++) {
            $newLines.Add($lines[$i])
        }
        $newLines.Add("")
        $newLines.Add("; ─── Cross-module symbol resolution ───")
        $newLines.Add($IncludeLine)
        $newLines.Add("")
        for ($i = $insertIdx + 1; $i -lt $lines.Count; $i++) {
            $newLines.Add($lines[$i])
        }
    } else {
        # Insert BEFORE the line at $insertIdx
        for ($i = 0; $i -lt $insertIdx; $i++) {
            $newLines.Add($lines[$i])
        }
        $newLines.Add("")
        $newLines.Add("; ─── Cross-module symbol resolution ───")
        $newLines.Add($IncludeLine)
        $newLines.Add("")
        for ($i = $insertIdx; $i -lt $lines.Count; $i++) {
            $newLines.Add($lines[$i])
        }
    }

    $newContent = $newLines -join "`r`n"

    if ($DryRun) {
        Write-Host "  [WOULD]  $($file.FullName) @ line $($insertIdx + 1) ($( if ($insertAfter) {'after'} else {'before'} ))" -ForegroundColor Yellow
    } else {
        try {
            $utf8NoBom = New-Object System.Text.UTF8Encoding $false
            [System.IO.File]::WriteAllText($file.FullName, $newContent, $utf8NoBom)
            $stats.Modified++
            if ($VerboseLog) { Write-Host "  [DONE]   $($file.FullName)" -ForegroundColor Green }
        } catch {
            $stats.Failed++
            $stats.Errors += "Write failed: $($file.FullName) - $_"
        }
    }
}

# Summary
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host " SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Total .asm files:     $($stats.Total)"
Write-Host "  Skipped (32-bit):     $($stats.Skipped32)" -ForegroundColor DarkGray
Write-Host "  Skipped (NASM):       $($stats.SkippedNASM)" -ForegroundColor DarkGray
Write-Host "  Already has include:  $($stats.AlreadyHas)" -ForegroundColor DarkGreen
if ($DryRun) {
    Write-Host "  Would modify:         $($stats.Total - $stats.Skipped32 - $stats.SkippedNASM - $stats.AlreadyHas - $stats.Failed)" -ForegroundColor Yellow
} else {
    Write-Host "  Modified:             $($stats.Modified)" -ForegroundColor Green
}
Write-Host "  Errors:               $($stats.Failed)" -ForegroundColor $(if ($stats.Failed -gt 0) { 'Red' } else { 'DarkGray' })

if ($stats.Errors.Count -gt 0) {
    Write-Host ""
    Write-Host "  Error Details:" -ForegroundColor Red
    foreach ($err in $stats.Errors) {
        Write-Host "    - $err" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "  NOTE: Ensure your build system passes /I $IncludeDir to ml64.exe" -ForegroundColor Yellow
Write-Host "  Example: ml64.exe /c /I $IncludeDir yourfile.asm" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
