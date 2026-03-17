@echo off
echo 🔥 Building RawrZ Native Engines 🔥

REM Check for compiler
where cl >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Visual Studio compiler not found!
    echo Please run from Visual Studio Developer Command Prompt
    pause
    exit /b 1
)

echo [INFO] Compiling C++ Stub Generator...
cl /EHsc /O2 /MT cpp-stub-generator.cpp /Fe:rawrz-native.exe
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Native engine compiled: rawrz-native.exe
) else (
    echo [ERROR] Compilation failed!
    pause
    exit /b 1
)

REM Clean up object files
del *.obj 2>nul

echo.
echo [INFO] Testing native engine...
rawrz-native.exe list

echo.
echo [SUCCESS] RawrZ Native Engine System ready!
echo.
echo Usage Examples:
echo   rawrz-native.exe list
echo   rawrz-native.exe toggle stub-generator
echo   rawrz-native.exe exec http-bot-generator
echo   rawrz-native.exe stub calc.exe cpp aes-256-gcm --anti-vm --anti-debug
echo.
pause