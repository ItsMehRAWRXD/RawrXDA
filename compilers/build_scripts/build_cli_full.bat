@echo off
REM ===============================================================================
REM RawrXD IDE - Full Build Script with CLI Access System
REM ===============================================================================

echo ===============================================================================
echo Building RawrXD IDE with CLI Access System
echo ===============================================================================

REM Set paths
set MASM_DIR=C:\masm32\bin
set SRC_DIR=D:\RawrXD-production-lazy-init\src\masm
set OBJ_DIR=D:\RawrXD-production-lazy-init\obj
set BIN_DIR=D:\RawrXD-production-lazy-init\bin

REM Create directories if they don't exist
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

echo.
echo ===============================================================================
echo Step 1: Building CLI Access System
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\cli_access_system.obj" "%SRC_DIR%\cli_access_system.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CLI Access System compilation failed
    exit /b 1
)
echo CLI Access System compiled successfully

echo.
echo ===============================================================================
echo Step 2: Building UI Extended Stubs
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\ui_extended_stubs.obj" "%SRC_DIR%\ui_extended_stubs.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: UI Extended Stubs compilation failed
    exit /b 1
)
echo UI Extended Stubs compiled successfully

echo.
echo ===============================================================================
echo Step 3: Building Universal Dispatcher
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\universal_dispatcher.obj" "%SRC_DIR%\universal_dispatcher.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Universal Dispatcher compilation failed
    exit /b 1
)
echo Universal Dispatcher compiled successfully

echo.
echo ===============================================================================
echo Step 4: Building UI MASM
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\ui_masm.obj" "%SRC_DIR%\ui_masm.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: UI MASM compilation failed
    exit /b 1
)
echo UI MASM compiled successfully

echo.
echo ===============================================================================
echo Step 5: Building Agentic MASM
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\agentic_masm.obj" "%SRC_DIR%\agentic_masm.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Agentic MASM compilation failed
    exit /b 1
)
echo Agentic MASM compiled successfully

echo.
echo ===============================================================================
echo Step 6: Building Advanced Planning Engine
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\advanced_planning_engine.obj" "%SRC_DIR%\advanced_planning_engine.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Advanced Planning Engine compilation failed
    exit /b 1
)
echo Advanced Planning Engine compiled successfully

echo.
echo ===============================================================================
echo Step 7: Building REST API Server
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\rest_api_server_full.obj" "%SRC_DIR%\rest_api_server_full.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: REST API Server compilation failed
    exit /b 1
)
echo REST API Server compiled successfully

echo.
echo ===============================================================================
echo Step 8: Building Distributed Tracer
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\distributed_tracer.obj" "%SRC_DIR%\distributed_tracer.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Distributed Tracer compilation failed
    exit /b 1
)
echo Distributed Tracer compiled successfully

echo.
echo ===============================================================================
echo Step 9: Building Enterprise Common
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\enterprise_common.obj" "%SRC_DIR%\enterprise_common.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Enterprise Common compilation failed
    exit /b 1
)
echo Enterprise Common compiled successfully

echo.
echo ===============================================================================
echo Step 10: Building ASM Log
echo ===============================================================================
"%MASM_DIR%\ml64" /c /Fo"%OBJ_DIR%\asm_log.obj" "%SRC_DIR%\asm_log.asm"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: ASM Log compilation failed
    exit /b 1
)
echo ASM Log compiled successfully

echo.
echo ===============================================================================
echo Step 11: Linking All Modules
echo ===============================================================================
link /SUBSYSTEM:CONSOLE /ENTRY:WinMainCRTStartup /OUT:"%BIN_DIR%\RawrXD_IDE.exe" ^
    "%OBJ_DIR%\cli_access_system.obj" ^
    "%OBJ_DIR%\ui_extended_stubs.obj" ^
    "%OBJ_DIR%\universal_dispatcher.obj" ^
    "%OBJ_DIR%\ui_masm.obj" ^
    "%OBJ_DIR%\agentic_masm.obj" ^
    "%OBJ_DIR%\advanced_planning_engine.obj" ^
    "%OBJ_DIR%\rest_api_server_full.obj" ^
    "%OBJ_DIR%\distributed_tracer.obj" ^
    "%OBJ_DIR%\enterprise_common.obj" ^
    "%OBJ_DIR%\asm_log.obj" ^
    kernel32.lib user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib uuid.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Linking failed
    exit /b 1
)

echo.
echo ===============================================================================
echo Build Complete!
echo ===============================================================================
echo Executable: %BIN_DIR%\RawrXD_IDE.exe
echo.
echo CLI Commands Available:
echo   - help           : Display help information
echo   - open ^<file^>    : Open file
echo   - save [file]    : Save file
echo   - build [config] : Build project
echo   - run [args]     : Run project
echo   - agent ^<action^> : Agent command
echo   - chat ^<message^> : Chat command
echo   - menu ^<id^>      : Execute menu action
echo   - widget ^<name^>  : Control widget
echo   - signal ^<emit^>  : Emit signal
echo   - list ^<type^>    : List resources
echo   - tree [path]    : Display file tree
echo.
echo ===============================================================================

pause
