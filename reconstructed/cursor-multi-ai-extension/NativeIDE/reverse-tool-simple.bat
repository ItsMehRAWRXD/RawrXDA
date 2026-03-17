@echo off
setlocal enabledelayedexpansion

echo ===================================================================
echo REVERSE-TOOL - Universal Zero-Config Compiler Toolchain 
echo ===================================================================

REM Check for MinGW/GCC
if exist "C:\mingw64\bin\gcc.exe" (
    set "PATH=C:\mingw64\bin;%PATH%"
    echo [FOUND] MinGW/GCC at C:\mingw64
)

REM Get command and file
set "CMD=%~1"
set "FILE=%~2"

if "%CMD%"=="--help" goto :show_help
if "%CMD%"=="toolchain-info" goto :show_info
if "%CMD%"=="templates" goto :create_templates
if "%CMD%"=="compile" goto :compile_file

REM Direct compilation (backwards compatible)
set "FILE=%CMD%"
if "%FILE%"=="" goto :show_help

:compile_file
echo [COMPILE] Compiling: %FILE%

for %%F in ("%FILE%") do (
    set "EXT=%%~xF"
    set "NAME=%%~nF" 
    set "DIR=%%~dpF"
)

set "OUT=%DIR%%NAME%.exe"

REM Compile based on extension
if /i "%EXT%"==".c" goto :compile_c
if /i "%EXT%"==".cpp" goto :compile_cpp
if /i "%EXT%"==".cxx" goto :compile_cpp

echo [ERROR] Unsupported file: %EXT%
exit /b 1

:compile_c
gcc --version >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [GCC] Compiling C...
    gcc -std=c11 -static -O2 "%FILE%" -o "%OUT%" -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lcomdlg32
    goto :check_result
)
echo [ERROR] GCC not found
exit /b 1

:compile_cpp  
g++ --version >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [G++] Compiling C++...
    g++ -std=c++20 -static -O2 "%FILE%" -o "%OUT%" -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lcomdlg32 -lole32 -luuid
    goto :check_result
)
echo [ERROR] G++ not found  
exit /b 1

:check_result
if exist "%OUT%" (
    echo [SUCCESS] Created: %OUT%
    dir "%OUT%" | findstr ".exe"
    exit /b 0
) else (
    echo [ERROR] Compilation failed
    exit /b 1
)

:show_help
echo.
echo Usage:
echo   %~nx0 [compile] ^<file.c/cpp^>  - Compile C/C++ file
echo   %~nx0 templates                - Create sample files
echo   %~nx0 toolchain-info          - Show compiler info
echo   %~nx0 --help                  - This help
echo.
exit /b 0

:show_info
echo.
echo Toolchain Information:
gcc --version >nul 2>&1 && echo [OK] GCC available || echo [NO] GCC not found
g++ --version >nul 2>&1 && echo [OK] G++ available || echo [NO] G++ not found  
echo.
exit /b 0

:create_templates
echo Creating templates...
mkdir templates 2>nul

echo #include ^<stdio.h^> > templates\hello.c
echo int main() { >> templates\hello.c  
echo     printf("Hello C!\n"); >> templates\hello.c
echo     return 0; >> templates\hello.c
echo } >> templates\hello.c

echo #include ^<iostream^> > templates\hello.cpp
echo int main() { >> templates\hello.cpp
echo     std::cout ^<^< "Hello C++!" ^<^< std::endl; >> templates\hello.cpp  
echo     return 0; >> templates\hello.cpp
echo } >> templates\hello.cpp

echo [OK] Templates created in ./templates/
exit /b 0