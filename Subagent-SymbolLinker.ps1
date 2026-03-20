# ============================================================================
# RawrXD Production Subagent: SymbolLinker
# Version: 2.0.0 | License: MIT
# Part of the RawrXD Autonomous Build System
# ============================================================================
#Requires -Version 7.0
<#
.SYNOPSIS
    Subagent: SymbolLinker - production unresolved-symbol auditor.

.DESCRIPTION
    Parses linker logs (MSVC + GCC/Clang forms), finds unresolved symbols, and
    classifies each symbol against source providers and CMake wiring:
      - UNLINKED (no provider found)
      - STUB_ONLY (only stub/fallback providers found)
      - COMMENTED_OUT_IN_CMAKE (real providers exist but are commented out)
      - UNWIRED_IN_CMAKE (real providers exist but are absent from CMake)
      - REAL_PROVIDER_PRESENT (real provider exists and is wired)

    This subagent no longer generates stubs in production mode.

.PARAMETER LogPath
    Path to linker/build log. Required.

.PARAMETER CMakePath
    Path to CMakeLists.txt. Defaults to repo-root CMakeLists.txt.

.PARAMETER SourceRoot
    Source tree root to scan for symbol providers. Defaults to .\src.

.PARAMETER AutoFix
    Retained for orchestrator compatibility. In production mode this subagent
    performs analysis only and does not write stub code.

.PARAMETER OutputFormat
    Text (default) or JSON.

.PARAMETER ReportPath
    Optional path to write JSON report.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]$LogPath,

    [string]$CMakePath,
    [string]$SourceRoot,

    [switch]$AutoFix,

    [ValidateSet('Text', 'JSON')]
    [string]$OutputFormat = 'Text',

    [string]$ReportPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $CMakePath) { $CMakePath = Join-Path $PSScriptRoot 'CMakeLists.txt' }
if (-not $SourceRoot) { $SourceRoot = Join-Path $PSScriptRoot 'src' }

function Get-SymbolName {
    param([string]$Signature)

    # Typical demangled form:
    # "struct Foo __cdecl handleBar(class Ctx const &)"
    if ($Signature -match '__(?:cdecl|stdcall|fastcall|thiscall|vectorcall)\s+(?<name>[A-Za-z_~][\w:~]*)\s*\(') {
        return $Matches['name']
    }

    # Generic function-like symbol:
    # "Foo::Bar(Baz)"
    if ($Signature -match '(?<name>[A-Za-z_~][\w:~]*)\s*\(') {
        return $Matches['name']
    }

    # Best effort for mangled MSVC symbols
    if ($Signature -match '^\?(?<name>[A-Za-z_]\w+)@@') {
        return $Matches['name']
    }

    return $Signature
}

function Parse-UnresolvedSymbols {
    param([string]$Content)

    $records = [System.Collections.Generic.List[object]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new()

    $patterns = @(
        'unresolved external symbol\s+"(?<sig>[^"]+)"'
        'unresolved external symbol\s+(?<sig>\?\S+)'
        "undefined reference to [`'\""](?<sig>[^`'\""]+)[`'\""]"
        'undefined reference to (?<sig>[A-Za-z_~:\?\.\$][^\s,;]+)'
    )

    foreach ($pat in $patterns) {
        foreach ($m in [regex]::Matches($Content, $pat, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
            $sig = $m.Groups['sig'].Value.Trim()
            if (-not $sig) { continue }
            if ($seen.Contains($sig)) { continue }
            [void]$seen.Add($sig)
            $records.Add([PSCustomObject]@{
                Signature = $sig
                Name      = Get-SymbolName -Signature $sig
            })
        }
    }

    return $records
}

function Convert-ToRepoRelativePath {
    param(
        [string]$PathValue,
        [string]$RepoRoot
    )
    $full = [System.IO.Path]::GetFullPath($PathValue)
    $root = [System.IO.Path]::GetFullPath($RepoRoot)
    if ($full.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        $rel = $full.Substring($root.Length).TrimStart('\','/')
    } else {
        $rel = $PathValue
    }
    return ($rel -replace '\\','/')
}

function Test-IsStubLikePath {
    param([string]$RepoRelativePath)
    return ($RepoRelativePath -match '(?i)(^|/)(stubs?\.cpp|.*_stubs?\.(cpp|asm)|.*_stub\.(cpp|asm)|.*_fallbacks?\.cpp|.*headless.*\.cpp|.*shim.*\.cpp|.*mock.*\.cpp|.*fake.*\.cpp|.*missing_handler.*\.cpp)$')
}

function Get-CMakeWireStatus {
    param(
        [string[]]$CMakeLines,
        [string]$RepoRelativePath
    )
    $escaped = [regex]::Escape($RepoRelativePath)
    $commented = $false
    $wired = $false

    foreach ($line in $CMakeLines) {
        if ($line -match "^\s*#.*$escaped") {
            $commented = $true
            continue
        }
        if ($line -match "^\s*[^#].*$escaped") {
            $wired = $true
        }
    }

    if ($wired) { return 'wired' }
    if ($commented) { return 'commented' }
    return 'unwired'
}

function Get-DissolvedCMakeEntries {
    param([string[]]$CMakeLines)
    $out = [System.Collections.Generic.List[object]]::new()
    foreach ($line in $CMakeLines) {
        if ($line -match '^\s*#\s*(?<path>src/[^\s]+)\s+#\s*excluded:.*(undefined|unresolved|missing)') {
            $out.Add([PSCustomObject]@{
                Path   = $Matches['path']
                Reason = $line.Trim()
            })
        }
    }
    return $out
}

if ($AutoFix) {
    Write-Warning "[SymbolLinker] AutoFix requested, but production mode is analysis-only (no stub generation)."
}

$report = [PSCustomObject]@{
    SymbolsParsed           = 0
    UnlinkedCount           = 0
    StubOnlyCount           = 0
    CommentedOutCount       = 0
    UnwiredCount            = 0
    RealProviderPresent     = 0
    DissolvedCMakeEntries   = 0
    StubsGenerated          = 0   # backward compatibility with orchestrator metric key
    Findings                = [System.Collections.Generic.List[object]]::new()
    DissolvedSources        = [System.Collections.Generic.List[object]]::new()
    Errors                  = [System.Collections.Generic.List[string]]::new()
}

try {
    if (-not (Test-Path $CMakePath -PathType Leaf)) {
        throw "CMake file not found: $CMakePath"
    }
    if (-not (Test-Path $SourceRoot -PathType Container)) {
        throw "Source root not found: $SourceRoot"
    }

    $repoRoot = $PSScriptRoot
    $logContent = Get-Content $LogPath -Raw
    $cmakeLines = Get-Content $CMakePath

    $symbols = Parse-UnresolvedSymbols -Content $logContent
    $report.SymbolsParsed = $symbols.Count

    $sourceFiles = Get-ChildItem $SourceRoot -Recurse -File -Include *.cpp,*.cc,*.cxx,*.c,*.h,*.hpp,*.asm -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '(?i)[\\/](build|bin|obj|out|history|runoff)[\\/]' } |
        Select-Object -ExpandProperty FullName

    $dissolved = Get-DissolvedCMakeEntries -CMakeLines $cmakeLines
    $report.DissolvedCMakeEntries = $dissolved.Count
    foreach ($d in $dissolved) { $report.DissolvedSources.Add($d) }

    foreach ($sym in $symbols) {
        $name = $sym.Name
        $sig = $sym.Signature
        $escapedName = [regex]::Escape($name)

        $providerMatches = @()
        if ($escapedName -and $escapedName.Length -gt 1) {
            $providerMatches = Select-String -Path $sourceFiles -Pattern "\b$escapedName\s*\(" -CaseSensitive -ErrorAction SilentlyContinue
        }
        $providerFiles = @($providerMatches | Select-Object -ExpandProperty Path -Unique)

        if ($providerFiles.Count -eq 0) {
            $report.UnlinkedCount++
            $report.Findings.Add([PSCustomObject]@{
                Symbol      = $name
                Signature   = $sig
                Status      = 'UNLINKED'
                Providers   = @()
                RealWired   = @()
                RealCommented = @()
                RealUnwired = @()
            })
            continue
        }

        $providerRecords = [System.Collections.Generic.List[object]]::new()
        foreach ($pf in $providerFiles) {
            $rel = Convert-ToRepoRelativePath -PathValue $pf -RepoRoot $repoRoot
            $wire = Get-CMakeWireStatus -CMakeLines $cmakeLines -RepoRelativePath $rel
            $stubLike = Test-IsStubLikePath -RepoRelativePath $rel
            $providerRecords.Add([PSCustomObject]@{
                Path     = $rel
                Wire     = $wire
                StubLike = $stubLike
            })
        }

        $realProviders = @($providerRecords | Where-Object { -not $_.StubLike })
        $realWired = @($realProviders | Where-Object { $_.Wire -eq 'wired' } | Select-Object -ExpandProperty Path)
        $realCommented = @($realProviders | Where-Object { $_.Wire -eq 'commented' } | Select-Object -ExpandProperty Path)
        $realUnwired = @($realProviders | Where-Object { $_.Wire -eq 'unwired' } | Select-Object -ExpandProperty Path)

        $status = 'REAL_PROVIDER_PRESENT'
        if ($realProviders.Count -eq 0) {
            $status = 'STUB_ONLY'
            $report.StubOnlyCount++
        } elseif ($realWired.Count -eq 0 -and $realCommented.Count -gt 0) {
            $status = 'COMMENTED_OUT_IN_CMAKE'
            $report.CommentedOutCount++
        } elseif ($realWired.Count -eq 0 -and $realUnwired.Count -gt 0) {
            $status = 'UNWIRED_IN_CMAKE'
            $report.UnwiredCount++
        } else {
            $report.RealProviderPresent++
        }

        $report.Findings.Add([PSCustomObject]@{
            Symbol        = $name
            Signature     = $sig
            Status        = $status
            Providers     = @($providerRecords | Select-Object -ExpandProperty Path)
            RealWired     = $realWired
            RealCommented = $realCommented
            RealUnwired   = $realUnwired
        })
    }
}
catch {
    $report.Errors.Add($_.ToString())
}

Write-Host ''
Write-Host '╔══════════════════════════════════════════════════════════════╗' -ForegroundColor Cyan
Write-Host '║   SymbolLinker v2 — Production Symbol Audit                 ║' -ForegroundColor Cyan
Write-Host '╠══════════════════════════════════════════════════════════════╣' -ForegroundColor Cyan
Write-Host "║  Symbols parsed             : $($report.SymbolsParsed)" -ForegroundColor Cyan
Write-Host "║  UNLINKED                   : $($report.UnlinkedCount)" -ForegroundColor $(if ($report.UnlinkedCount) { 'Yellow' } else { 'Green' })
Write-Host "║  STUB_ONLY                  : $($report.StubOnlyCount)" -ForegroundColor $(if ($report.StubOnlyCount) { 'Yellow' } else { 'Green' })
Write-Host "║  COMMENTED_OUT_IN_CMAKE     : $($report.CommentedOutCount)" -ForegroundColor $(if ($report.CommentedOutCount) { 'Yellow' } else { 'Green' })
Write-Host "║  UNWIRED_IN_CMAKE           : $($report.UnwiredCount)" -ForegroundColor $(if ($report.UnwiredCount) { 'Yellow' } else { 'Green' })
Write-Host "║  REAL_PROVIDER_PRESENT      : $($report.RealProviderPresent)" -ForegroundColor Green
Write-Host "║  Dissolved CMake exclusions : $($report.DissolvedCMakeEntries)" -ForegroundColor $(if ($report.DissolvedCMakeEntries) { 'Yellow' } else { 'Green' })
Write-Host "║  Errors                     : $($report.Errors.Count)" -ForegroundColor $(if ($report.Errors.Count) { 'Red' } else { 'Green' })
Write-Host '╚══════════════════════════════════════════════════════════════╝' -ForegroundColor Cyan

if ($report.Findings.Count -gt 0) {
    $critical = @($report.Findings | Where-Object { $_.Status -ne 'REAL_PROVIDER_PRESENT' })
    if ($critical.Count -gt 0) {
        Write-Host "`nCritical symbol findings (first 25):" -ForegroundColor Yellow
        $critical | Select-Object -First 25 | ForEach-Object {
            Write-Host ("  [{0}] {1}" -f $_.Status, $_.Symbol) -ForegroundColor Yellow
        }
    }
}

if ($ReportPath) {
    try {
        $report | ConvertTo-Json -Depth 6 | Set-Content $ReportPath -Encoding utf8
    }
    catch {
        Write-Warning "Could not write report to $ReportPath : $_"
    }
}

if ($OutputFormat -eq 'JSON') {
    $report | ConvertTo-Json -Depth 6
} else {
    Write-Output $report
}

if ($report.Errors.Count -gt 0) { exit 1 }
exit 0
