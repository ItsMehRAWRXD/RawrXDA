# =============================================================================
# stub_audit.ps1 — Stub Purge Audit for RawrXD IDE
# =============================================================================
# Usage:
#   .\stub_audit.ps1                      # auto-detect build dir from CMakeCache
#   .\stub_audit.ps1 -BuildDir G:\build   # explicit build dir
#   .\stub_audit.ps1 -LinkRsp d:\rawrxd\src\link.rsp  # explicit linker response file
#
# What it does:
#   1. Finds the MSVC linker (link.exe) via VS Dev tools or PATH
#   2. Re-runs the last link with /VERBOSE:UNRESOLVED, capturing output
#   3. Parses "unresolved external symbol" lines → set of required symbols
#   4. Scans known stub files for which symbols they provide
#   5. Outputs three lists:
#       LIVE_STUBS   — stubs whose symbols ARE in the unresolved set (keep/implement)
#       DEAD_STUBS   — stubs whose symbols are NOT needed (safe to delete)
#       ORPHAN_STUBS — stub files with no exported symbols detected (delete candidates)
# =============================================================================

param(
    [string]$BuildDir    = "",
    [string]$LinkRsp     = "",
    [string]$OutputFile  = "d:\rawrxd\src\stub_audit_report.txt",
    [string]$BuildLog    = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# 1. Locate build directory
# ---------------------------------------------------------------------------
if (-not $BuildDir) {
    $candidates = @("D:\rawrxd\build_prod", "G:\build", "d:\rawrxd\build", "d:\rawrxd\src\build")
    foreach ($c in $candidates) {
        if (Test-Path (Join-Path $c "CMakeCache.txt")) { $BuildDir = $c; break }
    }
}
if (-not $BuildDir) {
    Write-Warning "Could not auto-detect build dir. Pass -BuildDir <path>."
    $BuildDir = "D:\rawrxd\build_prod"
}

Write-Host "Build dir: $BuildDir"

# Expected map artifact (set by CMakeLists.txt MAP block)
$expectedMap = Join-Path $BuildDir "bin\RawrXD-Win32IDE.map"
if (Test-Path $expectedMap) {
    Write-Host "[MAP] Found: $expectedMap ($('{0:N0}' -f (Get-Item $expectedMap).Length) bytes)"
} else {
    Write-Warning "[MAP] Not found at $expectedMap — build RawrXD-Win32IDE target first."
}

# ---------------------------------------------------------------------------
# 2. Find MSVC link.exe
# ---------------------------------------------------------------------------
$link = Get-Command "link.exe" -ErrorAction SilentlyContinue
if (-not $link) {
    # Try VS 2022 default install
    $vsBin = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
    if (Test-Path $vsBin) { $link = $vsBin } else {
        # Try vswhere
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            $vsPath = & $vswhere -latest -property installationPath 2>$null
            $msvcVer = Get-ChildItem "$vsPath\VC\Tools\MSVC" -Name | Sort-Object -Descending | Select-Object -First 1
            $link = "$vsPath\VC\Tools\MSVC\$msvcVer\bin\Hostx64\x64\link.exe"
        }
    }
}

if (-not $link) {
    Write-Error "link.exe not found. Add MSVC bin to PATH or use 'Developer Command Prompt'."
}
Write-Host "Linker: $link"

# ---------------------------------------------------------------------------
# 3. Find or reconstruct the linker response file
# ---------------------------------------------------------------------------
if (-not $LinkRsp) {
    # CMake generates per-target .rsp files in the build tree
    $rspFiles = Get-ChildItem $BuildDir -Recurse -Filter "*.rsp" -ErrorAction SilentlyContinue |
                Where-Object { $_.Length -gt 1KB } |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 1
    if ($rspFiles) {
        $LinkRsp = $rspFiles.FullName
    }
    # Fallback: use the hand-maintained link.rsp in src/
    if (-not $LinkRsp -and (Test-Path "d:\rawrxd\src\link.rsp")) {
        $LinkRsp = "d:\rawrxd\src\link.rsp"
    }
}

$verboseLog = Join-Path $BuildDir "link_verbose_unresolved.txt"
Write-Host "Response file: $LinkRsp"
Write-Host "Verbose log  : $verboseLog"

# ---------------------------------------------------------------------------
# 4. Re-link with /VERBOSE (MSVC does NOT support /VERBOSE:UNRESOLVED)
# ---------------------------------------------------------------------------
if ($LinkRsp -and (Test-Path $LinkRsp)) {
    Write-Host "`nRunning linker pass with /VERBOSE ..."
    $linkOutput = & "$link" "@$LinkRsp" /VERBOSE 2>&1
    $linkOutput | Out-File -FilePath $verboseLog -Encoding utf8
    Write-Host "Verbose output written to: $verboseLog"
} else {
    Write-Warning "No .rsp file found. Skipping live link pass."
    Write-Host "  → To get accurate results, run from a CMake build directory with a recent build."
    $verboseLog = $null
}

# ---------------------------------------------------------------------------
# 5. Parse unresolved symbols from linker log
# ---------------------------------------------------------------------------
$unresolvedSymbols = [System.Collections.Generic.HashSet[string]]::new()

if ($verboseLog -and (Test-Path $verboseLog)) {
    $logText = Get-Content $verboseLog -Raw

    # Handles both LNK2001 and LNK2019 forms:
    #   error LNK2001: unresolved external symbol <sym>
    #   error LNK2019: unresolved external symbol <sym> referenced in ...
    $rx = [regex]'unresolved external symbol\s+(.+?)(?:\s+referenced\s+in|\r?\n)'
    foreach ($m in $rx.Matches($logText)) {
        $sym = $m.Groups[1].Value.Trim().Trim('"').TrimStart('_')
        if ($sym) { [void]$unresolvedSymbols.Add($sym) }
    }

    Write-Host "`nUnresolved symbols found: $($unresolvedSymbols.Count)"
}

# ---------------------------------------------------------------------------
# 6. Catalogue stub files (dynamic discovery)
# ---------------------------------------------------------------------------
# Previous hard-coded inventory drifted as files moved/renamed.
# Use recursive discovery so audits stay accurate across branches.
$stubFiles = @(Get-ChildItem "d:\rawrxd\src" -Recurse -File `
    -Include "*stub*.cpp", "*stubs*.cpp", "*stub*.hpp" `
    -ErrorAction SilentlyContinue |
    Where-Object {
        # Exclude generated/build/vendor mirrors if they appear under src
        $_.FullName -notmatch "\\(build|out|obj|bin|x64|x86)\\" -and
        $_.FullName -notmatch "\\(ggml|third_party|external|vendor)\\"
    } |
    Select-Object -ExpandProperty FullName -Unique)

Write-Host "Stub files catalogued: $($stubFiles.Count)"

# ---------------------------------------------------------------------------
# 7. Extract symbols provided by each stub file
# ---------------------------------------------------------------------------
# We look for function definitions: return-type funcname( pattern
$funcDefRegex = [regex]'(?m)^\s*(?:void|int|bool|HRESULT|HWND|HANDLE|size_t|std::\w+|[A-Za-z_]\w*\s+\*?)\s+([A-Za-z_]\w*)\s*\('

$liveStubs  = @()
$deadStubs  = @()
$orphanStubs = @()

foreach ($sf in $stubFiles) {
    $content  = Get-Content $sf -Raw -ErrorAction SilentlyContinue
    if (-not $content) { $orphanStubs += $sf; continue }

    $matches2 = $funcDefRegex.Matches($content)
    $provided = @($matches2 | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)

    if (@($provided).Count -eq 0) {
        $orphanStubs += $sf
        continue
    }

    if ($unresolvedSymbols.Count -gt 0) {
        # Check if any provided symbol is needed
        $needed = @($provided | Where-Object { $unresolvedSymbols.Contains($_) })
        if (@($needed).Count -gt 0) {
            $liveStubs  += [PSCustomObject]@{ File = $sf; Symbols = $needed -join ", " }
        } else {
            $deadStubs  += [PSCustomObject]@{ File = $sf; Symbols = $provided -join ", " }
        }
    } else {
        # No unresolved symbols in this link pass:
        # classify as dead-candidate (manual check still recommended before git rm).
        $deadStubs += [PSCustomObject]@{ File = $sf; Symbols = $provided -join ", " }
    }
}

# ---------------------------------------------------------------------------
# 8. Emit report
# ---------------------------------------------------------------------------
$report = [System.Text.StringBuilder]::new()
$ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

$null = $report.AppendLine("# RawrXD Stub Purge Audit Report")
$null = $report.AppendLine("Generated: $ts")
$null = $report.AppendLine("Build dir: $BuildDir")
$null = $report.AppendLine("")
$null = $report.AppendLine("## Summary")
$null = $report.AppendLine("  Unresolved symbols : $($unresolvedSymbols.Count)")
$null = $report.AppendLine("  Stub files scanned : $($stubFiles.Count)")
$null = $report.AppendLine("  LIVE (keep/impl)   : $($liveStubs.Count)")
$null = $report.AppendLine("  DEAD (safe delete) : $($deadStubs.Count)")
$null = $report.AppendLine("  ORPHAN (no symbols): $($orphanStubs.Count)")
$null = $report.AppendLine("")

if ($liveStubs.Count -gt 0) {
    $null = $report.AppendLine("## LIVE STUBS — implement these real functions")
    foreach ($s in $liveStubs) {
        $null = $report.AppendLine("  $($s.File)")
        $null = $report.AppendLine("    Required: $($s.Symbols)")
    }
    $null = $report.AppendLine("")
}

if ($deadStubs.Count -gt 0) {
    $null = $report.AppendLine("## DEAD STUBS — safe to git rm")
    foreach ($s in $deadStubs) {
        $null = $report.AppendLine("  $($s.File)")
    }
    $null = $report.AppendLine("")
    $null = $report.AppendLine("  git rm command:")
    $gitRm = ($deadStubs | ForEach-Object { "`"$($_.File -replace 'd:\\rawrxd\\', '')`"" }) -join " "
    $null = $report.AppendLine("    git rm -- $gitRm")
    $null = $report.AppendLine("")
}

if ($orphanStubs.Count -gt 0) {
    $null = $report.AppendLine("## ORPHAN STUBS — no parseable symbols (inspect manually)")
    foreach ($s in $orphanStubs) {
        $null = $report.AppendLine("  $s")
    }
    $null = $report.AppendLine("")
}

if ($unresolvedSymbols.Count -gt 0) {
    $null = $report.AppendLine("## ALL UNRESOLVED SYMBOLS (from /VERBOSE:UNRESOLVED)")
    foreach ($sym in ($unresolvedSymbols | Sort-Object)) {
        $null = $report.AppendLine("  $sym")
    }
}

$report.ToString() | Out-File -FilePath $OutputFile -Encoding utf8
Write-Host "`nReport written: $OutputFile"
Write-Host "`n=== SUMMARY ==="
Write-Host "  LIVE  (implement) : $($liveStubs.Count)"
Write-Host "  DEAD  (delete)    : $($deadStubs.Count)"
Write-Host "  ORPHAN (inspect)  : $($orphanStubs.Count)"

if ($deadStubs.Count -gt 0) {
    Write-Host "`nDead stub files eligible for git rm:"
    $deadStubs | ForEach-Object { Write-Host "  $($_.File)" }
}

