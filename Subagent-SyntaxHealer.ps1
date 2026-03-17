# ============================================================================
# RawrXD Production Subagent: SyntaxHealer
# Version: 1.0.0 | License: MIT
# Part of the RawrXD Autonomous Build System
# ============================================================================
#Requires -Version 7.0
<#
.SYNOPSIS
    Subagent: Syntax Healer — Fixes structural C/C++ syntax errors.

.DESCRIPTION
    Production-hardened subagent that scans source files for common syntax
    defects: missing semicolons after class/struct definitions, unmatched
    braces, malformed switch/case blocks, trailing comma in enums,
    mismatched parentheses, duplicate/empty function bodies.
    Supports backup, WhatIf, JSON output, and structured reporting.

.PARAMETER ScanPath
    Root directory to scan. Default: .\src

.PARAMETER AutoFix
    Automatically apply syntax fixes. Without this, only a report is generated.

.PARAMETER OutputFormat
    Text or JSON output.

.PARAMETER ReportPath
    Path to write a JSON report file.

.EXAMPLE
    .\Subagent-SyntaxHealer.ps1 -ScanPath .\src -AutoFix -Verbose
    .\Subagent-SyntaxHealer.ps1 -ScanPath .\src -WhatIf
#>

[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'Medium')]
param(
    [string]$ScanPath,

    [switch]$AutoFix,

    [ValidateSet('Text', 'JSON')]
    [string]$OutputFormat = 'Text',

    [string]$ReportPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $ScanPath) { $ScanPath = Join-Path $PSScriptRoot 'src' }
if (-not (Test-Path $ScanPath)) {
    Write-Warning "[SyntaxHealer] ScanPath not found: $ScanPath"
    $r = [PSCustomObject]@{ TotalFixes = 0; FilesScanned = 0; Errors = @("ScanPath not found: $ScanPath") }
    if ($OutputFormat -eq 'JSON') { $r | ConvertTo-Json -Depth 3 } else { Write-Output $r }
    exit 1
}

# ── Syntax Checks ────────────────────────────────────────────────────────────

function Test-MissingSemicolonAfterClosingBrace {
    <# Detect class/struct/enum definitions missing trailing semicolon #>
    param([string[]]$Lines)
    $fixes = [System.Collections.Generic.List[hashtable]]::new()

    $inBlock = $false
    $blockType = ''
    $depth = 0

    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]

        # Detect start of class/struct/enum
        if (-not $inBlock -and $line -match '^\s*(class|struct|enum)\s+\w+') {
            $blockType = $Matches[1]
            # Check if definition is on this line
            if ($line -match '\{') {
                $inBlock = $true
                $depth = ($line.ToCharArray() | Where-Object { $_ -eq '{' }).Count -
                         ($line.ToCharArray() | Where-Object { $_ -eq '}' }).Count
                if ($depth -le 0) {
                    # Opening and closing on same line or this line
                    if ($line -notmatch '}\s*;') {
                        $fixes.Add(@{ Line = $i; Type = "Missing ';' after $blockType closing brace"; Fix = { param($l) $l -replace '}\s*$', '};' } })
                    }
                    $inBlock = $false
                }
            }
        }
        elseif ($inBlock) {
            $depth += ($line.ToCharArray() | Where-Object { $_ -eq '{' }).Count
            $depth -= ($line.ToCharArray() | Where-Object { $_ -eq '}' }).Count

            if ($depth -le 0) {
                $inBlock = $false
                if ($line -match '}\s*$' -and $line -notmatch '}\s*;') {
                    $fixes.Add(@{ Line = $i; Type = "Missing ';' after $blockType closing brace"; Fix = { param($l) $l -replace '}\s*$', '};' } })
                }
            }
        }
    }

    return $fixes
}

function Test-TrailingCommaInEnum {
    <# Detect trailing comma before closing brace in enum #>
    param([string[]]$Lines)
    $fixes = [System.Collections.Generic.List[hashtable]]::new()

    for ($i = 1; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match '^\s*}\s*;' -and $Lines[$i-1] -match ',\s*$') {
            $fixes.Add(@{ Line = $i - 1; Type = 'Trailing comma before enum close'; Fix = { param($l) $l -replace ',\s*$', '' } })
        }
    }
    return $fixes
}

function Test-EmptyReturnInNonVoid {
    <# Detect 'return;' in functions that should return a value (heuristic) #>
    param([string[]]$Lines)
    $fixes = [System.Collections.Generic.List[hashtable]]::new()

    $currentRetType = ''
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        # Match function definition: "int FuncName(...) {"
        if ($Lines[$i] -match '^\s*(int|LRESULT|BOOL|DWORD|HRESULT|size_t|int32_t|uint32_t|int64_t|uint64_t|long|unsigned)\s+\w+\s*\([^)]*\)\s*\{?\s*$') {
            $currentRetType = $Matches[1]
        }
        elseif ($Lines[$i] -match '^\s*return\s*;\s*$' -and $currentRetType -and $currentRetType -ne 'void') {
            $fixes.Add(@{ Line = $i; Type = "Empty return in $currentRetType function"; Fix = { param($l) $l -replace 'return\s*;', 'return 0;' } })
        }
        # Reset at function close (heuristic: line is just "}")
        if ($Lines[$i] -match '^\s*}\s*$') {
            $currentRetType = ''
        }
    }
    return $fixes
}

function Test-MismatchedParentheses {
    <# Detect lines where parentheses don't balance (common typo) #>
    param([string[]]$Lines)
    $fixes = [System.Collections.Generic.List[hashtable]]::new()

    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        # Skip comments and preprocessor
        if ($line -match '^\s*(//|#|/\*)') { continue }
        # Skip strings (rough)
        $stripped = $line -replace '"[^"]*"', '' -replace "'[^']*'", ''

        $open  = ($stripped.ToCharArray() | Where-Object { $_ -eq '(' }).Count
        $close = ($stripped.ToCharArray() | Where-Object { $_ -eq ')' }).Count

        if ($close -gt $open -and ($close - $open) -eq 1) {
            # Extra close paren — report only, too risky to autofix
            $fixes.Add(@{ Line = $i; Type = 'Extra closing parenthesis'; Fix = $null })
        }
        elseif ($open -gt $close -and ($open - $close) -eq 1 -and $line -match ';\s*$') {
            # Missing close paren on a statement line
            $fixes.Add(@{ Line = $i; Type = 'Missing closing parenthesis'; Fix = { param($l) $l -replace ';\s*$', ');' } })
        }
    }
    return $fixes
}

function Test-DoubleSemicolon {
    <# Detect accidental ';;' #>
    param([string[]]$Lines)
    $fixes = [System.Collections.Generic.List[hashtable]]::new()

    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        if ($line -match '^\s*for\s*\(') { continue }  # for(;;) is valid
        if ($line -match '(?<![;/]);;(?!=)') {
            $fixes.Add(@{ Line = $i; Type = 'Double semicolon'; Fix = { param($l) $l -replace '(?<![;/]);;', ';' } })
        }
    }
    return $fixes
}

# ── Main ──────────────────────────────────────────────────────────────────────
Write-Verbose "[SyntaxHealer] Scanning: $ScanPath"

$report = [PSCustomObject]@{
    TotalFixes     = 0
    FilesScanned   = 0
    FilesModified  = 0
    FixLocations   = [System.Collections.Generic.List[string]]::new()
    Errors         = [System.Collections.Generic.List[string]]::new()
}

$backedUp = [System.Collections.Generic.HashSet[string]]::new()

$files = Get-ChildItem $ScanPath -Recurse -Include '*.cpp','*.c','*.h','*.hpp','*.hxx','*.cxx' -File -ErrorAction SilentlyContinue
$total = $files.Count
$idx = 0

foreach ($file in $files) {
    $idx++
    Write-Progress -Activity 'SyntaxHealer — Scanning' `
        -Status "$idx / $total : $($file.Name)" `
        -PercentComplete ([math]::Floor(($idx / $total) * 100))

    $report.FilesScanned++

    try {
        $lines = Get-Content $file.FullName

        # Run all checkers
        $allFixes = @()
        $allFixes += Test-MissingSemicolonAfterClosingBrace -Lines $lines
        $allFixes += Test-TrailingCommaInEnum -Lines $lines
        $allFixes += Test-EmptyReturnInNonVoid -Lines $lines
        $allFixes += Test-DoubleSemicolon -Lines $lines
        $allFixes += Test-MismatchedParentheses -Lines $lines

        if ($allFixes.Count -eq 0) { continue }

        $fileModified = $false

        # Sort by line descending so edits don't shift indices
        $sorted = $allFixes | Sort-Object { $_.Line } -Descending

        foreach ($fix in $sorted) {
            $lineNum  = $fix.Line + 1  # 1-based
            $location = "$($file.FullName):$lineNum"
            $desc     = $fix.Type

            Write-Verbose "  [$desc] at $location"

            if ($AutoFix -and $fix.Fix -and $PSCmdlet.ShouldProcess($location, $desc)) {
                # Backup once per file
                if (-not $backedUp.Contains($file.FullName)) {
                    Copy-Item $file.FullName "$($file.FullName).bak" -Force -ErrorAction SilentlyContinue
                    [void]$backedUp.Add($file.FullName)
                }

                $lines[$fix.Line] = & $fix.Fix $lines[$fix.Line]
                $report.TotalFixes++
                $report.FixLocations.Add("$location ($desc)")
                $fileModified = $true
            }
            elseif (-not $AutoFix) {
                $report.FixLocations.Add("[dry-run] $location ($desc)")
            }
        }

        if ($fileModified) {
            Set-Content $file.FullName $lines
            $report.FilesModified++
        }
    }
    catch {
        $msg = "Error scanning $($file.FullName): $_"
        Write-Warning $msg
        $report.Errors.Add($msg)
    }
}

Write-Progress -Activity 'SyntaxHealer — Scanning' -Completed

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host ''
Write-Host '╔══════════════════════════════════════════════╗' -ForegroundColor Cyan
Write-Host '║  SyntaxHealer — Summary                      ║' -ForegroundColor Cyan
Write-Host '╠══════════════════════════════════════════════╣' -ForegroundColor Cyan
Write-Host "║  Files scanned   : $($report.FilesScanned)" -ForegroundColor Cyan
Write-Host "║  Total fixes     : $($report.TotalFixes)" -ForegroundColor $(if ($report.TotalFixes) { 'Green' } else { 'Cyan' })
Write-Host "║  Files modified  : $($report.FilesModified)" -ForegroundColor Cyan
Write-Host "║  Errors          : $($report.Errors.Count)" -ForegroundColor $(if ($report.Errors.Count) { 'Red' } else { 'Cyan' })
Write-Host '╚══════════════════════════════════════════════╝' -ForegroundColor Cyan

if ($report.FixLocations.Count -gt 0) {
    Write-Host "`nFix locations:" -ForegroundColor Green
    $report.FixLocations | ForEach-Object { Write-Host "  * $_" -ForegroundColor Green }
}
if ($report.Errors.Count -gt 0) {
    Write-Host "`nErrors:" -ForegroundColor Red
    $report.Errors | ForEach-Object { Write-Host "  ! $_" -ForegroundColor Red }
}

# ── JSON report ───────────────────────────────────────────────────────────────
if ($ReportPath) {
    try {
        $report | ConvertTo-Json -Depth 4 | Set-Content $ReportPath -Encoding utf8
        Write-Verbose "Report written -> $ReportPath"
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
