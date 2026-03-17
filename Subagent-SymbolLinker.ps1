# ============================================================================
# RawrXD Production Subagent: SymbolLinker
# Version: 1.0.0 | License: MIT
# Part of the RawrXD Autonomous Build System
# ============================================================================
#Requires -Version 7.0
<#
.SYNOPSIS
    Subagent: Symbol Linker — Resolves unresolved external symbols by generating stubs.

.DESCRIPTION
    Production-hardened subagent that parses MSVC linker error logs for
    LNK2001/LNK2019 unresolved external symbol errors. For each missing symbol,
    it generates a minimal C++ stub implementation that satisfies the linker.
    Supports backup, WhatIf, JSON output, and structured reporting.

.PARAMETER LogPath
    Path to the linker error log. Required.

.PARAMETER StubOutputDir
    Directory to write generated stub files. Default: .\src\generated_stubs

.PARAMETER AutoFix
    Automatically generate and write stub files.

.PARAMETER OutputFormat
    Text or JSON output.

.PARAMETER ReportPath
    Path to write a JSON report file.

.EXAMPLE
    .\Subagent-SymbolLinker.ps1 -LogPath .\linker_output.log -AutoFix -Verbose
    .\Subagent-SymbolLinker.ps1 -LogPath .\build.log -WhatIf
#>

[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'Medium')]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]$LogPath,

    [string]$StubOutputDir,

    [switch]$AutoFix,

    [ValidateSet('Text', 'JSON')]
    [string]$OutputFormat = 'Text',

    [string]$ReportPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $StubOutputDir) { $StubOutputDir = Join-Path $PSScriptRoot 'src' 'generated_stubs' }

# ── Symbol Parser ────────────────────────────────────────────────────────────

function Parse-UnresolvedSymbols {
    param([string]$Content)

    $symbols = [System.Collections.Generic.List[hashtable]]::new()
    $seen    = [System.Collections.Generic.HashSet[string]]::new()

    # LNK2001: unresolved external symbol "type __cdecl name(params)" (?mangled)
    # LNK2019: unresolved external symbol "..." referenced in ...
    $patterns = @(
        'unresolved external symbol\s+"(?<sig>[^"]+)"'
        'unresolved external symbol\s+(?<sig>\?\w[^\s]+)'
    )

    foreach ($pat in $patterns) {
        foreach ($m in [regex]::Matches($Content, $pat)) {
            $sig = $m.Groups['sig'].Value
            if ($seen.Contains($sig)) { continue }
            [void]$seen.Add($sig)

            $parsed = Parse-Signature $sig
            if ($parsed) {
                $symbols.Add($parsed)
            }
            else {
                # Store raw mangled symbol
                $symbols.Add(@{
                    ReturnType    = 'void'
                    Name          = $sig
                    Parameters    = ''
                    FullSignature = $sig
                    IsMangled     = $true
                })
            }
        }
    }

    return $symbols
}

function Parse-Signature {
    param([string]$Sig)

    # "void __cdecl FunctionName(class Type const &)"
    if ($Sig -match '^(?<ret>[\w\s\*&:]+?)\s+__(?:cdecl|stdcall|fastcall|thiscall|vectorcall)\s+(?<name>[\w:~]+)\s*\((?<params>[^)]*)\)') {
        return @{
            ReturnType    = $Matches['ret'].Trim()
            Name          = $Matches['name'].Trim()
            Parameters    = $Matches['params'].Trim()
            FullSignature = $Sig
            IsMangled     = $false
        }
    }

    # Simpler: "RetType Name(Params)"
    if ($Sig -match '^(?<ret>[\w\s\*&:]+?)\s+(?<name>[\w:~]+)\s*\((?<params>[^)]*)\)') {
        return @{
            ReturnType    = $Matches['ret'].Trim()
            Name          = $Matches['name'].Trim()
            Parameters    = $Matches['params'].Trim()
            FullSignature = $Sig
            IsMangled     = $false
        }
    }

    return $null
}

# ── Stub Generator ───────────────────────────────────────────────────────────

function Get-DefaultReturn {
    param([string]$Type)
    switch -Regex ($Type) {
        '^void$'                      { return '' }
        '^bool$|^BOOL$'              { return 'return false;' }
        '^int$|^long$|^LONG$|^DWORD$|^UINT$|^HRESULT$|^LRESULT$|^int32_t$|^int64_t$|^size_t$|^uint32_t$' {
            return 'return 0;'
        }
        '^float$|^double$'           { return 'return 0.0;' }
        '^char\s*\*$|^const\s+char\s*\*$|^LPCSTR$|^LPSTR$' { return 'return "";' }
        '^wchar_t\s*\*$|^const\s+wchar_t\s*\*$|^LPCWSTR$|^LPWSTR$' { return 'return L"";' }
        '\*$'                         { return 'return nullptr;' }
        '^std::string$'              { return 'return {};' }
        '^std::vector'               { return 'return {};' }
        default                       { return 'return {};' }
    }
}

function Format-StubParams {
    param([string]$Params)
    if (-not $Params -or $Params -eq 'void') { return '' }

    # Give anonymous params names
    $parts = $Params -split ','
    $named = @()
    $idx = 0
    foreach ($p in $parts) {
        $p = $p.Trim()
        $idx++
        # If param has no name (just a type), add one
        if ($p -match '^[\w\s\*&:<>]+$' -and $p -notmatch '\w+\s*$') {
            $named += "$p p$idx"
        }
        else {
            $named += $p
        }
    }
    return ($named -join ', ')
}

function Generate-StubFile {
    param(
        [System.Collections.Generic.List[hashtable]]$Symbols,
        [string]$OutputPath
    )

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine('// ============================================================================')
    [void]$sb.AppendLine('// Auto-generated linker stubs — RawrXD SymbolLinker Subagent')
    [void]$sb.AppendLine("// Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
    [void]$sb.AppendLine("// Symbols: $($Symbols.Count)")
    [void]$sb.AppendLine('// WARNING: These are minimal stubs to satisfy the linker.')
    [void]$sb.AppendLine('//          Replace with real implementations before shipping.')
    [void]$sb.AppendLine('// ============================================================================')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('#include <stdint.h>')
    [void]$sb.AppendLine('#include <string>')
    [void]$sb.AppendLine('#ifndef WIN32_LEAN_AND_MEAN')
    [void]$sb.AppendLine('#define WIN32_LEAN_AND_MEAN')
    [void]$sb.AppendLine('#endif')
    [void]$sb.AppendLine('#include <windows.h>')
    [void]$sb.AppendLine('')
    [void]$sb.AppendLine('#pragma warning(push)')
    [void]$sb.AppendLine('#pragma warning(disable: 4100)  // unreferenced formal parameter')
    [void]$sb.AppendLine('')

    foreach ($sym in $Symbols) {
        if ($sym.IsMangled) {
            [void]$sb.AppendLine("// MANGLED (could not demangle): $($sym.Name)")
            [void]$sb.AppendLine("// Link with the correct .obj/.lib to resolve this symbol.")
            [void]$sb.AppendLine('')
            continue
        }

        $ret    = $sym.ReturnType
        $name   = $sym.Name
        $params = Format-StubParams $sym.Parameters
        $defRet = Get-DefaultReturn $ret

        # Handle class::method
        $isMethod = $name -match '::'

        [void]$sb.AppendLine("// Stub: $($sym.FullSignature)")
        if ($isMethod) {
            [void]$sb.AppendLine("// NOTE: Class method stub — may need header include")
        }
        [void]$sb.AppendLine("$ret $name($params) {")
        [void]$sb.AppendLine("    // TODO: implement $name")
        if ($defRet) {
            [void]$sb.AppendLine("    $defRet")
        }
        [void]$sb.AppendLine('}')
        [void]$sb.AppendLine('')
    }

    [void]$sb.AppendLine('#pragma warning(pop)')

    if (-not (Test-Path (Split-Path $OutputPath))) {
        New-Item -ItemType Directory -Path (Split-Path $OutputPath) -Force | Out-Null
    }
    Set-Content $OutputPath $sb.ToString() -Encoding utf8
}

# ── Main ──────────────────────────────────────────────────────────────────────
Write-Verbose "[SymbolLinker] Parsing: $LogPath"

$logContent = Get-Content $LogPath -Raw -ErrorAction Stop

$report = [PSCustomObject]@{
    StubsGenerated    = 0
    SymbolsParsed     = 0
    MangledSymbols    = 0
    StubFile          = ''
    SymbolNames       = [System.Collections.Generic.List[string]]::new()
    Errors            = [System.Collections.Generic.List[string]]::new()
}

try {
    $symbols = Parse-UnresolvedSymbols -Content $logContent
    $report.SymbolsParsed = $symbols.Count
    $report.MangledSymbols = ($symbols | Where-Object { $_.IsMangled }).Count

    foreach ($s in $symbols) {
        $report.SymbolNames.Add($s.Name)
    }

    $demangledSymbols = [System.Collections.Generic.List[hashtable]]::new()
    foreach ($s in $symbols) {
        if (-not $s.IsMangled) { $demangledSymbols.Add($s) }
    }

    Write-Host "  Parsed $($symbols.Count) unresolved symbols ($($demangledSymbols.Count) demangled, $($report.MangledSymbols) mangled)"

    if ($demangledSymbols.Count -gt 0) {
        $stubFile = Join-Path $StubOutputDir "linker_stubs_$(Get-Date -Format 'yyyyMMdd_HHmmss').cpp"
        $report.StubFile = $stubFile

        if ($AutoFix -and $PSCmdlet.ShouldProcess($stubFile, "Generate $($demangledSymbols.Count) stub implementations")) {
            Generate-StubFile -Symbols $demangledSymbols -OutputPath $stubFile
            $report.StubsGenerated = $demangledSymbols.Count
            Write-Verbose "  Stubs written → $stubFile"
        }
        elseif (-not $AutoFix) {
            Write-Verbose "  [dry-run] Would generate $($demangledSymbols.Count) stubs to $stubFile"
        }
    }
}
catch {
    $msg = "Error parsing symbols: $_"
    Write-Warning $msg
    $report.Errors.Add($msg)
}

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host ''
Write-Host '╔══════════════════════════════════════════════╗' -ForegroundColor Cyan
Write-Host '║  SymbolLinker — Summary                      ║' -ForegroundColor Cyan
Write-Host '╠══════════════════════════════════════════════╣' -ForegroundColor Cyan
Write-Host "║  Symbols parsed  : $($report.SymbolsParsed)" -ForegroundColor Cyan
Write-Host "║  Stubs generated : $($report.StubsGenerated)" -ForegroundColor $(if ($report.StubsGenerated) { 'Green' } else { 'Cyan' })
Write-Host "║  Mangled (skip)  : $($report.MangledSymbols)" -ForegroundColor $(if ($report.MangledSymbols) { 'Yellow' } else { 'Cyan' })
Write-Host "║  Errors          : $($report.Errors.Count)" -ForegroundColor $(if ($report.Errors.Count) { 'Red' } else { 'Cyan' })
Write-Host '╚══════════════════════════════════════════════╝' -ForegroundColor Cyan

if ($report.SymbolNames.Count -gt 0 -and $report.SymbolNames.Count -le 30) {
    Write-Host "`nUnresolved symbols:" -ForegroundColor Yellow
    $report.SymbolNames | ForEach-Object { Write-Host "  ~ $_" -ForegroundColor Yellow }
}
elseif ($report.SymbolNames.Count -gt 30) {
    Write-Host "`nFirst 30 unresolved symbols (of $($report.SymbolNames.Count)):" -ForegroundColor Yellow
    $report.SymbolNames | Select-Object -First 30 | ForEach-Object { Write-Host "  ~ $_" -ForegroundColor Yellow }
}

if ($report.StubFile -and $report.StubsGenerated -gt 0) {
    Write-Host "`nStub file: $($report.StubFile)" -ForegroundColor Green
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
