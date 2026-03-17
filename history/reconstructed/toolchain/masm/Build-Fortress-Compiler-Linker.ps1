#Requires -Version 5.1
<#
.SYNOPSIS
    Build RawrXD Fortress Solo Compiler and Linker (MASM64 / NASM x64).

.DESCRIPTION
    Builds:
    - solo_standalone_compiler.asm -> rawrxd-scc (NASM or MASM)
    - rawrxd_link.asm -> rawrxd_link.exe (MASM64)
    Output: D:\RawrXD\bin\

.PARAMETER Assembler
    masm64, nasm64, or both (default: both)
#>
param(
    [ValidateSet("masm64", "nasm64", "both")]
    [string]$Assembler = "both",
    [string]$SrcRoot = "D:\RawrXD\src\asm",
    [string]$BinRoot = "D:\RawrXD\bin"
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path $BinRoot)) { New-Item -ItemType Directory -Path $BinRoot -Force | Out-Null }

$ml64 = $null
$nasm = $null
foreach ($p in @(
    "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe",
    "D:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
)) {
    if (Test-Path $p) { $ml64 = $p; break }
}
if (-not $ml64) { $ml64 = "ml64" }

foreach ($p in @("C:\nasm\nasm.exe", "C:\Program Files\NASM\nasm.exe", "D:\rawrxd\toolchain\nasm\nasm.exe")) {
    if (Test-Path $p) { $nasm = $p; break }
}
if (-not $nasm) { $nasm = "nasm" }

Write-Host "RawrXD Fortress Compiler + Linker Build" -ForegroundColor Cyan
Write-Host "  MASM64: $ml64"
Write-Host "  NASM:   $nasm"
Write-Host "  Out:    $BinRoot"
Write-Host ""

# --- RawrXD-SCC v4.0 (700-line COFF64 assembler; replaces ml64 after bootstrap) ---
$sccAsm = Join-Path $SrcRoot "rawrxd_scc.asm"
if (Test-Path $sccAsm) {
    Write-Host "[1] Building rawrxd_scc.asm (RawrXD-SCC v4.0, MASM64)..." -ForegroundColor Yellow
    $sccObj = Join-Path $BinRoot "rawrxd_scc.obj"
    $sccExe = Join-Path $BinRoot "rawrxd_scc.exe"
    & $ml64 /nologo /c /Fo $sccObj $sccAsm 2>&1
    if ($LASTEXITCODE -eq 0) {
        & link /nologo $sccObj /entry:main /subsystem:console /out:$sccExe kernel32.lib 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  OK: $sccExe (bootstrap: ml64 once, then scc assembles itself)" -ForegroundColor Green
        } else { Write-Host "  FAIL: link rawrxd_scc" -ForegroundColor Red }
    } else { Write-Host "  FAIL: ml64 rawrxd_scc.asm" -ForegroundColor Red }
} else {
    Write-Host "[1] Skip: rawrxd_scc.asm not found" -ForegroundColor Gray
}

# --- Linker (MASM64 only) ---
$linkAsm = Join-Path $SrcRoot "rawrxd_link.asm"
if (Test-Path $linkAsm) {
    Write-Host "[2] Building rawrxd_link.asm (MASM64)..." -ForegroundColor Yellow
    $linkObj = Join-Path $BinRoot "rawrxd_link.obj"
    $linkExe = Join-Path $BinRoot "rawrxd_link.exe"
    & $ml64 /nologo /c /Fo $linkObj $linkAsm 2>&1
    if ($LASTEXITCODE -eq 0) {
        & link /nologo $linkObj /subsystem:console /out:$linkExe kernel32.lib 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  OK: $linkExe" -ForegroundColor Green
        } else { Write-Host "  FAIL: link" -ForegroundColor Red }
    } else { Write-Host "  FAIL: ml64 rawrxd_link.asm" -ForegroundColor Red }
} else {
    Write-Host "[2] Skip: rawrxd_link.asm not found" -ForegroundColor Gray
}

# --- Solo compiler: NASM or MASM ---
$soloAsm = Join-Path $SrcRoot "solo_standalone_compiler.asm"
if (-not (Test-Path $soloAsm)) {
    $soloAsm = Join-Path $SrcRoot "..\..\legacy\solo_standalone_compiler.asm"
}
if (Test-Path $soloAsm) {
    if ($Assembler -eq "nasm64" -or $Assembler -eq "both") {
        Write-Host "[3] Building solo_standalone_compiler (NASM x64)..." -ForegroundColor Yellow
        $soloObjNasm = Join-Path $BinRoot "rawrxd-scc-nasm64.obj"
        $soloExeNasm = Join-Path $BinRoot "rawrxd-scc-nasm64.exe"
        & $nasm -f win64 $soloAsm -o $soloObjNasm 2>&1
        if ($LASTEXITCODE -eq 0) {
            & link /nologo $soloObjNasm /subsystem:console /out:$soloExeNasm kernel32.lib 2>&1
            if ($LASTEXITCODE -eq 0) { Write-Host "  OK: $soloExeNasm" -ForegroundColor Green }
            else { Write-Host "  FAIL: link (nasm)" -ForegroundColor Red }
        } else { Write-Host "  FAIL: nasm (solo compiler may be MASM-only)" -ForegroundColor Red }
    }
    if ($Assembler -eq "masm64" -or $Assembler -eq "both") {
        Write-Host "[4] Building solo_standalone_compiler (MASM64)..." -ForegroundColor Yellow
        $soloObjMasm = Join-Path $BinRoot "rawrxd-scc-masm64.obj"
        $soloExeMasm = Join-Path $BinRoot "rawrxd-scc-masm64.exe"
        & $ml64 /nologo /c /Fo $soloObjMasm $soloAsm 2>&1
        if ($LASTEXITCODE -eq 0) {
            & link /nologo $soloObjMasm /subsystem:console /out:$soloExeMasm kernel32.lib 2>&1
            if ($LASTEXITCODE -eq 0) { Write-Host "  OK: $soloExeMasm" -ForegroundColor Green }
            else { Write-Host "  FAIL: link (masm)" -ForegroundColor Red }
        } else { Write-Host "  FAIL: ml64 (solo compiler may be NASM-only)" -ForegroundColor Red }
    }
} else {
    Write-Host "[3/4] Skip: solo_standalone_compiler.asm not found" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Fortress compiler/linker build done. Binaries in $BinRoot" -ForegroundColor Green
