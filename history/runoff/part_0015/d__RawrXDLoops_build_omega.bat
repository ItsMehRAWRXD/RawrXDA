@echo off
REM ============================================================
REM Underground King - Omega Build Script
REM Pure MASM64 Professional DAW
REM ============================================================

echo.
echo ============================================================
echo  UNDERGROUND KING - OMEGA BUILD
echo  Pure MASM64 Professional DAW
echo ============================================================
echo.

REM Setup paths
set MASM_PATH=C:\masm32
set ML64_PATH=%MASM_PATH%\bin\ml64.exe
set LINK_PATH=%MASM_PATH%\bin\link.exe

REM Check for ml64 in MASM32
if exist "%ML64_PATH%" (
    echo [+] Found ml64.exe in MASM32
) else (
    REM Try Visual Studio paths
    for %%P in (
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\ml64.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\ml64.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\ml64.exe"
        "C:\VS2022Enterprise\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\ml64.exe"
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64\ml64.exe"
    ) do (
        if exist %%P (
            set ML64_PATH=%%~P
            echo [+] Found ml64.exe at %%~P
            goto :found_ml64
        )
    )
    
    REM Try to find any ml64.exe
    echo [!] Searching for ml64.exe...
    for /f "delims=" %%i in ('where ml64.exe 2^>nul') do (
        set ML64_PATH=%%i
        echo [+] Found ml64.exe at %%i
        goto :found_ml64
    )
    
    echo [!] ERROR: ml64.exe not found!
    echo [!] Install Visual Studio with C++ tools or MASM64
    pause
    exit /b 1
)

:found_ml64

REM Find Windows SDK lib path
set SDK_LIB=
for %%P in (
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22000.0\um\x64"
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.19041.0\um\x64"
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.18362.0\um\x64"
) do (
    if exist %%P (
        set SDK_LIB=%%~P
        echo [+] Found Windows SDK libs at %%~P
        goto :found_sdk
    )
)

:found_sdk

REM Find VC runtime libs
set VC_LIB=
for %%P in (
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.38.33130\lib\x64"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.38.33130\lib\x64"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\lib\x64"
    "C:\VS2022Enterprise\VC\Tools\MSVC\14.38.33130\lib\x64"
    "C:\masm32\lib"
) do (
    if exist %%P (
        set VC_LIB=%%~P
        echo [+] Found VC libs at %%~P
        goto :found_vc
    )
)

:found_vc

cd /d "%~dp0"
echo.
echo [*] Building omega.asm...
echo.

REM Assemble
echo [1/2] Assembling...
"%ML64_PATH%" /c /Cp /Zi /Fl"omega.lst" omega.asm
if errorlevel 1 (
    echo.
    echo [!] Assembly FAILED!
    echo [!] Check omega.lst for errors
    pause
    exit /b 1
)

echo [+] Assembly successful!

REM Link
echo [2/2] Linking...
set LIBS=kernel32.lib user32.lib gdi32.lib winmm.lib ole32.lib

REM Build link command with available lib paths
set LINK_CMD=link /SUBSYSTEM:WINDOWS /ENTRY:WinMain /OUT:omega.exe /DEBUG omega.obj %LIBS%

if defined SDK_LIB (
    set LINK_CMD=%LINK_CMD% /LIBPATH:"%SDK_LIB%"
)
if defined VC_LIB (
    set LINK_CMD=%LINK_CMD% /LIBPATH:"%VC_LIB%"
)

echo Running: %LINK_CMD%
%LINK_CMD%

if errorlevel 1 (
    echo.
    echo [!] Linking FAILED!
    echo [!] Make sure Windows SDK is installed
    
    REM Try alternate link method
    echo [*] Trying alternate link method...
    link /SUBSYSTEM:WINDOWS /ENTRY:WinMain /OUT:omega.exe omega.obj ^
        kernel32.lib user32.lib gdi32.lib winmm.lib ole32.lib ^
        /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" ^
        /LIBPATH:"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.38.33130\lib\x64"
    
    if errorlevel 1 (
        pause
        exit /b 1
    )
)

echo.
echo ============================================================
echo  BUILD SUCCESSFUL!
echo ============================================================
echo.
echo  Output: omega.exe
echo.
echo  Controls:
echo    Space   = Play/Stop
echo    1-5     = Select Panel
echo    G       = Cycle Genre (Techno/Acid/Ambient/HipHop/House)
echo    R       = Reload Pattern
echo    L       = Load AI Model
echo    Click   = Toggle pattern steps
echo.
echo ============================================================

REM Check if user wants to run
set /p RUN="Run omega.exe now? (Y/N): "
if /i "%RUN%"=="Y" (
    start "" omega.exe
)

pause
