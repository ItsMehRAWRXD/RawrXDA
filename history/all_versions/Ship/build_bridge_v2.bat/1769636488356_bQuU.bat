@echo off
setlocal enabledelayedexpansion

:: VS 2022 Path
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
set "ML64=%VS_PATH%\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\ml64.exe"
set "LINK=%VS_PATH%\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\link.exe"

:: SDK Paths
set "UCRT_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"
set "UM_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

echo [1/2] Assembling RawrXD_NativeModelBridge_v2_FIXED.asm...
"%ML64%" /c /Zi /Cp /nologo /Fo"RawrXD_NativeModelBridge_v2_FIXED.obj" "RawrXD_NativeModelBridge_v2_FIXED.asm"
if %errorlevel% neq 0 (
    echo Error during assembly.
    exit /b %errorlevel%
)

echo [2/2] Linking RawrXD_NativeModelBridge_v2_FIXED.dll...
"%LINK%" /DLL /NOENTRY /MACHINE:X64 /DEBUG ^
    /LIBPATH:"%UCRT_PATH%" ^
    /LIBPATH:"%UM_PATH%" ^
    /LIBPATH:"%VS_PATH%\VC\Tools\MSVC\14.42.34433\lib\x64" ^
    "RawrXD_NativeModelBridge_v2_FIXED.obj" ^
    kernel32.lib user32.lib ucrt.lib vcruntime.lib msvcrt.lib legacy_stdio_definitions.lib ^
    /OUT:"RawrXD_NativeModelBridge_v2_FIXED.dll"

if %errorlevel% equ 0 (
    echo Build Succeeded: RawrXD_NativeModelBridge_v2_FIXED.dll
) else (
    echo Linker Error.
)
