@echo off
REM ============================================================================
REM Smoke Test Build Script - Slash Commands with Real Model Loaders
REM ============================================================================

setlocal enabledelayedexpansion

echo.
echo ============================================================
echo   RawrXD Slash Command Smoke Test - Build Script
echo ============================================================
echo.

REM Configuration
set "RAWRXD_ROOT=d:\rawrxd"
set "SRC_DIR=%RAWRXD_ROOT%\src"
set "BUILD_DIR=%RAWRXD_ROOT%\build\smoke_test"
set "OUTPUT_DIR=%RAWRXD_ROOT%\bin\smoke_test"

REM Compiler settings
set "CXX=cl"
set "CXXFLAGS=/std:c++20 /EHsc /W3 /O2 /DNDEBUG /DWIN32 /D_WINDOWS"
set "INCLUDES=/I"%SRC_DIR%" /I"%SRC_DIR%\win32app" /I"%SRC_DIR%\cli" /I"%SRC_DIR%\ggml" /I"%SRC_DIR%\inference""
set "LIBS=user32.lib kernel32.lib"

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo [1/4] Compiling smoke test...
cd /d "%BUILD_DIR%"

REM Compile smoke test
%CXX% %CXXFLAGS% %INCLUDES% ^
    "%SRC_DIR%\test\slash_command_smoke_test.cpp" ^
    "%SRC_DIR%\cli\CLI_SlashRouter.cpp" ^
    "%SRC_DIR%\gguf_loader.cpp" ^
    "%SRC_DIR%\streaming_gguf_loader.cpp" ^
    "%SRC_DIR%\cpu_inference_engine.cpp" ^
    "%SRC_DIR%\win32app\Win32IDE_SlashRouter.cpp" ^
    "%SRC_DIR%\win32app\Win32IDE_KVCacheCleanup.cpp" ^
    /c /Fo"smoke_test.obj" 2>&1

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Compilation failed!
    goto :error
)

echo [2/4] Linking...
%CXX% smoke_test.obj %LIBS% /Fe:"%OUTPUT_DIR%\slash_command_smoke_test.exe" /link /SUBSYSTEM:CONSOLE

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Linking failed!
    goto :error
)

echo [3/4] Copying dependencies...
if exist "%RAWRXD_ROOT%\bin\*.dll" (
    copy /Y "%RAWRXD_ROOT%\bin\*.dll" "%OUTPUT_DIR%\" >nul 2>&1
)

echo [4/4] Build complete!
echo.
echo Executable: %OUTPUT_DIR%\slash_command_smoke_test.exe
echo.

REM Check for model
set "MODEL_PATH="
if exist "d:\codestral22b.gguf" (
    set "MODEL_PATH=d:\codestral22b.gguf"
) else if exist "f:\models\codestral22b.gguf" (
    set "MODEL_PATH=f:\models\codestral22b.gguf"
) else if exist "g:\models\codestral22b.gguf" (
    set "MODEL_PATH=g:\models\codestral22b.gguf"
)

echo ============================================================
echo   Running Smoke Test
echo ============================================================
echo.

if defined MODEL_PATH (
    echo Using model: %MODEL_PATH%
    echo.
    "%OUTPUT_DIR%\slash_command_smoke_test.exe" --model "%MODEL_PATH%" --verbose
) else (
    echo No model found - running without model
    echo.
    "%OUTPUT_DIR%\slash_command_smoke_test.exe" --verbose
)

echo.
echo ============================================================
echo   Smoke Test Complete
echo ============================================================
echo.

goto :end

:error
echo.
echo [ERROR] Build failed!
exit /b 1

:end
endlocal
exit /b 0