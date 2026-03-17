#=============================================================================
# Build_Amphibious.ps1  — v2 (fully verified)
# Builds RawrXD_CLI.exe and RawrXD_GUI.exe from shared sovereign core
#
# Requirements:
#   VS 2022 BuildTools 14.44.35207  (ml64, link, lib)
#   Windows SDK 10.0.22621.0
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Build_Amphibious.ps1
#=============================================================================
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---- Toolchain paths -------------------------------------------------------
$MSVC_BIN = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64"
$ML64  = "$MSVC_BIN\ml64.exe"
$LINK  = "$MSVC_BIN\link.exe"
$LIB   = "$MSVC_BIN\lib.exe"
$UM    = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

$SRC   = $PSScriptRoot
$OUT   = "$SRC\build_out"
New-Item -ItemType Directory -Force $OUT | Out-Null

# ---- simple wrapper --------------------------------------------------------
function Run-Step {
    param([string]$Label, [string]$Exe, [string[]]$ToolArgs)
    Write-Host "[....] $Label" -ForegroundColor Yellow
    & $Exe @ToolArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] $Label  (exit $LASTEXITCODE)" -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "[ OK ] $Label" -ForegroundColor Green
}

Write-Host ""
Write-Host "=== RawrXD Amphibious Build ===  (MSVC $MSVC_BIN)" -ForegroundColor Cyan
Write-Host ""

#=============================================================================
# 0. Create import library for printf from msvcrt.dll
#    (VS2022 msvcrt.lib no longer contains a direct printf thunk)
#=============================================================================
$printfDef = "$OUT\msvcrt_printf.def"
@"
LIBRARY msvcrt.dll
EXPORTS
    printf
"@ | Set-Content -Encoding ascii $printfDef

Run-Step "Generate msvcrt_printf.lib" $LIB @(
    "/DEF:$printfDef",
    "/OUT:$OUT\msvcrt_printf.lib",
    "/MACHINE:X64",
    "/NOLOGO"
)

#=============================================================================
# 1. Assemble sovereign core
#=============================================================================
Run-Step "Assemble RawrXD_Sovereign_Core" $ML64 @(
    "/nologo", "/c",
    "/Fo", "$OUT\sovereign_core.obj",
    "$SRC\RawrXD_Sovereign_Core.asm"
)

#=============================================================================
# 2. CLI target
#=============================================================================
Run-Step "Assemble RawrXD_CLI" $ML64 @(
    "/nologo", "/c",
    "/Fo", "$OUT\cli.obj",
    "$SRC\RawrXD_CLI.asm"
)

Run-Step "Link RawrXD_CLI.exe" $LINK @(
    "/nologo", "/NODEFAULTLIB",
    "/SUBSYSTEM:CONSOLE", "/ENTRY:main",
    "/OUT:$OUT\RawrXD_CLI.exe",
    "$OUT\cli.obj",
    "$OUT\sovereign_core.obj",
    "$OUT\msvcrt_printf.lib",
    "$UM\kernel32.lib",
    "$UM\user32.lib"
)

#=============================================================================
# 3. GUI target
#=============================================================================
Run-Step "Assemble RawrXD_GUI" $ML64 @(
    "/nologo", "/c",
    "/Fo", "$OUT\gui.obj",
    "$SRC\RawrXD_GUI.asm"
)

Run-Step "Link RawrXD_GUI.exe" $LINK @(
    "/nologo", "/NODEFAULTLIB",
    "/SUBSYSTEM:WINDOWS", "/ENTRY:WinMain",
    "/OUT:$OUT\RawrXD_GUI.exe",
    "$OUT\gui.obj",
    "$OUT\sovereign_core.obj",
    "$OUT\msvcrt_printf.lib",
    "$UM\kernel32.lib",
    "$UM\user32.lib",
    "$UM\gdi32.lib"
)

#=============================================================================
# Done
#=============================================================================
Write-Host ""
Write-Host "=== BUILD COMPLETE ===" -ForegroundColor Cyan
$cliSz = (Get-Item "$OUT\RawrXD_CLI.exe").Length
$guiSz = (Get-Item "$OUT\RawrXD_GUI.exe").Length
Write-Host "  CLI : $OUT\RawrXD_CLI.exe  ($cliSz bytes)"
Write-Host "  GUI : $OUT\RawrXD_GUI.exe  ($guiSz bytes)"
Write-Host ""
Write-Host "Quick test:"
Write-Host "  & '$OUT\RawrXD_CLI.exe'"
Write-Host "  Start-Process '$OUT\RawrXD_GUI.exe'"
