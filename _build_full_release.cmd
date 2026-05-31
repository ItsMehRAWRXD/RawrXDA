@echo off
REM ============================================================================
REM PHASE 1 OPTIMIZATION: Release Build with Debug Symbols Stripped
REM ============================================================================
REM
REM This build configuration removes debug bloat (55 MB) by:
REM   - Removing /DEBUG linker flag (no embedded PDB data)
REM   - Removing /Zi compile flag (no debug info in objects)
REM   - Adding /OPT:REF (strip unreferenced functions)
REM   - Adding /OPT:ICF (fold identical functions)
REM   - Adding /MERGE:.rdata=.text (merge read-only data with code)
REM   - Adding /ALIGN:512 (reduce section alignment waste)
REM
REM Expected Result: 113.8 MB -> 60-75 MB (30-45 MB savings)
REM Build Time: ~30 seconds (no debug info processing)
REM
REM ============================================================================

setlocal

set "ROOT=D:\rawrxd"
set "MSVC=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717"
set "ML64=%MSVC%\bin\Hostx64\x64\ml64.exe"
set "CL_EXE=%MSVC%\bin\Hostx64\x64\cl.exe"
set "LINK_EXE=%MSVC%\bin\Hostx64\x64\link.exe"
set "KIT=C:\Program Files (x86)\Windows Kits\10"
set "SDK=10.0.22621.0"

set "INCLUDE=%MSVC%\include;%KIT%\Include\%SDK%\ucrt;%KIT%\Include\%SDK%\um;%KIT%\Include\%SDK%\shared;%KIT%\Include\%SDK%\winrt"
set "LIB=%MSVC%\lib\x64;%KIT%\Lib\%SDK%\ucrt\x64;%KIT%\Lib\%SDK%\um\x64"
set "PATH=%MSVC%\bin\Hostx64\x64;%KIT%\bin\%SDK%\x64;%PATH%"

cd /d "%ROOT%"
if not exist build mkdir build

echo.
echo ╔══════════════════════════════════════════════════════════════════╗
echo ║ PHASE 1 OPTIMIZATION BUILD - DEBUG SYMBOLS STRIPPED              ║
echo ║ Expected Size Reduction: 30-45 MB                               ║
echo ╚══════════════════════════════════════════════════════════════════╝
echo.

REM ============================================================================
REM ASSEMBLY PHASE (unchanged)
REM ============================================================================
echo [1/3] Assembling Sovereign ASM modules...

"%ML64%" /c /Cx SovereignText.asm /Fobuild\SovereignText.obj || exit /b 1
"%ML64%" /c /Cx SovereignChat.asm /Fobuild\SovereignChat.obj || exit /b 1
"%ML64%" /c /Cx SovereignResidencyScheduler.asm /Fobuild\SovereignResidencyScheduler.obj || exit /b 1
"%ML64%" /c /Cx SovereignHarnessShim.asm /Fobuild\SovereignHarnessShim.obj || exit /b 1
"%ML64%" /c /Cx SovereignGMMUPinning.asm /Fobuild\SovereignGMMUPinning.obj || exit /b 1
"%ML64%" /c /Cx SovereignWMMALoader.asm /Fobuild\SovereignWMMALoader.obj || exit /b 1
"%ML64%" /c /Cx SovereignWMMAKernel.asm /Fobuild\SovereignWMMAKernel.obj || exit /b 1

if exist SovereignWMMALoader.obj move /y SovereignWMMALoader.obj build\SovereignWMMALoader.obj >nul
if exist SovereignWMMAKernel.obj move /y SovereignWMMAKernel.obj build\SovereignWMMAKernel.obj >nul

echo [2/3] Compiling C++ modules (RELEASE: no /Zi, no debug info)...

REM KEY DIFFERENCE: Removed /Zi (no debug info in object files)
"%CL_EXE%" /nologo /std:c++20 /O2 /GS- /EHs-c- /Zl /c SovereignCapture.cpp /Fobuild\SovereignCapture.obj || exit /b 1
"%CL_EXE%" /nologo /std:c++20 /O2 /GS- /EHs-c- /Zl /c SovereignBlitSmoke.cpp /Fobuild\SovereignBlitSmoke.obj || exit /b 1

echo [3/3] Linking with OPTIMIZATION FLAGS...

if exist RawrXD-Sovereign.exe del /f /q RawrXD-Sovereign.exe

REM KEY DIFFERENCES:
REM   - Removed /DEBUG (no embedded PDB data)
REM   - Added /OPT:REF (strip unreferenced functions)
REM   - Added /OPT:ICF (fold identical functions)
REM   - Added /MERGE:.rdata=.text (merge read-only into code section)
REM   - Added /ALIGN:512 (reduce alignment padding waste)
REM   - Added /LTCG:OFF (disable link-time code gen bloat)
REM

"%LINK_EXE%" /nologo /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup /MACHINE:X64 /NODEFAULTLIB ^
  /OPT:REF /OPT:ICF /MERGE:.rdata=.text /ALIGN:512 ^
  /LTCG:OFF ^
  build\SovereignBlitSmoke.obj ^
  build\SovereignCapture.obj ^
  build\SovereignText.obj ^
  build\SovereignChat.obj ^
  build\SovereignResidencyScheduler.obj ^
  build\SovereignHarnessShim.obj ^
  build\SovereignGMMUPinning.obj ^
  build\SovereignWMMALoader.obj ^
  build\SovereignWMMAKernel.obj ^
  kernel32.lib user32.lib gdi32.lib msvcrt.lib ^
  /OUT:RawrXD-Sovereign.exe || exit /b 1

REM ============================================================================
REM SIZE VERIFICATION
REM ============================================================================

for %%F in (RawrXD-Sovereign.exe) do set SIZE=%%~zF
set /a SIZEMB=SIZE / 1048576
set /a SIZEKB=SIZE / 1024

if %SIZE% GEQ 1048576 (
  echo.
  echo ❌ FAIL: RawrXD-Sovereign.exe size %SIZE% bytes (%SIZEKB% KB, %SIZEMB% MB)
  echo    Size constraint: must be ^< 1048576 bytes ^(1 MB^)
  exit /b 2
)

echo.
echo ╔══════════════════════════════════════════════════════════════════╗
echo ║ ✅ PHASE 1 BUILD SUCCESS                                        ║
echo ╠══════════════════════════════════════════════════════════════════╣
echo ║ Binary: RawrXD-Sovereign.exe                                     ║
echo ║ Size:   %SIZE% bytes (%SIZEKB% KB)                              ║
echo ║ Status: ✅ UNDER 1 MB CONSTRAINT                                ║
echo ║                                                                  ║
echo ║ Expected IDE Size: 60-75 MB (from 113.8 MB)                     ║
echo ║ Savings Estimate: 30-45 MB debug bloat removed                  ║
echo ╚══════════════════════════════════════════════════════════════════╝
echo.

exit /b 0
