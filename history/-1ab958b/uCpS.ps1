#!/usr/bin/env pwsh
# Build Internal Toolchain
# Creates self-contained MASM compiler and linker

$ErrorActionPreference = "Stop"

Write-Host "Building Internal Toolchain (Self-Contained)" -ForegroundColor Cyan

$ToolchainDir = Split-Path -Parent $PSCommandPath
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $ToolchainDir)

# Check for external compiler to build initial tools
$CppCompiler = $null
$compilers = @("cl.exe", "g++.exe", "clang++.exe")

foreach ($compiler in $compilers) {
    if (Get-Command $compiler -ErrorAction SilentlyContinue) {
        $CppCompiler = $compiler
        break
    }
}

if (-not $CppCompiler) {
    Write-Host "ERROR: Need initial C++ compiler to build internal toolchain" -ForegroundColor Red
    Write-Host "Install one of: MSVC, GCC, or Clang" -ForegroundColor Yellow
    Write-Host "" 
    Write-Host "After building internal tools, NO external compiler needed!" -ForegroundColor Green
    exit 1
}

Write-Host "Using: $CppCompiler" -ForegroundColor Green

# Build PE writer (internal linker)
Write-Host "Building internal linker..." -ForegroundColor Yellow
$peWriterSource = Join-Path $ToolchainDir "pe_writer.cpp"
$peWriterExe = Join-Path $ToolchainDir "internal_link.exe"

if ($CppCompiler -eq "cl.exe") {
    cl.exe /O2 /EHsc $peWriterSource /Fe:$peWriterExe | Out-Null
} else {
    & $CppCompiler -O2 -o $peWriterExe $peWriterSource
}

if ($LASTEXITCODE -eq 0 -and (Test-Path $peWriterExe)) {
    Write-Host "  Success: internal_link.exe" -ForegroundColor Green
} else {
    Write-Host "  Failed to build internal linker" -ForegroundColor Red
    exit 1
}

# Build MASM compiler (from assembly)
Write-Host "Building internal MASM compiler..." -ForegroundColor Yellow
$masmSource = Join-Path $ToolchainDir "masm_solo_compiler.asm"
$masmObj = Join-Path $ToolchainDir "masm_solo_compiler.obj"
$masmExe = Join-Path $ToolchainDir "masm_solo_compiler.exe"

# For initial build, need external ml64.exe one time
$ml64 = $null
$ml64Paths = @(
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\ml64.exe",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\x64\ml64.exe"
)

foreach ($path in $ml64Paths) {
    if (Test-Path $path) {
        $ml64 = $path
        break
    }
}

if (-not $ml64) {
    $ml64 = Get-Command ml64.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

if ($ml64) {
    & $ml64 /c /Fo $masmObj $masmSource | Out-Null
    
    # Link with internal linker or external
    if (Test-Path $peWriterExe) {
        & $peWriterExe /OUT:$masmExe $masmObj
    } else {
        link.exe /OUT:$masmExe /SUBSYSTEM:CONSOLE /MACHINE:X64 $masmObj kernel32.lib | Out-Null
    }
    
    if ($LASTEXITCODE -eq 0 -and (Test-Path $masmExe)) {
        Write-Host "  Success: masm_solo_compiler.exe" -ForegroundColor Green
    } else {
        Write-Host "  Failed to build MASM compiler" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "  MASM source exists but need ML64.exe for initial bootstrap" -ForegroundColor Yellow
    Write-Host "  After first build, MASM compiler is self-contained" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Internal Toolchain Ready!" -ForegroundColor Green
Write-Host "  internal_link.exe       - Self-contained linker" -ForegroundColor Gray
Write-Host "  masm_solo_compiler.exe  - Self-contained MASM compiler" -ForegroundColor Gray
Write-Host ""
Write-Host "Future builds require NO external SDK or toolchain!" -ForegroundColor Cyan
