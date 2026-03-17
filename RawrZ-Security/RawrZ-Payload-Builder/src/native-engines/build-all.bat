@echo off
echo 🔥 Building All RawrZ Native Engines 🔥
echo.

REM Build C++ Engine
echo [1/3] Building C++ Engine...
where cl >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    cl /EHsc /O2 /MT cpp-stub-generator.cpp /Fe:rawrz-cpp.exe >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        echo [SUCCESS] C++ engine: rawrz-cpp.exe
        del *.obj 2>nul
    ) else (
        echo [ERROR] C++ compilation failed
    )
) else (
    echo [SKIP] Visual Studio compiler not found
)

REM Build Rust Engine
echo [2/3] Building Rust Engine...
cd rust-engine
where cargo >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    cargo build --release >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        copy target\release\rawrz-rust.exe ..\rawrz-rust.exe >nul
        echo [SUCCESS] Rust engine: rawrz-rust.exe
    ) else (
        echo [ERROR] Rust compilation failed
    )
) else (
    echo [SKIP] Rust compiler not found
)
cd ..

REM Build Go Engine
echo [3/3] Building Go Engine...
cd go-engine
where go >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    go build -o ..\rawrz-go.exe main.go >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        echo [SUCCESS] Go engine: rawrz-go.exe
    ) else (
        echo [ERROR] Go compilation failed
    )
) else (
    echo [SKIP] Go compiler not found
)
cd ..

echo.
echo === RawrZ Native Engines Built ===
if exist rawrz-cpp.exe echo [✓] C++ Engine Ready
if exist rawrz-rust.exe echo [✓] Rust Engine Ready  
if exist rawrz-go.exe echo [✓] Go Engine Ready
echo.

echo Testing engines...
if exist rawrz-cpp.exe (
    echo [C++] Testing...
    rawrz-cpp.exe list
    echo.
)

if exist rawrz-go.exe (
    echo [Go] Testing...
    rawrz-go.exe list
    echo.
)

echo [INFO] Usage Examples:
echo   rawrz-cpp.exe toggle http-bot-generator
echo   rawrz-go.exe exec beaconism
echo   rawrz-rust.exe stub calc.exe cpp aes-256-gcm --anti-vm
echo.
pause