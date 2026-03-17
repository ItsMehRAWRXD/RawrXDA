# test-reverse-linking.ps1
# Validates the "source = linking - assembling" reverse-link architecture.
# Checks MASM-AutoPatch-Builder.ps1  +  genesis_build.ps1  (no ml64/cl/clang anywhere)

$ErrorActionPreference = "Stop"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  RawrXD Reverse-Link Architecture Validation" -ForegroundColor Cyan
Write-Host "  source = linking - assembling (DisableRecompile=ON)" -ForegroundColor Magenta
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host

$builderPath = "D:\MASM-AutoPatch-Builder.ps1"
$genesisPath = "D:\genesis_build.ps1"

# helper: parse file and report syntax errors
function Test-ParseFile([string]$path, [string]$label) {
    if (!(Test-Path $path)) {
        Write-Host "    FAIL  $label not found at $path" -ForegroundColor Red
        return $false
    }
    $tokens = $null; $errs = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errs)
    if ($errs -and $errs.Count -gt 0) {
        Write-Host "    FAIL  $label has $($errs.Count) syntax error(s):" -ForegroundColor Red
        $errs | Select-Object -First 3 | ForEach-Object {
            Write-Host "      Line $($_.Extent.StartLineNumber): $($_.Message)" -ForegroundColor Red
        }
        return $false
    }
    return $true
}

# ── MASM-AutoPatch-Builder.ps1 checks ─────────────────────────────────────────
Write-Host "[TEST 1] MASM-AutoPatch-Builder.ps1 parses cleanly..." -ForegroundColor White
if (Test-ParseFile $builderPath "MASM-AutoPatch-Builder.ps1") {
    Write-Host "    OK  No syntax errors" -ForegroundColor Green
}

$script = Get-Content $builderPath -Raw

Write-Host "[TEST 2] DisableRecompile parameter present..." -ForegroundColor White
if ($script -match 'DisableRecompile') {
    Write-Host "    OK  DisableRecompile found" -ForegroundColor Green
} else { Write-Host "    FAIL  DisableRecompile missing" -ForegroundColor Red }

Write-Host "[TEST 3] Resolve-InHouseCompiler (no ml64/cl/clang resolver)..." -ForegroundColor White
if ($script -match 'Resolve-InHouseCompiler') {
    Write-Host "    OK  Resolve-InHouseCompiler present" -ForegroundColor Green
} else { Write-Host "    FAIL  Resolve-InHouseCompiler missing" -ForegroundColor Red }

Write-Host "[TEST 4] /DIRECTLINK + Invoke-InHouseLinkOne present..." -ForegroundColor White
if ($script -match 'DIRECTLINK' -and $script -match 'Invoke-InHouseLinkOne') {
    Write-Host "    OK  /DIRECTLINK + Invoke-InHouseLinkOne wired" -ForegroundColor Green
} else { Write-Host "    FAIL  DIRECTLINK or Invoke-InHouseLinkOne missing" -ForegroundColor Red }

Write-Host "[TEST 5] Invoke-MASMBuildOne removed (old ml64 function gone)..." -ForegroundColor White
if ($script -notmatch 'Invoke-MASMBuildOne') {
    Write-Host "    OK  Old Invoke-MASMBuildOne is gone" -ForegroundColor Green
} else { Write-Host "    FAIL  Invoke-MASMBuildOne still present" -ForegroundColor Red }

Write-Host "[TEST 6] No live ml64/cl/clang invocations in builder..." -ForegroundColor White
$live = [regex]::Matches($script, '(?m)^\s*(?!#)[^\r\n]*\b(Start-Process|& )\b[^\r\n]*\b(ml64|cl\.exe|clang)\b')
if ($live.Count -eq 0) {
    Write-Host "    OK  Zero live ml64/cl/clang invocations" -ForegroundColor Green
} else {
    Write-Host "    WARN  $($live.Count) potential invocation(s) — review:" -ForegroundColor Yellow
    $live | ForEach-Object { Write-Host "      >> $($_.Value.Trim())" -ForegroundColor DarkYellow }
}

# ── genesis_build.ps1 checks ──────────────────────────────────────────────────
Write-Host ""
Write-Host "-- genesis_build.ps1  (Monolithic EXE) -----------------" -ForegroundColor Cyan

Write-Host "[TEST 7] genesis_build.ps1 parses cleanly..." -ForegroundColor White
if (Test-ParseFile $genesisPath "genesis_build.ps1") {
    Write-Host "    OK  No syntax errors" -ForegroundColor Green
}

$genesis = Get-Content $genesisPath -Raw

Write-Host "[TEST 8] /DIRECTLINK + /OUT_COFF per-module..." -ForegroundColor White
if ($genesis -match 'DIRECTLINK' -and $genesis -match 'OUT_COFF') {
    Write-Host "    OK  /DIRECTLINK + /OUT_COFF present" -ForegroundColor Green
} else { Write-Host "    FAIL  DIRECTLINK or OUT_COFF missing" -ForegroundColor Red }

Write-Host "[TEST 9] /LIBMODE => rawrxd_core.lib + rawrxd_gpu.lib..." -ForegroundColor White
if ($genesis -match 'LIBMODE' -and $genesis -match 'rawrxd_core\.lib' -and $genesis -match 'rawrxd_gpu\.lib') {
    Write-Host "    OK  /LIBMODE + both static libs wired" -ForegroundColor Green
} else { Write-Host "    FAIL  /LIBMODE or lib names missing" -ForegroundColor Red }

Write-Host "[TEST 10] /LINKMODE + /SUBSYSTEM:WINDOWS + /ENTRY:WinMain (monolithic PE32+)..." -ForegroundColor White
if ($genesis -match 'LINKMODE' -and $genesis -match 'SUBSYSTEM:WINDOWS' -and $genesis -match 'ENTRY:WinMain') {
    Write-Host "    OK  Monolithic PE32+ link args present" -ForegroundColor Green
} else { Write-Host "    FAIL  Final link args missing" -ForegroundColor Red }

Write-Host "[TEST 11] No VSINSTALLDIR / Resolve-VSTool in genesis..." -ForegroundColor White
if ($genesis -notmatch 'VSINSTALLDIR' -and $genesis -notmatch 'Resolve-VSTool') {
    Write-Host "    OK  Zero VS dependency — pure in-house" -ForegroundColor Green
} else { Write-Host "    FAIL  VS reference still present" -ForegroundColor Red }

Write-Host "[TEST 12] Parallel /DIRECTLINK via ForEach-Object -Parallel..." -ForegroundColor White
if ($genesis -match 'ForEach-Object -Parallel' -and $genesis -match 'ThrottleLimit') {
    Write-Host "    OK  Parallel DIRECTLINK with ThrottleLimit" -ForegroundColor Green
} else { Write-Host "    FAIL  Parallel dispatch missing" -ForegroundColor Red }

Write-Host "[TEST 13] No live ml64/cl/clang in genesis_build.ps1..." -ForegroundColor White
$genLive = [regex]::Matches($genesis, '(?m)^\s*(?!#)[^\r\n]*\b(Start-Process|& )\b[^\r\n]*\b(ml64|cl\.exe|clang)\b')
if ($genLive.Count -eq 0) {
    Write-Host "    OK  Zero live ml64/cl/clang invocations" -ForegroundColor Green
} else {
    Write-Host "    FAIL  $($genLive.Count) invocation(s) found" -ForegroundColor Red
}

# ── Plan-only dry run ──────────────────────────────────────────────────────────
Write-Host ""
Write-Host "-- Plan-only dry run (genesis_build.ps1 -PlanOnly) -----" -ForegroundColor Cyan
try {
    & $genesisPath -PlanOnly -ErrorAction Stop
} catch {
    Write-Host "  PlanOnly threw: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  Usage:" -ForegroundColor Yellow
Write-Host "  .\genesis_build.ps1" -ForegroundColor White
Write-Host "    => /DIRECTLINK each .asm => COFF .obj  (no assemble step)" -ForegroundColor Gray
Write-Host "    => /LIBMODE:  core objs => rawrxd_core.lib" -ForegroundColor Gray
Write-Host "    => /LIBMODE:  gpu  objs => rawrxd_gpu.lib" -ForegroundColor Gray
Write-Host "    => /LINKMODE: entry + libs + .res => RawrXD.exe (PE32+)" -ForegroundColor Gray
Write-Host "    => Single binary. NO per-subagent .exe splatter." -ForegroundColor Gray
Write-Host "========================================================" -ForegroundColor Cyan
