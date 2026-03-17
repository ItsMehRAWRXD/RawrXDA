# fix_ide_logging.ps1 — Batch fix std::cout/printf/qDebug in IDE source (per .cursorrules)
# Run from repo root. Backs up nothing; use git to revert.
# Excludes: ggml-*, sqlite3, gguf.c, third-party C, asm, *.cu, *.cuh, *.m, *.js

$srcRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\src"))
$excludeDirs = @(
    "ggml-cuda", "ggml-metal", "ggml-hexagon", "ggml-vulkan", "ggml-cann", "ggml-sycl",
    "ggml-opencl", "ggml-webgpu", "ggml-zdnn", "ggml-rpc", "ggml-blas", "ggml-cpu",
    "ggml-hexagon", "ggml_masm", "security-engines"
)
$excludeFiles = @("sqlite3.c", "ggml.c", "gguf.c", "ggml-quants.c", "ggml-backend.cpp", "ggml-opt.cpp")

$count = 0
$reported = @()

Get-ChildItem -Path $srcRoot -Recurse -Include "*.cpp","*.h" -File | ForEach-Object {
    $full = [System.IO.Path]::GetFullPath($_.FullName)
    if (-not $full.StartsWith($srcRoot, [StringComparison]::OrdinalIgnoreCase)) { return }
    $base = $srcRoot.TrimEnd('\', '/')
    if ($full.Length -le $base.Length) { return }
    $rel = $full.Substring($base.Length).TrimStart('\', '/')
    if ([string]::IsNullOrEmpty($rel)) { return }
    $skip = $false
    foreach ($d in $excludeDirs) {
        if ($rel -like "$d*") { $skip = $true; break }
    }
    if ($skip) { return }
    foreach ($f in $excludeFiles) {
        if ($_.Name -eq $f) { $skip = $true; break }
    }
    if ($skip) { return }

    $content = Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { return }

    $changed = $false
    # Simple literal-only cout to LOG_INFO (single string, no vars)
    if ($content -match 'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl\s*;') {
        $content = $content -replace 'std::cout\s*<<\s*"([^"]*)"\s*<<\s*std::endl\s*;', 'LOG_INFO("$1");'
        $changed = $true
    }
    if ($content -match 'std::cout\s*<<\s*"([^"]*)"\s*;\s*$') {
        $content = $content -replace 'std::cout\s*<<\s*"([^"]*)"\s*;\s*$', 'LOG_INFO("$1");'
        $changed = $true
    }
    # Ensure IDELogger include for Win32 sources
    if ($changed -and $rel -like "win32app*" -and $content -notmatch 'IDELogger\.h') {
        if ($content -match '(?m)^#include\s+"Win32IDE\.h"') {
            $content = $content -replace '(#include\s+"Win32IDE\.h")', "`$1`r`n#include `"IDELogger.h`""
            $changed = $true
        }
    }

    if ($changed) {
        Set-Content -LiteralPath $_.FullName -Value $content -NoNewline
        $count++
        $reported += $rel
    }
}

Write-Host "fix_ide_logging: touched $count files."
$reported | ForEach-Object { Write-Host "  $_" }
