@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM RawrXD Production Build Script
REM Builds: ASM Core DLLs + Win32 IDE + CLI
REM No Qt, No external dependencies
REM ============================================================================

echo.
echo ===============================================
echo  RawrXD Production Build
echo ===============================================
echo.

REM Setup VS2022 paths
set "MSVC_BIN=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
set "MSVC_LIB=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\onecore\x64"
set "SDK_UM=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
set "SDK_UCRT=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"
set "SDK_INC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0"

set PATH=%MSVC_BIN%;%PATH%

cd /d D:\RawrXD\Ship

echo [1/5] Assembling Titan Kernel...
ml64 /c /Zi RawrXD_Titan_Kernel.asm
if errorlevel 1 (
    echo [ERROR] Titan Kernel assembly failed
    goto :error
)

echo [2/5] Linking Titan Kernel DLL...
link /DLL /OUT:RawrXD_Titan_Kernel.dll /SUBSYSTEM:WINDOWS /ENTRY:DllMain /MACHINE:X64 ^
    RawrXD_Titan_Kernel.obj ^
    /LIBPATH:"%MSVC_LIB%" /LIBPATH:"%SDK_UM%" /LIBPATH:"%SDK_UCRT%" ^
    kernel32.lib user32.lib ntdll.lib msvcrt.lib ucrt.lib vcruntime.lib legacy_stdio_definitions.lib
if errorlevel 1 (
    echo [ERROR] Titan Kernel linking failed
    goto :error
)

echo [3/5] Assembling Native Model Bridge...
ml64 /c /Zi RawrXD_NativeModelBridge.asm
if errorlevel 1 (
    echo [ERROR] Native Model Bridge assembly failed
    goto :error
)

echo [4/5] Linking Native Model Bridge DLL...
link /DLL /OUT:RawrXD_NativeModelBridge.dll /SUBSYSTEM:WINDOWS /ENTRY:DllMain /MACHINE:X64 ^
    RawrXD_NativeModelBridge.obj ^
    /LIBPATH:"%MSVC_LIB%" /LIBPATH:"%SDK_UM%" /LIBPATH:"%SDK_UCRT%" ^
    kernel32.lib user32.lib ntdll.lib msvcrt.lib ucrt.lib vcruntime.lib legacy_stdio_definitions.lib
if errorlevel 1 (
    echo [ERROR] Native Model Bridge linking failed
    goto :error
)

echo [5/5] Building Win32 IDE and CLI...
cl /O2 /DNDEBUG /DUNICODE /D_UNICODE /EHsc ^
    /I"%SDK_INC%\um" /I"%SDK_INC%\shared" /I"%SDK_INC%\ucrt" ^
    RawrXD_Win32_IDE.cpp ^
    /link /LIBPATH:"%MSVC_LIB%" /LIBPATH:"%SDK_UM%" /LIBPATH:"%SDK_UCRT%" ^
    user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ^
    /OUT:RawrXD_IDE.exe /SUBSYSTEM:WINDOWS
if errorlevel 1 (
    echo [WARNING] Win32 IDE build failed (continuing...)
)

cl /O2 /DNDEBUG /EHsc ^
    /I"%SDK_INC%\um" /I"%SDK_INC%\shared" /I"%SDK_INC%\ucrt" ^
    RawrXD_CLI_Standalone.cpp ^
    /link /LIBPATH:"%MSVC_LIB%" /LIBPATH:"%SDK_UM%" /LIBPATH:"%SDK_UCRT%" ^
    user32.lib ^
    /OUT:RawrXD_CLI.exe /SUBSYSTEM:CONSOLE
if errorlevel 1 (
    echo [WARNING] CLI build failed (continuing...)
)

echo.
echo ===============================================
echo  Build Complete!
echo ===============================================
echo.
echo Artifacts:
dir /B *.dll *.exe 2>nul | findstr /i "RawrXD"
echo.
goto :end

:error
echo.
echo Build failed with errors.
exit /b 1

:end
endlocal
