@echo off
setlocal enabledelayedexpansion

echo ========================================
echo OS Explorer Interceptor - Final Build
echo ========================================
echo.

REM Set paths
set "MASM32_PATH=C:\masm32"
set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
set "VC_TOOLS=%VS_PATH%\VC\Tools\MSVC\14.44.35207"
set "ML64_PATH=%VC_TOOLS%\bin\Hostx64\x64"
set "LINK_PATH=%VC_TOOLS%\bin\Hostx64\x64"
set "WINSDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0"

REM Set project paths (use absolute paths)
set "SRC_PATH=D:\lazy init ide\src"
set "BIN_PATH=D:\lazy init ide\bin"
set "OBJ_PATH=D:\lazy init ide\obj"

REM Create directories if they don't exist
if not exist "%BIN_PATH%" mkdir "%BIN_PATH%"
if not exist "%OBJ_PATH%" mkdir "%OBJ_PATH%"

echo [1/6] Building x64 Interceptor DLL...
"%ML64_PATH%\ml64.exe" /c /Fo"%OBJ_PATH%\os_explorer_interceptor_x64.obj" -D_WINDOWS -D_AMD64_ "%SRC_PATH%\os_explorer_interceptor_simple.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble x64 DLL
    pause
    exit /b 1
)

echo [2/6] Linking x64 Interceptor DLL...
"%LINK_PATH%\link.exe" /DLL /SUBSYSTEM:WINDOWS /ENTRY:DllMain /IGNORE:4216 /OUT:"%BIN_PATH%\os_explorer_interceptor_x64.dll" /LIBPATH:"%VC_TOOLS%\lib\x64" /LIBPATH:"%WINSDK_LIB%\um\x64" /LIBPATH:"%WINSDK_LIB%\ucrt\x64" /DEF:"%SRC_PATH%\os_interceptor.def" "%OBJ_PATH%\os_explorer_interceptor_x64.obj" kernel32.lib user32.lib advapi32.lib ws2_32.lib ole32.lib
if errorlevel 1 (
    echo ERROR: Failed to link x64 DLL
    pause
    exit /b 1
)

echo [3/6] Building x86 Interceptor DLL...
REM Use full path to ml.exe
call "%MASM32_PATH%\bin\ml.exe" /c /coff /Fo"%OBJ_PATH%\os_explorer_interceptor_x86.obj" /I"%MASM32_PATH%\include" -D_WINDOWS -D_WIN32 -D_X86_ "%SRC_PATH%\os_explorer_interceptor_simple_x86.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble x86 DLL
    pause
    exit /b 1
)

echo [4/6] Linking x86 Interceptor DLL...
"%LINK_PATH%\link.exe" /DLL /SUBSYSTEM:WINDOWS /ENTRY:DllMain /MACHINE:X86 /OUT:"%BIN_PATH%\os_explorer_interceptor_x86.dll" /LIBPATH:"%MASM32_PATH%\lib" /LIBPATH:"%WINSDK_LIB%\um\x86" /LIBPATH:"%WINSDK_LIB%\ucrt\x86" /DEF:"%SRC_PATH%\os_interceptor.def" "%OBJ_PATH%\os_explorer_interceptor_x86.obj" kernel32.lib user32.lib advapi32.lib ws2_32.lib ole32.lib
if errorlevel 1 (
    echo ERROR: Failed to link x86 DLL
    pause
    exit /b 1
)

echo [5/6] Building Universal CLI...
call "%MASM32_PATH%\bin\ml.exe" /c /coff /Fo"%OBJ_PATH%\os_interceptor_cli.obj" /I"%MASM32_PATH%\include" /I"%SRC_PATH%" -D_CONSOLE -D_WIN32 -D_X86_ "%SRC_PATH%\os_interceptor_cli_universal.asm"
if errorlevel 1 (
    echo ERROR: Failed to assemble CLI
    pause
    exit /b 1
)

echo [6/6] Linking Universal CLI...
"%LINK_PATH%\link.exe" /MACHINE:X86 /SUBSYSTEM:CONSOLE /OUT:"%BIN_PATH%\os_interceptor_cli.exe" /LIBPATH:"%MASM32_PATH%\lib" /LIBPATH:"%WINSDK_LIB%\um\x86" /LIBPATH:"%WINSDK_LIB%\ucrt\x86" "%OBJ_PATH%\os_interceptor_cli.obj" kernel32.lib user32.lib advapi32.lib psapi.lib
if errorlevel 1 (
    echo ERROR: Failed to link CLI
    pause
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.
echo Built files:
echo   - %BIN_PATH%\os_explorer_interceptor_x64.dll (x64 Interceptor DLL)
echo   - %BIN_PATH%\os_explorer_interceptor_x86.dll (x86 Interceptor DLL)
echo   - %BIN_PATH%\os_interceptor_cli.exe (Universal CLI - x86)
echo.
echo Usage:
echo   Run %BIN_PATH%\os_interceptor_cli.exe
echo   It will automatically detect and use the correct DLL
echo.
echo Features:
echo   - Automatically finds existing Cursor processes
echo   - Can launch new Cursor instances
echo   - Injects the appropriate DLL (x86 or x64)
echo   - Provides detailed status messages
echo.
pause
