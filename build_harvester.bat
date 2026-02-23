@echo off
echo Building model-llm-harvester.asm...

:: Check for ml64 (MASM)
where ml64 >nul 2>nul
if %ERRORLEVEL% equ 0 goto :build

echo ml64.exe not found in PATH. Attempting to find vcvars64.bat...

:: Try standard VS2022 Enterprise location (GitHub Actions)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    goto :build
)

:: Try standard VS2022 Community location
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    goto :build
)

:: Try standard VS2019 Enterprise location
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    goto :build
)

:: Try standard VS2019 Community location
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    goto :build
)

:: Try VS2022 Build Tools location
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    goto :build
)

:: Try VS2019 Build Tools location
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    goto :build
)

echo ERROR: ml64.exe not found and vcvars64.bat could not be located.
echo Please run this from a Visual Studio Native Tools Command Prompt.
exit /b 1

:build
:: Assemble and Link
:: /Entry:main is crucial because we are not using the CRT (mainCRTStartup)
:: /Subsystem:console is standard for CLI tools
ml64.exe model-llm-harvester.asm /link /subsystem:console /entry:main /defaultlib:kernel32.lib /out:model-llm-harvester.exe

if %ERRORLEVEL% equ 0 (
    echo.
    echo Build Successful!
    echo Run with: model-llm-harvester.exe
) else (
    echo.
    echo Build Failed.
)
