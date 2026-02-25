@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Universal Binary Analyzer Build Script
echo ========================================
echo.

REM Set paths
set "MASM32_PATH=C:\masm32"
set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
set "VC_TOOLS=%VS_PATH%\VC\Tools\MSVC\14.44.35207"
set "ML64_PATH=%VC_TOOLS%\bin\Hostx64\x64"
set "LINK_PATH=%VC_TOOLS%\bin\Hostx64\x64"
set "WINSDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0"

REM Set project paths
set "SRC_PATH=src"
set "BIN_PATH=bin"
set "OBJ_PATH=obj"

REM Create directories if they don't exist
if not exist "%BIN_PATH%" mkdir "%BIN_PATH%"
if not exist "%OBJ_PATH%" mkdir "%OBJ_PATH%"

echo [1/2] Building Universal Analyzer...
"%MASM32_PATH%\bin\ml.exe" /c /coff /Fo"%OBJ_PATH%\universal_analyzer.obj" /I"%MASM32_PATH%\include" -D_CONSOLE -D_WIN32 -D_X86_ "%SRC_PATH%\universal_analyzer.asm"
if errorlevel 1 goto :error_asm

echo [2/2] Linking Universal Analyzer...
"%LINK_PATH%\link.exe" /SUBSYSTEM:CONSOLE /MACHINE:X86 /OUT:"%BIN_PATH%\universal_analyzer.exe" /LIBPATH:"%MASM32_PATH%\lib" /LIBPATH:"%WINSDK_LIB%\um\x86" /LIBPATH:"%WINSDK_LIB%\ucrt\x86" "%OBJ_PATH%\universal_analyzer.obj" kernel32.lib user32.lib advapi32.lib psapi.lib
if errorlevel 1 goto :error_link

echo.
echo ========================================
echo Build completed successfully!
echo Universal Analyzer: %BIN_PATH%\universal_analyzer.exe
echo ========================================
echo.

endlocal
exit /b 0

:error_asm
echo.
echo ========================================
echo ERROR: Assembly failed!
echo ========================================
echo.
endlocal
exit /b 1

:error_link
echo.
echo ========================================
echo ERROR: Linking failed!
echo ========================================
echo.
endlocal
exit /b 1
