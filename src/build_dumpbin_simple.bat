@echo off
echo ========================================
echo DumpBin Simple Build Script
echo ========================================
echo.

REM Set paths
set "MASM32_PATH=C:\masm32"
set "VS_PATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx86\x86"

REM Check if MASM32 exists
if not exist "%MASM32_PATH%\bin\ml.exe" (
    echo [-] ERROR: MASM32 not found at %MASM32_PATH%
    echo [-] Please install MASM32 or update the path
    exit /b 1
)

REM Check if Visual Studio Build Tools exist
if not exist "%VS_PATH%\link.exe" (
    echo [-] ERROR: Visual Studio Build Tools not found at %VS_PATH%
    echo [-] Please install Visual Studio Build Tools or update the path
    exit /b 1
)

REM Set environment
set "PATH=%MASM32_PATH%\bin;%VS_PATH%;%PATH%"

echo [+] Building DumpBin Simple...
echo.

REM Assemble
echo [+] Assembling dumpbin_simple.asm...
ml.exe /c /coff /nologo /I"%MASM32_PATH%\include" dumpbin_simple.asm
if errorlevel 1 (
    echo [-] Assembly failed
    exit /b 1
)

echo [+] Assembly successful
echo.

REM Link
echo [+] Linking dumpbin_simple.obj...
link.exe /SUBSYSTEM:CONSOLE /LIBPATH:"%MASM32_PATH%\lib" /LIBPATH:"%VS_PATH%\..\..\..\lib\x86" dumpbin_simple.obj kernel32.lib user32.lib
if errorlevel 1 (
    echo [-] Linking failed
    exit /b 1
)

echo [+] Linking successful
echo.

REM Check if executable was created
if exist "dumpbin_simple.exe" (
    echo [+] Build completed successfully!
    echo [+] Executable: dumpbin_simple.exe
    echo.
    echo [+] File size:
    dir dumpbin_simple.exe
    echo.
    echo [+] Testing DumpBin Simple...
    echo.
    dumpbin_simple.exe dumpbin_simple.exe
) else (
    echo [-] Build failed: dumpbin_simple.exe not created
    exit /b 1
)

endlocal
