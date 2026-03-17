@echo off
setlocal enabledelayedexpansion

echo ============================================
echo  RawrXD Unified Build - Zero Qt Version
echo ============================================
echo.

:: VS2022 Enterprise paths
set "MSVC_BIN=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64"
set "MSVC_LIB=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\onecore\x64"
set "MSVC_INC=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include"

set "SDK_INC=C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0"
set "SDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0"

:: Add to PATH
set PATH=%MSVC_BIN%;%PATH%

:: Common compiler flags
set "CFLAGS=/nologo /O2 /W3 /EHsc"
set "INCLUDES=/I"%MSVC_INC%" /I"%SDK_INC%\ucrt" /I"%SDK_INC%\um" /I"%SDK_INC%\shared""
set "LIBPATH=/LIBPATH:"%MSVC_LIB%" /LIBPATH:"%SDK_LIB%\ucrt\x64" /LIBPATH:"%SDK_LIB%\um\x64""
set "WINLIBS=user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib advapi32.lib msftedit.lib"

cd /d D:\RawrXD\Ship

echo [1/3] Building Win32 IDE (RawrXD_Win32_IDE.cpp)...
cl.exe %CFLAGS% %INCLUDES% RawrXD_Win32_IDE.cpp /link %LIBPATH% /SUBSYSTEM:WINDOWS /OUT:RawrXD_IDE_Ship.exe %WINLIBS%
if %errorlevel% equ 0 (
    echo      [OK] RawrXD_IDE_Ship.exe
) else (
    echo      [!] IDE Build failed
)

echo.
echo Build Complete - Zero Qt dependencies
echo ============================================

endlocal
pause
