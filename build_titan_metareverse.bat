@echo off
setlocal enabledelayedexpansion

:: RawrXD_Titan_MetaReverse Build Script
:: Finds VS Build Tools or Community and assembles + links with zero environment
:: Result: RawrXD-Titan-MetaReverse.exe with zero static imports

echo [*] RawrXD Titan MetaReverse - Zero-Dependency Build
echo.

:: Find ml64.exe from any VS installation
set "ML64="
set "LINK="

echo [+] Searching for ml64.exe...
for /f "delims=" %%i in ('where /r "%ProgramFiles(x86)%\Microsoft Visual Studio" ml64.exe 2^>nul ^| findstr /i "x64"') do (
    set "ML64=%%i"
    goto :found_ml64
)

for /f "delims=" %%i in ('where /r "%ProgramFiles%\Microsoft Visual Studio" ml64.exe 2^>nul ^| findstr /i "x64"') do (
    set "ML64=%%i"
    goto :found_ml64
)

echo [-] ml64.exe not found in VS Build Tools or Community
exit /b 1

:found_ml64
echo [+] Found ml64.exe: %ML64%

:: Find link.exe in same directory
for %%D in ("%ML64%") do (
    set "LINK=%%~dpD\link.exe"
)

if not exist "%LINK%" (
    echo [-] link.exe not found at %LINK%
    exit /b 1
)

echo [+] Found link.exe: %LINK%
echo.

:: Paths
set "SRCFILE=D:\rawrxd\src\RawrXD_Titan_MetaReverse.asm"
set "OBJFILE=D:\rawrxd\src\RawrXD_Titan_MetaReverse.obj"
set "OUTEXE=D:\rawrxd\bin\RawrXD-Titan-MetaReverse.exe"

if not exist "%SRCFILE%" (
    echo [-] Source file not found: %SRCFILE%
    exit /b 1
)

:: Step 1: Assemble
echo [1/2] Assembling %SRCFILE%...
"%ML64%" /c /Zi /O2 /Fo"%OBJFILE%" "%SRCFILE%"
if errorlevel 1 (
    echo [-] Assembly failed
    exit /b 1
)
echo [+] Assembly successful: %OBJFILE%

:: Step 2: Link with NODEFAULTLIB
echo [2/2] Linking (no default libs, PEBMain entry)...
"%LINK%" /SUBSYSTEM:CONSOLE /NODEFAULTLIB /ENTRY:PEBMain /OUT:"%OUTEXE%" "%OBJFILE%"
if errorlevel 1 (
    echo [-] Linking failed
    exit /b 1
)
echo [+] Linking successful: %OUTEXE%

echo.
echo ============================================================
echo [SUCCESS] Built RawrXD-Titan-MetaReverse.exe
echo          Zero static imports, PEB walking bootstrap
echo          Size: %OUTEXE%
echo ============================================================

endlocal
exit /b 0
