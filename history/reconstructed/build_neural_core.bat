@echo off
setlocal EnableDelayedExpansion

:: RawrXD Neural Engine Build Script
:: Build configuration: Release-AVX512

:: Search for ml64.exe and link.exe in common VS installation paths
set "ML64="
set "LINKER="

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
if not exist "%VS_PATH%" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
if not exist "%VS_PATH%" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"

for /f "delims=" %%i in ('dir /b /ad /on "%VS_PATH%"') do (
    set "VERSION=%%i"
)

set "BIN_PATH=%VS_PATH%\!VERSION!\bin\Hostx64\x64"
set "ML64=%BIN_PATH%\ml64.exe"
set "LINKER=%BIN_PATH%\link.exe"

if not exist "%ML64%" (
    echo [!] ml64.exe not found at %ML64%
    exit /b 1
)

echo [*] Assembling rawrxd_neural_core.asm...
"%ML64%" /c /nologo /Zi /Zd /W3 /WX /DWIN64 /D_AMD64_ ^
    /I"\masm64\include64" ^
    /Fo"neural_core.obj" ^
    "src\rawrxd_neural_core.asm"

if errorlevel 1 (
    echo [!] Assembly failed
    exit /b 1
)

echo [*] Linking DLL...
"%LINKER%" /DLL /OUT:rawrxd_neural_core.dll ^
    /MACHINE:X64 /LARGEADDRESSAWARE ^
    /SUBSYSTEM:WINDOWS /RELEASE /OPT:REF /OPT:ICF ^
    /STACK:8388608,4194304 ^
    neural_core.obj ^
    kernel32.lib user32.lib shell32.lib comdlg32.lib ^
    /ENTRY:DllMain

if errorlevel 1 (
    echo [!] Linking failed
    exit /b 1
)

echo [+] Build successful: rawrxd_neural_core.dll
echo [+] Exports:
"%LINKER%" /exports rawrxd_neural_core.dll

pause
