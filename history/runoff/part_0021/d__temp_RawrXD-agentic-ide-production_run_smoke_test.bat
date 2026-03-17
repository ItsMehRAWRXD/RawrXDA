@echo off
REM Smoke test for static linking

set MSVC_PATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717
set CL=%MSVC_PATH%\bin\Hostx64\x64\cl.exe
set LINK=%MSVC_PATH%\bin\Hostx64\x64\link.exe
set SDK_PATH=C:\Program Files (x86)\Windows Kits\10

set INCLUDE=%SDK_PATH%\Include\10.0.22621.0\shared;%SDK_PATH%\Include\10.0.22621.0\ucrt;%SDK_PATH%\Include\10.0.22621.0\um;%SDK_PATH%\Include\10.0.22621.0\winrt;%MSVC_PATH%\include
set LIB=%MSVC_PATH%\lib\x64;%MSVC_PATH%\lib\onecore\x64;%SDK_PATH%\Lib\10.0.22621.0\um\x64;%SDK_PATH%\Lib\10.0.22621.0\ucrt\x64

echo.
echo ================================================================
echo    Static Linking Smoke Test
echo ================================================================
echo.

cd D:\temp\RawrXD-agentic-ide-production

echo [1/2] Compiling smoke test (against static-linked DLL)...
call "%CL%" /c /O2 /MD /nologo /Fo"test_static_linking.obj" "test_static_linking.c"
if errorlevel 1 (
    echo   X Compilation failed!
    exit /b 1
)
echo       OK

echo.
echo [2/2] Linking with RawrXD-SovereignLoader.lib (import lib)...
call "%LINK%" /NOLOGO /MACHINE:X64 /OUT:"test_static_linking.exe" "test_static_linking.obj" "build-sovereign-static\bin\RawrXD-SovereignLoader.lib" kernel32.lib user32.lib
if errorlevel 1 (
    echo   X Linking failed!
    exit /b 1
)
echo       OK

echo.
if exist "test_static_linking.exe" (
    echo   Executable created: test_static_linking.exe
    echo.
    echo Running smoke test...
    echo.
    test_static_linking.exe
    echo.
) else (
    echo   X Executable not found!
    exit /b 1
)

echo ================================================================
echo SUCCESS
echo ================================================================
