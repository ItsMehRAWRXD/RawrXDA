# Diagnose-Win32IDE-DLLs.ps1
# Runs dumpbin on RawrXD-Win32IDE.exe and verifies required DLLs exist.
# Use when IDE crashes silently on launch (missing DLLs cause loader failure before WinMain).

param(
    [string]$ExePath = "D:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe",
    [switch]$FixPath
)

$ErrorActionPreference = "Stop"

# Resolve exe path
$exeDir = Split-Path -Parent $ExePath
$exeName = Split-Path -Leaf $ExePath
if (-not (Test-Path $ExePath)) {
    $alt = Join-Path (Get-Location) "build_ide\bin\RawrXD-Win32IDE.exe"
    if (Test-Path $alt) { $ExePath = $alt; $exeDir = Split-Path -Parent $ExePath }
    else { Write-Host "ERROR: RawrXD-Win32IDE.exe not found at $ExePath or $alt"; exit 1 }
}

Write-Host "`n=== RawrXD-Win32IDE DLL Diagnostics ===" -ForegroundColor Cyan
Write-Host "Exe: $ExePath`n"

# Find dumpbin
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -property installationPath 2>$null
$dumpbin = $null
if ($vsPath) {
    $candidates = Get-ChildItem -Path "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue
    if ($candidates) { $dumpbin = $candidates[0].FullName }
}
if (-not $dumpbin) { Write-Host "WARN: dumpbin not found; skipping dependency list" -ForegroundColor Yellow }
else {
    Write-Host "--- Dependencies (dumpbin /dependents) ---" -ForegroundColor Yellow
    & $dumpbin /dependents $ExePath 2>&1
}

# Check critical DLLs in System32 and exe dir
$sys32 = Join-Path $env:windir "System32"
$critical = @(
    "vulkan-1.dll",      # Vulkan runtime - often missing on minimal Windows
    "D3DCOMPILER_47.dll",# DirectX shader compiler
    "dbghelp.dll",       # Crash containment minidump
    "d2d1.dll", "DWrite.dll", "d3d11.dll", "dcomp.dll"
)
Write-Host "`n--- DLL presence ---" -ForegroundColor Yellow
foreach ($dll in $critical) {
    $inSys = Test-Path (Join-Path $sys32 $dll)
    $inExe = Test-Path (Join-Path $exeDir $dll)
    $ok = $inSys -or $inExe
    $status = if ($ok) { "OK" } else { "MISSING" }
    $color = if ($ok) { "Green" } else { "Red" }
    Write-Host "  $dll : $status (System32=$inSys, exe_dir=$inExe)" -ForegroundColor $color
}

# Suggested fixes
Write-Host "`n--- Suggested fixes ---" -ForegroundColor Yellow
Write-Host "  - Run from exe's directory: cd `"$exeDir`"; .\RawrXD-Win32IDE.exe"
Write-Host "  - Debug console (see early output): `$env:RAWRXD_DEBUG_CONSOLE='1'; .\RawrXD-Win32IDE.exe"
Write-Host "  - Vulkan runtime: https://vulkan.lunarg.com/sdk/home"
Write-Host "  - DirectX End-User Runtime: https://www.microsoft.com/en-us/download/details.aspx?id=35"
Write-Host ""
