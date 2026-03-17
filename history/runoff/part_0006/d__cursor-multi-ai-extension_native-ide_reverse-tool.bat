@echo off
setlocal enabledelayedexpansion

:: reverse-tool – portable zero-conf compiler toolchain for Windows
set "ROOT=%~dp0reverse-tool.d"
set "TOOLCHAIN=%ROOT%\toolchain"
set "PATH=%TOOLCHAIN%\bin;%PATH%"

:: Unpack if needed
if not exist "%TOOLCHAIN%" (
    mkdir "%ROOT%" 2>nul
    powershell -c "& {$s=Get-Content '%~f0' -Raw; $a=$s.IndexOf('__ARCHIVE_BELOW__'); if($a -gt 0){$b=$s.Substring($a+17); [System.IO.File]::WriteAllBytes('%ROOT%\payload.zip',[System.Convert]::FromBase64String($b))}}"
    powershell -c "Expand-Archive '%ROOT%\payload.zip' '%ROOT%' -Force"
    del "%ROOT%\payload.zip" 2>nul
    echo Toolchain unpacked to %TOOLCHAIN%
)

:: Language dispatcher
set "FILE=%~1"
set "EXT=%~x1"
set "OUT=%~n1.exe"

if "%1"=="--unpack-only" (
    echo Compilers ready in %TOOLCHAIN%
    exit /b 0
)

if "%EXT%"==".c" (
    "%TOOLCHAIN%\bin\gcc.exe" -static -O2 "%FILE%" -o "%OUT%"
) else if "%EXT%"==".cpp" (
    "%TOOLCHAIN%\bin\g++.exe" -static -O2 "%FILE%" -o "%OUT%"
) else if "%EXT%"==".rs" (
    "%TOOLCHAIN%\bin\rustc.exe" -C target-feature=+crt-static "%FILE%" -o "%OUT%"
) else if "%EXT%"==".go" (
    "%TOOLCHAIN%\bin\go.exe" build -ldflags "-s -w" -o "%OUT%" "%FILE%"
) else if "%EXT%"==".py" (
    echo #!%TOOLCHAIN%\bin\python.exe > "%OUT%.py"
    type "%FILE%" >> "%OUT%.py"
    ren "%OUT%.py" "%OUT%"
) else (
    echo Usage: %0 ^<source.{c,cpp,rs,go,py}^>
    echo Produces static executable with zero dependencies
    exit /b 1
)

if exist "%OUT%" (
    echo → %OUT%
    exit /b 0
) else (
    echo Compilation failed
    exit /b 1
)

__ARCHIVE_BELOW__