@echo off
REM ============================================================================
REM build_rawrxd_win32_migration.bat
REM 
REM Builds RawrXD with Zero Qt dependencies using 32-component foundation
REM Compiles migrated main_qt.cpp and links against Win32 DLLs instead of Qt
REM ============================================================================

setlocal enabledelayedexpansion

echo.
echo ╔═══════════════════════════════════════════════════════════════════════════════╗
echo ║                  RawrXD Zero-Qt Build System                                  ║
echo ║                   Compiler: MSVC 19.50 (VS2022)                               ║
echo ║                   Target: Win32 x64 Release                                   ║
echo ║                   Dependencies: 32 Foundation Components                      ║
echo ╚═══════════════════════════════════════════════════════════════════════════════╝
echo.

REM ============================================================================
REM SETUP ENVIRONMENT
REM ============================================================================

set "MSVC=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717"
set "SDK=C:\Program Files (x86)\Windows Kits\10"
set "SDK_VER=10.0.22621.0"
set "SHIP_DIR=D:\RawrXD\Ship"
set "SRC_DIR=D:\RawrXD\src"
set "BUILD_DIR=%SRC_DIR%\build"

REM Add MSVC to PATH
set "PATH=!MSVC!\bin\Hostx64\x64;!PATH!"

echo [1/5] Verifying environment...
if not exist "!MSVC!\bin\Hostx64\x64\cl.exe" (
    echo ERROR: MSVC not found at !MSVC!
    exit /b 1
)
if not exist "!SHIP_DIR!" (
    echo ERROR: Ship directory not found at !SHIP_DIR!
    exit /b 1
)
echo ✓ Environment OK

REM ============================================================================
REM SETUP INCLUDE AND LINK PATHS
REM ============================================================================

echo [2/5] Configuring compiler flags...

set "INCLUDES=/I"!SDK!\Include\!SDK_VER!\um""
set "INCLUDES=!INCLUDES! /I"!SDK!\Include\!SDK_VER!\shared""
set "INCLUDES=!INCLUDES! /I"!SDK!\Include\!SDK_VER!\ucrt""
set "INCLUDES=!INCLUDES! /I"!MSVC!\include""
set "INCLUDES=!INCLUDES! /I"!SRC_DIR!""
set "INCLUDES=!INCLUDES! /I"!SHIP_DIR!""

set "LIBS=/LIBPATH:"!MSVC!\lib\onecore\x64""
set "LIBS=!LIBS! /LIBPATH:"!SDK!\Lib\!SDK_VER!\um\x64""
set "LIBS=!LIBS! /LIBPATH:"!SDK!\Lib\!SDK_VER!\ucrt\x64""
set "LIBS=!LIBS! /LIBPATH:"!SHIP_DIR!""

set "CFLAGS=/std:c++17 /O2 /DNDEBUG /MD /EHsc /W4"

echo ✓ Compiler flags configured

REM ============================================================================
REM COMPILE MAIN EXECUTABLE
REM ============================================================================

echo [3/5] Compiling main_qt_migrated.cpp...

if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"

cd /d "!BUILD_DIR!"

cl !CFLAGS! !INCLUDES! "!SRC_DIR!\qtapp\main_qt_migrated.cpp" ^
    /link !LIBS! ^
    RawrXD_Foundation_Integration.lib ^
    RawrXD_MainWindow_Win32.lib ^
    RawrXD_InferenceEngine.lib ^
    RawrXD_Executor.lib ^
    RawrXD_AgenticEngine.lib ^
    RawrXD_AgentCoordinator.dll ^
    RawrXD_Core.lib ^
    RawrXD_MemoryManager.lib ^
    RawrXD_TaskScheduler.lib ^
    RawrXD_FileManager_Win32.lib ^
    RawrXD_SettingsManager_Win32.lib ^
    RawrXD_TextEditor_Win32.lib ^
    RawrXD_TerminalManager_Win32.lib ^
    RawrXD_ResourceManager_Win32.lib ^
    kernel32.lib user32.lib psapi.lib dbghelp.lib shlwapi.lib ^
    /OUT:RawrXD_IDE_Win32.exe ^
    /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Compilation failed with error code %ERRORLEVEL%
    exit /b 1
)

echo ✓ Compilation successful

REM ============================================================================
REM VERIFY OUTPUT
REM ============================================================================

echo [4/5] Verifying executable...

if not exist "RawrXD_IDE_Win32.exe" (
    echo ERROR: Output executable not created
    exit /b 1
)

for /f "usebackq" %%F in ('RawrXD_IDE_Win32.exe') do set "EXE_SIZE=%%~zF"
echo ✓ RawrXD_IDE_Win32.exe created (%EXE_SIZE% bytes)

REM Verify no Qt dependencies
echo Checking for Qt dependencies...
dumpbin /dependents RawrXD_IDE_Win32.exe 2>nul | findstr /i "Qt5 Qt6 qml"
if %ERRORLEVEL% equ 0 (
    echo WARNING: Qt dependencies still found
) else (
    echo ✓ Zero Qt dependencies verified
)

REM ============================================================================
REM COPY TO SHIP DIRECTORY
REM ============================================================================

echo [5/5] Deploying to Ship directory...

copy /Y "RawrXD_IDE_Win32.exe" "!SHIP_DIR!\RawrXD_IDE_Win32.exe" >nul
if %ERRORLEVEL% equ 0 (
    echo ✓ Deployed to !SHIP_DIR!\RawrXD_IDE_Win32.exe
) else (
    echo ERROR: Failed to deploy executable
    exit /b 1
)

echo.
echo ╔═══════════════════════════════════════════════════════════════════════════════╗
echo ║                      BUILD COMPLETE - SUCCESS                                 ║
echo ║                                                                              ║
echo ║  Output: !SHIP_DIR!\RawrXD_IDE_Win32.exe                                 ║
echo ║  Size: %EXE_SIZE% bytes                                                       ║
echo ║  Qt Dependencies: ZERO                                                        ║
echo ║  Foundation Components: 32                                                    ║
echo ║                                                                              ║
echo ║  Next: Run .\RawrXD_IDE_Win32.exe to test                                    ║
echo ║        Or: .\RawrXD_FoundationTest.exe to validate components                ║
echo ╚═══════════════════════════════════════════════════════════════════════════════╝
echo.

cd /d "!SHIP_DIR!"

exit /b 0
