@echo off
setlocal enabledelayedexpansion
pushd D:\RawrXD\Ship

:: VS 2022 Path
set "VS_PATH=C:\VS2022Enterprise"
set "ML64=%VS_PATH%\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set "LINK=%VS_PATH%\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"

:: SDK Paths
set "UCRT_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
set "UM_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"

echo [1/2] Assembling RawrXD_NativeModelBridge_v2_FIXED.asm...
"%ML64%" /c /Zi /Cp /nologo /Fo"RawrXD_NativeModelBridge_v2_FIXED.obj" "D:\RawrXD\Ship\RawrXD_NativeModelBridge_v2_FIXED.asm"
if %errorlevel% neq 0 (
    echo Error during assembly.
    popd
    exit /b %errorlevel%
)

echo [2/2] Linking RawrXD_NativeModelBridge_v2_FIXED.dll...
"%LINK%" /DLL /NOENTRY /MACHINE:X64 /DEBUG ^
    /LIBPATH:"%UCRT_PATH%" ^
    /LIBPATH:"%UM_PATH%" ^
    /LIBPATH:"%VS_PATH%\VC\Tools\MSVC\14.50.35717\lib\x64" ^
    "D:\RawrXD\Ship\RawrXD_NativeModelBridge_v2_FIXED.obj" ^
    kernel32.lib user32.lib ucrt.lib vcruntime.lib msvcrt.lib legacy_stdio_definitions.lib ^
    /OUT:"D:\RawrXD\Ship\RawrXD_NativeModelBridge_v2_FIXED.dll"

if %errorlevel% equ 0 (
    echo Build Succeeded: RawrXD_NativeModelBridge_v2_FIXED.dll
) else (
    echo Linker Error.
)
popd
