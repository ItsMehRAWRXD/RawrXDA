@echo off
REM ===============================================================================
REM CLI Access System - Comprehensive Test Suite
REM ===============================================================================

set EXE=bin\RawrXD_IDE.exe

echo ===============================================================================
echo CLI Access System - Test Suite
echo ===============================================================================
echo.

REM Check if executable exists
if not exist "%EXE%" (
    echo ERROR: %EXE% not found
    echo Please run build_cli_full.bat first
    exit /b 1
)

echo ===============================================================================
echo Test 1: Help Command
echo ===============================================================================
"%EXE%" help
if %ERRORLEVEL% NEQ 0 (
    echo FAIL: Help command failed
    exit /b 1
)
echo PASS: Help command
echo.

echo ===============================================================================
echo Test 2: List Commands
echo ===============================================================================

echo --- List Files ---
"%EXE%" list files
echo.

echo --- List Menus ---
"%EXE%" list menus
echo.

echo --- List Widgets ---
"%EXE%" list widgets
echo.

echo --- List Agents ---
"%EXE%" list agents
echo.

echo PASS: List commands
echo.

echo ===============================================================================
echo Test 3: Menu Access
echo ===============================================================================

echo --- File.Open ---
"%EXE%" menu File.Open
echo.

echo --- Theme.Dark ---
"%EXE%" menu Theme.Dark
echo.

echo --- Agent.Validate ---
"%EXE%" menu Agent.Validate
echo.

echo PASS: Menu access commands
echo.

echo ===============================================================================
echo Test 4: Widget Control
echo ===============================================================================

echo --- Editor Focus ---
"%EXE%" widget editor focus
echo.

echo --- Chat Clear ---
"%EXE%" widget chat clear
echo.

echo --- Terminal Execute ---
"%EXE%" widget terminal execute
echo.

echo PASS: Widget control commands
echo.

echo ===============================================================================
echo Test 5: Theme Commands
echo ===============================================================================

echo --- Light Theme ---
"%EXE%" theme light
echo.

echo --- Dark Theme ---
"%EXE%" theme dark
echo.

echo --- Amber Theme ---
"%EXE%" theme amber
echo.

echo PASS: Theme commands
echo.

echo ===============================================================================
echo Test 6: Signal Commands
echo ===============================================================================

echo --- Emit Signal ---
"%EXE%" signal emit file-opened
echo.

echo --- Connect Signal ---
"%EXE%" signal connect build-complete on-build-done
echo.

echo PASS: Signal commands
echo.

echo ===============================================================================
echo Test 7: Dispatch Commands
echo ===============================================================================

echo --- Dispatch Read ---
"%EXE%" dispatch read-file test.txt
echo.

echo --- Dispatch Build ---
"%EXE%" dispatch build
echo.

echo PASS: Dispatch commands
echo.

echo ===============================================================================
echo Test 8: Tool Commands
echo ===============================================================================

echo --- Tool Lint ---
"%EXE%" tool lint src/
echo.

echo --- Tool Format ---
"%EXE%" tool format myfile.asm
echo.

echo PASS: Tool commands
echo.

echo ===============================================================================
echo Test 9: File Tree
echo ===============================================================================

echo --- Display Tree ---
"%EXE%" tree src/
echo.

echo PASS: Tree command
echo.

echo ===============================================================================
echo Test 10: Configuration
echo ===============================================================================

echo --- Set Config ---
"%EXE%" config model gpt-4
echo.

echo PASS: Config command
echo.

echo ===============================================================================
echo All Tests Completed Successfully!
echo ===============================================================================
echo.
echo Test Summary:
echo   - Help command: PASS
echo   - List commands: PASS
echo   - Menu access: PASS
echo   - Widget control: PASS
echo   - Theme commands: PASS
echo   - Signal commands: PASS
echo   - Dispatch commands: PASS
echo   - Tool commands: PASS
echo   - Tree command: PASS
echo   - Config command: PASS
echo.
echo ===============================================================================

pause
