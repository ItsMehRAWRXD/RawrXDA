@echo off
setlocal enabledelayedexpansion

echo ===============================================
echo π Native IDE - Production Build System π
echo Systems Engineering Grade - Zero Dependencies
echo ===============================================

REM Build configuration
set BUILD_TYPE=Release
set ARCH=x64
set STATIC_LINK=ON
set SECURITY_HARDENED=ON

REM Detect Visual Studio 2022
set VS_PATH=
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VS_PATH=%%i
)

if not exist "%VS_PATH%" (
    echo [ERROR] Visual Studio 2022 not found
    echo Install Visual Studio 2022 with C++ workload
    pause
    exit /b 1
)

echo [TOOLCHAIN] Visual Studio 2022: %VS_PATH%

REM Initialize build environment
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to initialize VS2022 environment
    pause
    exit /b 1
)

REM Clean build directories
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist
mkdir build
mkdir dist

echo [CMAKE] Configuring production build...
cd build

cmake .. ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
    -DCMAKE_CXX_STANDARD=20 ^
    -DCMAKE_CXX_STANDARD_REQUIRED=ON

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed
    cd ..
    pause
    exit /b 1
)

echo [BUILD] Compiling Native IDE...
cmake --build . --config %BUILD_TYPE% --parallel

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed
    cd ..
    pause
    exit /b 1
)

cd ..

REM Package distribution
echo [PACKAGE] Creating portable distribution...
copy build\%BUILD_TYPE%\NativeIDE.exe dist\
mkdir dist\toolchain
mkdir dist\templates
mkdir dist\plugins

REM Create portable toolchain script
echo @echo off > dist\toolchain\compile.bat
echo REM Portable C/C++ compiler using system tools >> dist\toolchain\compile.bat
echo if "%%~x1"==".c" cl /nologo /O2 /MT /Fe:%%~n1.exe %%1 >> dist\toolchain\compile.bat
echo if "%%~x1"==".cpp" cl /nologo /O2 /MT /std:c++17 /Fe:%%~n1.exe %%1 >> dist\toolchain\compile.bat

REM Create demo templates
echo #include ^<stdio.h^> > dist\templates\hello.c
echo int main() { printf("Hello from Native IDE!\n"); return 0; } >> dist\templates\hello.c

echo #include ^<iostream^> > dist\templates\hello.cpp
echo int main() { std::cout ^<^< "Hello C++20!\n"; return 0; } >> dist\templates\hello.cpp

REM Create README
echo # Native IDE - Portable Distribution > dist\README.md
echo. >> dist\README.md
echo Production-grade IDE built with systems engineering standards. >> dist\README.md
echo. >> dist\README.md
echo ## Features >> dist\README.md
echo - Zero-dependency portable executable >> dist\README.md
echo - Built-in C/C++ compilation support >> dist\README.md
echo - Plugin architecture >> dist\README.md
echo - Modern C++20 codebase >> dist\README.md

echo.
echo [SUCCESS] π Native IDE built successfully!
echo.
echo Distribution: dist\NativeIDE.exe
echo Size: 
for %%A in (dist\NativeIDE.exe) do echo   %%~zA bytes
echo.
echo Test the build:
echo   cd dist
echo   NativeIDE.exe
echo.
pause