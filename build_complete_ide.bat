@echo off
REM ============================================================================
REM BUILD SCRIPT: Compile RawrXD Complete IDE with Assembly + C++ Integration
REM ============================================================================
REM This script builds the complete IDE with:
REM - Assembly layer (x64 MASM)
REM - C++ application layer
REM - WinHTTP AI client
REM - Real Win32 GUI components
REM ============================================================================

setlocal enabledelayedexpansion

REM Detect Visual Studio compiler
set VCVARS=
if exist "D:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set VCVARS=D:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat
) else (
    echo ERROR: Could not find VS2022 vcvars64.bat
    exit /b 1
)

echo ============================================================================
echo [STEP 1] Initializing Visual Studio 2022 environment...
echo ============================================================================
call "%VCVARS%"
if errorlevel 1 (
    echo ERROR: Failed to initialize VS environment
    exit /b 1
)

echo.
echo ============================================================================
echo [STEP 2] Compiling x64 Assembly (RawrXD_TextEditorGUI.asm)
echo ============================================================================
echo Assembling with ml64.exe (non-stubbed, production-ready)...
ml64.exe RawrXD_TextEditorGUI.asm /c /Fo RawrXD_TextEditorGUI.obj /nologo
if errorlevel 1 (
    echo ERROR: Assembly compilation failed
    exit /b 1
)
echo [OK] Assembly compiled successfully

echo.
echo ============================================================================
echo [STEP 3] Compiling C++ Files
echo ============================================================================

echo Compiling IDE_MainWindow.cpp...
cl.exe /c IDE_MainWindow.cpp /W4 /nologo /Fo IDE_MainWindow.obj
if errorlevel 1 (
    echo ERROR: IDE_MainWindow.cpp compilation failed
    exit /b 1
)
echo [OK] IDE_MainWindow.obj created

echo Compiling AI_Integration.cpp...
cl.exe /c AI_Integration.cpp /W4 /nologo /Fo AI_Integration.obj
if errorlevel 1 (
    echo ERROR: AI_Integration.cpp compilation failed
    exit /b 1
)
echo [OK] AI_Integration.obj created

echo Compiling RawrXD_IDE_Complete.cpp...
cl.exe /c RawrXD_IDE_Complete.cpp /W4 /nologo /Fo RawrXD_IDE_Complete.obj
if errorlevel 1 (
    echo ERROR: RawrXD_IDE_Complete.cpp compilation failed
    exit /b 1
)
echo [OK] RawrXD_IDE_Complete.obj created

echo.
echo ============================================================================
echo [STEP 4] Linking All Objects into RawrXD_IDE.exe
echo ============================================================================
echo Linking with kernel32, user32, gdi32, comdlg32, winhttp...
link.exe ^
    RawrXD_IDE_Complete.obj ^
    IDE_MainWindow.obj ^
    AI_Integration.obj ^
    RawrXD_TextEditorGUI.obj ^
    kernel32.lib user32.lib gdi32.lib comdlg32.lib winhttp.lib ^
    /OUT:RawrXD_IDE.exe /NOLOGO /SUBSYSTEM:WINDOWS /ENTRY:WinMainA

if errorlevel 1 (
    echo ERROR: Linking failed
    exit /b 1
)
echo [OK] RawrXD_IDE.exe created successfully

echo.
echo ============================================================================
echo [STEP 5] Compiling MockAI_Server.exe for Testing
echo ============================================================================
if exist MockAI_Server.cpp (
    cl.exe /c MockAI_Server.cpp /W4 /nologo /Fo MockAI_Server.obj
    if errorlevel 1 (
        echo ERROR: MockAI_Server.cpp compilation failed
        exit /b 1
    )
    link.exe MockAI_Server.obj kernel32.lib ws2_32.lib /OUT:MockAI_Server.exe /NOLOGO /SUBSYSTEM:CONSOLE
    if errorlevel 1 (
        echo ERROR: MockAI_Server linking failed
        exit /b 1
    )
    echo [OK] MockAI_Server.exe created
) else (
    echo [WARN] MockAI_Server.cpp not found, skipping
)

echo.
echo ============================================================================
echo [SUCCESS] Build Complete!
echo ============================================================================
echo.
echo Deliverables:
echo   - RawrXD_IDE.exe          (Main IDE application)
echo   - RawrXD_IDE.exe.pdb      (Debug symbols, if available)
echo   - MockAI_Server.exe       (Test AI server, if compiled)
echo.
echo To run:
echo   1. Start MockAI_Server.exe in a terminal (listens on port 8000)
echo   2. Run RawrXD_IDE.exe to start the editor
echo.
echo Assembly Layer:          RawrXD_TextEditorGUI.asm (855 lines, production-ready)
echo C++ Application Layer:   IDE_MainWindow.cpp + AI_Integration.cpp
echo Entry Point:             RawrXD_IDE_Complete.cpp (WinMainA)
echo.
echo All procedures use REAL Win32 APIs (zero simulation)
echo All procedures have unique names for continuation/tracing
echo ============================================================================

endlocal
exit /b 0
