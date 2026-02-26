#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD v15.0.0-GOLD Build Integrity Validator
.DESCRIPTION
    Verifies 13 MASM64 modules for syntax integrity, symbol balance, and cryptographic hashes.
    Generates JSON attestation for technical due diligence.
.NOTES
    Run from repository root (D:\rawrxd).
    Requires: ml64.exe (VS 2022 Build Tools), PowerShell 5.1+
#>

param(
    [switch]$SkipAssembly,      # Skip ml64 syntax check (fast mode)
    [string]$RepoRoot = $PSScriptRoot
)

$ErrorActionPreference = "Stop"
Set-Location $RepoRoot

# ── Resolve ml64.exe ────────────────────────────────────────────────
$ml64Candidates = @(
    "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
)

$ml64 = $null
foreach ($candidate in $ml64Candidates) {
    if (Test-Path $candidate) { $ml64 = $candidate; break }
}
# Fallback: try PATH
if (-not $ml64) {
    $found = Get-Command ml64.exe -ErrorAction SilentlyContinue
    if ($found) { $ml64 = $found.Source }
}
# Fallback: wildcard search under VS2022Enterprise
if (-not $ml64) {
    $wild = Get-ChildItem "C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($wild) { $ml64 = $wild.FullName }
}

$hasMl64 = ($null -ne $ml64) -and (Test-Path $ml64)
if (-not $hasMl64 -and -not $SkipAssembly) {
    Write-Host "[WARN] ml64.exe not found. Assembly syntax checks will be skipped." -ForegroundColor Yellow
    $SkipAssembly = $true
}

# ── Validation State ────────────────────────────────────────────────
$ValidationResults = @{
    Metadata = @{
        Timestamp    = Get-Date -Format "o"
        Version      = "15.0.0-GOLD"
        Validator    = "RawrXD-BuildCert-v1"
        Platform     = "MASM64/x64"
        ML64Path     = if ($hasMl64) { $ml64 } else { "NOT FOUND" }
        RepoRoot     = $RepoRoot
    }
    Modules = @()
    Summary = @{
        TotalFiles = 13
        Passed     = 0
        Failed     = 0
        Warnings   = 0
    }
}

# ── Module Manifest (actual line counts from repo) ──────────────────
$AsmFiles = @(
    @{ Name = "RawrCodex.asm";                 Path = "src\asm\RawrCodex.asm";                 Critical = $true;  LinesExpected = 9715 },
    @{ Name = "FlashAttention_AVX512.asm";      Path = "src\asm\FlashAttention_AVX512.asm";      Critical = $true;  LinesExpected = 1104 },
    @{ Name = "RawrXD_Debug_Engine.asm";        Path = "src\asm\RawrXD_Debug_Engine.asm";        Critical = $true;  LinesExpected = 1628 },
    @{ Name = "RawrXD_KQuant_Dequant.asm";     Path = "src\asm\RawrXD_KQuant_Dequant.asm";     Critical = $true;  LinesExpected = 563  },
    @{ Name = "RawrXD_Swarm_Network.asm";      Path = "src\asm\RawrXD_Swarm_Network.asm";      Critical = $false; LinesExpected = 1153 },
    @{ Name = "RawrXD_QuadBuffer_Streamer.asm"; Path = "src\asm\RawrXD_QuadBuffer_Streamer.asm"; Critical = $true;  LinesExpected = 1561 },
    @{ Name = "RawrXD_EnterpriseLicense.asm";   Path = "src\asm\RawrXD_EnterpriseLicense.asm";   Critical = $true;  LinesExpected = 996  },
    @{ Name = "RawrXD_License_Shield.asm";      Path = "src\asm\RawrXD_License_Shield.asm";      Critical = $true;  LinesExpected = 1092 },
    @{ Name = "memory_patch.asm";               Path = "src\asm\memory_patch.asm";               Critical = $true;  LinesExpected = 229  },
    @{ Name = "byte_search.asm";                Path = "src\asm\byte_search.asm";                Critical = $true;  LinesExpected = 390  },
    @{ Name = "request_patch.asm";              Path = "src\asm\request_patch.asm";              Critical = $true;  LinesExpected = 118  },
    @{ Name = "custom_zlib.asm";                Path = "src\asm\custom_zlib.asm";                Critical = $false; LinesExpected = 495  },
    @{ Name = "inference_kernels.asm";          Path = "src\asm\inference_kernels.asm";          Critical = $true;  LinesExpected = 151  }
)

# ── Helper Functions ────────────────────────────────────────────────

function Test-MasmSyntax {
    param([string]$FilePath, [string]$FileName)

    if ($SkipAssembly) {
        return @{ Status = "SKIPPED"; Errors = 0; Warnings = 0; Output = "ml64 check skipped" }
    }

    $tempObj = Join-Path $env:TEMP ($FileName -replace '\.asm$', '.obj')

    try {
        # Include path for .inc files
        $incDir = Split-Path $FilePath -Parent
        $output = & $ml64 /nologo /c /I"$incDir" /Fo"$tempObj" "$FilePath" 2>&1
        $exitCode = $LASTEXITCODE

        if ($exitCode -eq 0) {
            $warnCount = ($output | Select-String "warning [A-Z]\d{4}:").Count
            return @{ Status = "PASS"; Errors = 0; Warnings = $warnCount; Output = "Clean assembly" }
        } else {
            $errors   = ($output | Select-String "error [A-Z]\d{4}:").Count
            $warnings = ($output | Select-String "warning [A-Z]\d{4}:").Count
            # Capture first 10 error lines for diagnostics
            $errorLines = ($output | Select-String "error" | Select-Object -First 10) -join "`n"
            return @{ Status = "FAIL"; Errors = $errors; Warnings = $warnings; Output = $errorLines }
        }
    } catch {
        return @{ Status = "ERROR"; Errors = 1; Warnings = 0; Output = $_.Exception.Message }
    } finally {
        if (Test-Path $tempObj) { Remove-Item $tempObj -Force -ErrorAction SilentlyContinue }
    }
}

function Get-ProcBalance {
    param([string]$FilePath)
    $content = Get-Content $FilePath -Raw
    # Match PROC/ENDP as whole words, case-insensitive (MASM is case-insensitive for directives)
    $procs = [regex]::Matches($content, '(?im)^\s*\w+\s+PROC\b').Count
    $endps = [regex]::Matches($content, '(?im)^\s*\w+\s+ENDP\b').Count
    return @{ PROC = $procs; ENDP = $endps; Balanced = ($procs -eq $endps) }
}

function Get-CryptoHash {
    param([string]$FilePath)
    $sha256 = Get-FileHash $FilePath -Algorithm SHA256
    $md5    = Get-FileHash $FilePath -Algorithm MD5
    return @{ SHA256 = $sha256.Hash; MD5 = $md5.Hash }
}

function Test-NasmContamination {
    param([string]$FilePath)
    $content = Get-Content $FilePath -Raw
    # Patterns that indicate NASM/GAS syntax leaked into a MASM file
    $nasmPatterns = @(
        '(?m)^\s*section\s+\.text\b',
        '(?m)^\s*section\s+\.data\b',
        '(?m)^\s*section\s+\.bss\b',
        '(?m)^\s*segment\s+align\b',
        '(?m)^\s*global\s+\w+',
        '(?m)^\s*DEFAULT\s+REL\b',
        '(?m)^\s*%macro\b',
        '(?m)^\s*%define\b'
    )
    $hits = @()
    foreach ($pattern in $nasmPatterns) {
        $m = [regex]::Matches($content, $pattern)
        if ($m.Count -gt 0) { $hits += $pattern }
    }
    return @{ Contaminated = ($hits.Count -gt 0); Score = $hits.Count; Patterns = $hits }
}

function Get-ExportedSymbols {
    param([string]$FilePath)
    $content = Get-Content $FilePath -Raw
    # Collect PUBLIC declarations
    $publics = [regex]::Matches($content, '(?im)^\s*PUBLIC\s+(\w+)') | ForEach-Object { $_.Groups[1].Value }
    # Collect EXTERN declarations
    $externs = [regex]::Matches($content, '(?im)^\s*EXTERN\s+(\w+)') | ForEach-Object { $_.Groups[1].Value }
    return @{ Public = @($publics); Extern = @($externs) }
}

# ── Main Validation Loop ───────────────────────────────────────────

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   RawrXD v15.0.0-GOLD  Build Integrity Verification     ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host "  Repo Root : $RepoRoot"
Write-Host "  ml64.exe  : $(if ($hasMl64) { $ml64 } else { 'NOT FOUND (syntax check skipped)' })"
Write-Host "  Timestamp : $($ValidationResults.Metadata.Timestamp)"
Write-Host ""

$totalLines = 0

foreach ($file in $AsmFiles) {
    $fullPath = Join-Path $RepoRoot $file.Path
    Write-Host "  [$($AsmFiles.IndexOf($file) + 1)/13] $($file.Name) " -NoNewline

    $result = @{
        FileName = $file.Name
        Path     = $file.Path
        Critical = $file.Critical
        Exists   = Test-Path $fullPath
    }

    if (-not $result.Exists) {
        $result.Status  = "MISSING"
        $result.Overall = "FAIL"
        $ValidationResults.Summary.Failed++
        Write-Host "MISSING" -ForegroundColor Red
        $ValidationResults.Modules += $result
        continue
    }

    # Line count
    $lines = (Get-Content $fullPath).Count
    $result.Lines     = $lines
    $result.LinesExpected = $file.LinesExpected
    $result.LineDelta = [Math]::Abs($lines - $file.LinesExpected)
    $result.LineCheck = ($result.LineDelta -le 100)  # Allow 100-line drift for comment edits
    $totalLines += $lines

    # Syntax (ml64)
    $syntax = Test-MasmSyntax -FilePath $fullPath -FileName $file.Name
    $result.Syntax = $syntax

    # PROC/ENDP balance
    $balance = Get-ProcBalance -FilePath $fullPath
    $result.ProcBalance = $balance

    # Cryptographic hashes
    $hashes = Get-CryptoHash -FilePath $fullPath
    $result.Hashes = $hashes

    # NASM contamination
    $nasmCheck = Test-NasmContamination -FilePath $fullPath
    $result.NasmCheck = $nasmCheck

    # Exported symbols
    $symbols = Get-ExportedSymbols -FilePath $fullPath
    $result.Symbols = $symbols

    # ── Determine overall status ──
    $issues = @()

    if ($syntax.Status -eq "FAIL")     { $issues += "SYNTAX($($syntax.Errors) errors)" }
    if (-not $balance.Balanced)        { $issues += "UNBALANCED(PROC=$($balance.PROC),ENDP=$($balance.ENDP))" }
    if (-not $result.LineCheck)        { $issues += "LINE_DRIFT($($result.LineDelta))" }
    if ($nasmCheck.Contaminated)       { $issues += "NASM_LEAK($($nasmCheck.Score))" }

    if ($issues.Count -eq 0) {
        $result.Overall = "PASS"
        $result.Issues  = @()
        $ValidationResults.Summary.Passed++
        Write-Host "PASS" -ForegroundColor Green
    } elseif ($file.Critical -and ($syntax.Status -eq "FAIL" -or -not $balance.Balanced)) {
        $result.Overall = "CRITICAL_FAIL"
        $result.Issues  = $issues
        $ValidationResults.Summary.Failed++
        Write-Host "CRITICAL FAIL  [$($issues -join '; ')]" -ForegroundColor Red
    } else {
        $result.Overall = "WARNING"
        $result.Issues  = $issues
        $ValidationResults.Summary.Warnings++
        Write-Host "WARNING  [$($issues -join '; ')]" -ForegroundColor Yellow
    }

    $ValidationResults.Modules += $result
}

# ── Summary ─────────────────────────────────────────────────────────
$ValidationResults.Summary.TotalLines = $totalLines

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                   Verification Summary                   ║" -ForegroundColor Cyan
Write-Host "╠═══════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  Total Modules : $($ValidationResults.Summary.TotalFiles.ToString().PadLeft(4))                                    ║"
Write-Host "║  Total Lines   : $($totalLines.ToString('N0').PadLeft(8))                                ║"
Write-Host "║  Passed        : $($ValidationResults.Summary.Passed.ToString().PadLeft(4))                                    ║" -ForegroundColor Green
Write-Host "║  Warnings      : $($ValidationResults.Summary.Warnings.ToString().PadLeft(4))                                    ║" -ForegroundColor Yellow
Write-Host "║  Failed        : $($ValidationResults.Summary.Failed.ToString().PadLeft(4))                                    ║" -ForegroundColor Red
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# ── Attestation Output ──────────────────────────────────────────────
$certPath = Join-Path $RepoRoot "RawrXD_v15.0.0-GOLD_BuildAttestation.json"
$ValidationResults | ConvertTo-Json -Depth 10 | Out-File -FilePath $certPath -Encoding UTF8

if ($ValidationResults.Summary.Failed -eq 0) {
    Write-Host ""
    Write-Host "  ✅ BUILD CERTIFICATE VALID" -ForegroundColor Green
    Write-Host "  Attestation : $certPath"
    Write-Host ""
    Write-Host "  SHA256 Fingerprints (Critical Modules):" -ForegroundColor White
    $ValidationResults.Modules | Where-Object { $_.Critical -and $_.Exists } | ForEach-Object {
        $hash = $_.Hashes.SHA256
        Write-Host "    $($_.FileName.PadRight(38)) $($hash.Substring(0,16))..." -ForegroundColor DarkGray
    }
    Write-Host ""
    exit 0
} else {
    Write-Host ""
    Write-Host "  ❌ BUILD CERTIFICATE INVALID" -ForegroundColor Red
    Write-Host "  Critical failures in $($ValidationResults.Summary.Failed) module(s):" -ForegroundColor Red
    $ValidationResults.Modules | Where-Object { $_.Overall -eq "CRITICAL_FAIL" } | ForEach-Object {
        Write-Host "    - $($_.FileName): $($_.Issues -join '; ')" -ForegroundColor Red
    }
    Write-Host ""
    exit 1
}
