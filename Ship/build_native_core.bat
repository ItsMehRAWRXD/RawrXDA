@echo off
setlocal enabledelayedexpansion

REM Manual setup for VS2022 and Windows SDK
set "MSVC_BIN=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
set "MSVC_LIB=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\onecore\x64"
set "SDK_UM_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
set "SDK_UCRT_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"

set "PATH=%MSVC_BIN%;%PATH%"
set "LIB=%MSVC_LIB%;%SDK_UM_LIB%;%SDK_UCRT_LIB%"

REM Check if ml64 and link are in path
where ml64.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ml64.exe not found. Check MSVC_BIN path.
    exit /b 1
)

cd /d D:\RawrXD\Ship

echo ==============================================================================
echo [BUILD] RawrXD Native Core (Bridge + Titan Kernel)
echo ==============================================================================

REM 1. Assemble Native Model Bridge
echo [ASM] Assembling RawrXD_NativeModelBridge.asm...
ml64 /c /Zi /D"PRODUCTION=1" RawrXD_NativeModelBridge.asm
if errorlevel 1 (
    echo [ERROR] Bridge assembly failed
    exit /b 1
)

REM 2. Link Native Model Bridge
echo [LINK] Linking RawrXD_NativeModelBridge.dll...
link /DLL /OUT:RawrXD_NativeModelBridge.dll ^
    /SUBSYSTEM:WINDOWS /ENTRY:DllMain /MACHINE:X64 ^
    RawrXD_NativeModelBridge.obj ^
    kernel32.lib user32.lib ntdll.lib msvcrt.lib
if errorlevel 1 (
    echo [ERROR] Bridge linking failed
    exit /b 1
)

REM 3. Assemble Titan Kernel
echo [ASM] Assembling RawrXD_Titan_Kernel.asm...
ml64 /c /Zi /D"PRODUCTION=1" RawrXD_Titan_Kernel.asm
if errorlevel 1 (
    echo [ERROR] Titan Kernel assembly failed
    exit /b 1
)

REM 4. Link Titan Kernel
echo [LINK] Linking RawrXD_Titan_Kernel.dll...
link /DLL /OUT:RawrXD_Titan_Kernel.dll ^
    /SUBSYSTEM:WINDOWS /ENTRY:DllMain /MACHINE:X64 ^
    RawrXD_Titan_Kernel.obj ^
    kernel32.lib user32.lib ntdll.lib msvcrt.lib
if errorlevel 1 (
    echo [ERROR] Titan Kernel linking failed
    exit /b 1
)

echo ==============================================================================
echo [SUCCESS] Native Core built successfully!
echo ==============================================================================
dir /B RawrXD_NativeModelBridge.dll RawrXD_Titan_Kernel.dll

endlocal
