@echo off
echo ========================================
echo Systems Engineering IDE - Clean Build
echo Using Portable Toolchains (No VS Required)
echo ========================================
echo.

REM Check for portable toolchains
if not exist "portable-toolchains" (
    echo Error: portable-toolchains directory not found
    echo Please ensure your portable toolchain is in the current directory
    pause
    exit /b 1
)

REM Find GCC and Clang
set GCC_PATH=
set CLANG_PATH=

for /d %%i in (portable-toolchains\*) do (
    if exist "%%i\bin\gcc.exe" (
        set GCC_PATH=%%i\bin\gcc.exe
        echo Found GCC: !GCC_PATH!
    )
    if exist "%%i\bin\clang.exe" (
        set CLANG_PATH=%%i\bin\clang.exe
        echo Found Clang: !CLANG_PATH!
    )
)

if "%GCC_PATH%"=="" if "%CLANG_PATH%"=="" (
    echo Error: No suitable compiler found in portable toolchains
    pause
    exit /b 1
)

echo.
echo Found portable toolchains:
if not "%GCC_PATH%"=="" echo - GCC: %GCC_PATH%
if not "%CLANG_PATH%"=="" echo - Clang: %CLANG_PATH%
echo.

REM Try GCC first
if not "%GCC_PATH%"=="" (
    echo [1/3] Compiling with Portable GCC ^(C++ with inline assembly^)...
    "%GCC_PATH%" -std=c++20 -O3 -march=native -flto -ffast-math ^
        -msse4.2 -mavx2 -mfma -fopenmp ^
        -Wno-unused-variable -Wno-unused-function ^
        systems-ide-core-clean.cpp ide-main-clean.cpp ^
        -o systems-ide.exe ^
        -static-libgcc -static-libstdc++ ^
        2>gcc_errors.txt
    
    if !ERRORLEVEL! EQU 0 (
        echo ✓ GCC compilation successful: systems-ide.exe
        goto test_exe
    ) else (
        echo ✗ GCC compilation failed, checking errors...
        if exist gcc_errors.txt type gcc_errors.txt
        echo.
    )
)

REM Try Clang if GCC failed
if not "%CLANG_PATH%"=="" (
    echo [2/3] Trying Clang compilation...
    "%CLANG_PATH%" -std=c++20 -O3 -march=native -flto -ffast-math ^
        -msse4.2 -mavx2 -mfma ^
        -Wno-unused-variable -Wno-unused-function ^
        systems-ide-core-clean.cpp ide-main-clean.cpp ^
        -o systems-ide-clang.exe ^
        2>clang_errors.txt
    
    if !ERRORLEVEL! EQU 0 (
        echo ✓ Clang compilation successful: systems-ide-clang.exe
        set COMPILED_EXE=systems-ide-clang.exe
        goto test_exe
    ) else (
        echo ✗ Clang compilation failed, checking errors...
        if exist clang_errors.txt type clang_errors.txt
        echo.
    )
)

echo Error: Both GCC and Clang compilation failed
pause
exit /b 1

:test_exe
echo.
echo [3/3] Testing compiled executable...
if exist systems-ide.exe (
    set COMPILED_EXE=systems-ide.exe
) else (
    set COMPILED_EXE=systems-ide-clang.exe
)

echo Testing: %COMPILED_EXE%
echo.

REM Test the executable
%COMPILED_EXE% --help 2>nul
if !ERRORLEVEL! EQU 0 (
    echo ✓ Executable runs successfully
) else (
    echo ✗ Executable test failed, but binary was created
)

echo.
echo ========================================
echo Build Summary:
echo ========================================
if exist systems-ide.exe (
    echo ✓ GCC Build: systems-ide.exe ^(%~z0 bytes^)
)
if exist systems-ide-clang.exe (
    echo ✓ Clang Build: systems-ide-clang.exe ^(%~z1 bytes^)
)
echo.
echo Available Features:
echo - Native C++ core with inline assembly
echo - TLS beaconism keychain integration  
echo - Multi-language compilation support
echo - Header-free compilation mode
echo - D: drive direct access
echo - Performance monitoring ^(PMU^)
echo - Memory-mapped I/O optimization
echo - SSE/AVX SIMD acceleration
echo.
echo Usage: %COMPILED_EXE% ^[source_file^]
echo Example: %COMPILED_EXE% test.cpp
echo.
echo To test with your portable toolchains:
echo 1. Place source files in current directory
echo 2. Run: %COMPILED_EXE% filename.cpp
echo 3. Check generated .exe output
echo.
pause