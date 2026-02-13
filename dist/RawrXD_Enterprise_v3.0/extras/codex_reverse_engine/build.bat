@echo off
echo ============================================
echo CODEX REVERSE ENGINE ULTIMATE v7.0
echo Professional PE Analysis & Build System
echo ============================================
echo.

REM Check for MASM64 in common locations
set "ML64_PATH="
if exist "C:\masm64\bin64\ml64.exe" set "ML64_PATH=C:\masm64\bin64"
if exist "D:\masm64\bin64\ml64.exe" set "ML64_PATH=D:\masm64\bin64"
if exist "E:\masm64\bin64\ml64.exe" set "ML64_PATH=E:\masm64\bin64"
if exist "C:\Program Files\MASM64\bin64\ml64.exe" set "ML64_PATH=C:\Program Files\MASM64\bin64"
if exist "D:\Program Files\MASM64\bin64\ml64.exe" set "ML64_PATH=D:\Program Files\MASM64\bin64"
if exist "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe" set "ML64_PATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe" set "ML64_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"

if "%ML64_PATH%"=="" (
    echo ERROR: MASM64 not found!
    echo.
    echo Please install MASM64 or update the path.
    echo.
    echo Common locations:
    echo   C:\masm64\bin64\
    echo   C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\
    echo   C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\
    echo.
    echo You can download MASM64 from:
    echo   https://www.masm32.com/
    exit /b 1
)

echo Found MASM64 at: %ML64_PATH%
echo.

echo [1/3] Assembling CodexUltimate.asm...
"%ML64_PATH%\ml64.exe" CodexUltimate.asm /c /FoCodexUltimate.obj
if errorlevel 1 (
    echo ERROR: Assembly failed!
    exit /b 1
)
echo SUCCESS: Assembly completed.
echo.

echo [2/3] Linking CodexUltimate.obj...
"%ML64_PATH%\link.exe" CodexUltimate.obj /subsystem:console /entry:main /out:CodexUltimate.exe
if errorlevel 1 (
    echo ERROR: Linking failed!
    exit /b 1
)
echo SUCCESS: Linking completed.
echo.

echo [3/3] Verifying build...
if exist CodexUltimate.exe (
    echo SUCCESS: CodexUltimate.exe created successfully!
    echo.
    dir CodexUltimate.exe
    echo.
    echo Build completed at %TIME% on %DATE%
    echo.
    echo To run the tool:
    echo   CodexUltimate.exe
    echo.
    echo To analyze a directory:
    echo   1. Select option 2 (Directory Mode)
    echo   2. Input: C:\Path\To\Target
    echo   3. Output: C:\Reversed\Output
    echo   4. Project: YourProjectName
) else (
    echo ERROR: CodexUltimate.exe not found!
    exit /b 1
)

echo ============================================
echo BUILD COMPLETE
echo ============================================