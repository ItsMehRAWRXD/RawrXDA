@echo off
setlocal enabledelayedexpansion

echo ============================================
echo  RawrXD Production Build - Zero Qt Version
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
set "WINLIBS=kernel32.lib user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib advapi32.lib"

cd /d D:\RawrXD\Ship

echo [1/5] Building Titan Kernel DLL...
ml64.exe /nologo /c /Fo:RawrXD_Titan_Kernel.obj RawrXD_Titan_Kernel.asm 2>nul
if %errorlevel% neq 0 (
    echo      [!] Assembly failed, using pre-built if available
) else (
    link.exe /nologo /DLL /OUT:RawrXD_Titan_Kernel.dll RawrXD_Titan_Kernel.obj %LIBPATH% kernel32.lib ntdll.lib libcmt.lib libvcruntime.lib libucrt.lib
    if %errorlevel% equ 0 echo      [OK] RawrXD_Titan_Kernel.dll
)

echo [2/5] Building Native Model Bridge DLL...
ml64.exe /nologo /c /Fo:RawrXD_NativeModelBridge.obj RawrXD_NativeModelBridge.asm 2>nul
if %errorlevel% neq 0 (
    echo      [!] Assembly failed, using pre-built if available
) else (
    link.exe /nologo /DLL /OUT:RawrXD_NativeModelBridge.dll RawrXD_NativeModelBridge.obj %LIBPATH% kernel32.lib ntdll.lib
    if %errorlevel% equ 0 echo      [OK] RawrXD_NativeModelBridge.dll
)

echo [3/8] Building Inference Engine DLL...
cl.exe %CFLAGS% /LD %INCLUDES% RawrXD_InferenceEngine.c /link %LIBPATH% /DLL /OUT:RawrXD_InferenceEngine.dll kernel32.lib
if %errorlevel% equ 0 echo      [OK] RawrXD_InferenceEngine.dll

echo [4/8] Building Agentic Engine DLL...
cl.exe %CFLAGS% /LD %INCLUDES% RawrXD_AgenticEngine.c /link %LIBPATH% /DLL /OUT:RawrXD_AgenticEngine.dll kernel32.lib user32.lib shell32.lib
if %errorlevel% equ 0 echo      [OK] RawrXD_AgenticEngine.dll

echo [5/8] Building Terminal Manager DLL...
cl.exe %CFLAGS% /LD %INCLUDES% RawrXD_TerminalMgr.c /link %LIBPATH% /DLL /OUT:RawrXD_TerminalMgr.dll kernel32.lib
if %errorlevel% equ 0 echo      [OK] RawrXD_TerminalMgr.dll

echo [6/8] Building Plan Orchestrator DLL...
cl.exe %CFLAGS% /LD %INCLUDES% RawrXD_PlanOrchestrator.c /link %LIBPATH% /DLL /OUT:RawrXD_PlanOrchestrator.dll kernel32.lib shlwapi.lib
if %errorlevel% equ 0 echo      [OK] RawrXD_PlanOrchestrator.dll

echo [7/8] Building Win32 IDE (Full Production)...
cl.exe %CFLAGS% %INCLUDES% Win32_IDE_Complete.cpp /link %LIBPATH% /SUBSYSTEM:WINDOWS /OUT:RawrXD_IDE_Production.exe %WINLIBS%
if %errorlevel% equ 0 echo      [OK] RawrXD_IDE_Production.exe

echo [8/8] Building CLI...
cl.exe %CFLAGS% %INCLUDES% RawrXD_CLI_Standalone.cpp /link %LIBPATH% /SUBSYSTEM:CONSOLE /OUT:RawrXD_CLI.exe %WINLIBS%
if %errorlevel% equ 0 echo      [OK] RawrXD_CLI.exe

echo.
echo ============================================
echo  Build Complete - Output Files:
echo ============================================
echo.
for %%f in (*.exe *.dll) do (
    for /f "tokens=1-3 delims= " %%a in ('dir /n "%%f" ^| find "%%f"') do (
        echo   %%f    %%a bytes
    )
)

echo.
echo Zero Qt dependencies - Pure Win32 + MASM64
echo ============================================

endlocal
