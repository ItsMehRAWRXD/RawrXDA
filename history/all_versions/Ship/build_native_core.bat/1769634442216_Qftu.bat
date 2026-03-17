@echo off
setlocal enabledelayedexpansion

REM Absolute paths for VS2022 Build Tools
set "ML64=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set "LINK=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"

REM Check if tools exist
if not exist "%ML64%" (
    echo [ERROR] ml64.exe not found at %ML64%
    exit /b 1
)
if not exist "%LINK%" (
    echo [ERROR] link.exe not found at %LINK%
    exit /b 1
)

cd /d D:\RawrXD\Ship

echo ==============================================================================
echo [BUILD] RawrXD Native Core (Bridge + Titan Kernel)
echo ==============================================================================

REM 1. Assemble Native Model Bridge
echo [ASM] Assembling RawrXD_NativeModelBridge.asm...
"%ML64%" /c /Zi /D"PRODUCTION=1" RawrXD_NativeModelBridge.asm
if errorlevel 1 (
    echo [ERROR] Bridge assembly failed
    exit /b 1
)

REM 2. Link Native Model Bridge
echo [LINK] Linking RawrXD_NativeModelBridge.dll...
"%LINK%" /DLL /OUT:RawrXD_NativeModelBridge.dll ^
    /SUBSYSTEM:WINDOWS /ENTRY:DllMain /MACHINE:X64 ^
    RawrXD_NativeModelBridge.obj ^
    kernel32.lib user32.lib ntdll.lib msvcrt.lib
if errorlevel 1 (
    echo [ERROR] Bridge linking failed
    exit /b 1
)

REM 3. Assemble Titan Kernel
echo [ASM] Assembling RawrXD_Titan_Kernel.asm...
"%ML64%" /c /Zi /D"PRODUCTION=1" RawrXD_Titan_Kernel.asm
if errorlevel 1 (
    echo [ERROR] Titan Kernel assembly failed
    exit /b 1
)

REM 4. Link Titan Kernel
echo [LINK] Linking RawrXD_Titan_Kernel.dll...
"%LINK%" /DLL /OUT:RawrXD_Titan_Kernel.dll ^
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
