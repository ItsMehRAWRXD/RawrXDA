@echo off
echo ========================================
echo Building RawrXD GUI Enhanced Edition
echo ========================================

set COMPILER=g++
set CXXFLAGS=-std=c++17 -O2 -DUNICODE -D_UNICODE
set INCLUDES=-Iinclude -Isrc
set LIBS=-lws2_32 -lcomctl32 -lole32 -loleaut32 -luuid
set OUTDIR=build
set TARGET=%OUTDIR%\rawrxd_gui.exe

if not exist %OUTDIR% mkdir %OUTDIR%

echo.
echo [1/4] Compiling GUI Enhanced Implementation...
%COMPILER% %CXXFLAGS% %INCLUDES% -c src/gui_main_enhanced.cpp -o %OUTDIR%/gui_main_enhanced.o
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: GUI Enhanced compilation failed!
    exit /b 1
)

echo [2/4] Compiling GUI Launcher...
%COMPILER% %CXXFLAGS% %INCLUDES% -c src/gui_launcher.cpp -o %OUTDIR%/gui_launcher.o -mwindows
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: GUI Launcher compilation failed!
    exit /b 1
)

echo [3/4] Compiling Stubs and Integration...
%COMPILER% %CXXFLAGS% %INCLUDES% -c src/cli_extras_stubs.cpp -o %OUTDIR%/cli_extras_stubs.o
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Stubs compilation failed!
    exit /b 1
)

echo [4/4] Linking GUI Application...
%COMPILER% %CXXFLAGS% -o %TARGET% ^
    %OUTDIR%/gui_launcher.o ^
    %OUTDIR%/gui_main_enhanced.o ^
    %OUTDIR%/cli_extras_stubs.o ^
    %LIBS% -mwindows

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Linking failed!
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESS!
echo ========================================
echo Executable: %TARGET%
echo.
echo Running GUI...
%TARGET%
