@echo off
setlocal enabledelayedexpansion

REM Build script for RawrXD_NativeModelBridge.asm DLL
REM Requires: ml64.exe (MASM64 assembler) + link.exe (linker)

echo [BUILD] Starting RawrXD Native Model Bridge DLL build...
echo.

REM Check for ml64
where ml64.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ml64.exe not found. Install VS2022 Build Tools.
    exit /b 1
)

REM Setup paths
set MASM64_PATH=C:\masm64
set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise
set ASM_FILE=RawrXD_NativeModelBridge.asm
set OBJ_FILE=RawrXD_NativeModelBridge.obj
set DLL_FILE=RawrXD_NativeModelBridge.dll
set LIB_FILE=RawrXD_NativeModelBridge.lib

REM Assembling
echo [ASM] Assembling %ASM_FILE%...
ml64 /c /Zi /D"PRODUCTION=1" /I"%MASM64_PATH%\include64" %ASM_FILE%

if errorlevel 1 (
    echo [ERROR] Assembly failed
    exit /b 1
)

echo [ASM] Assembly successful: %OBJ_FILE%

REM Linking (as DLL)
echo.
echo [LINK] Linking to %DLL_FILE%...

link /DLL /OUT:%DLL_FILE% /PDB:%DLL_FILE:.dll=.pdb% ^
    /SUBSYSTEM:WINDOWS /ENTRY:DllMain /MACHINE:X64 ^
    /NODEFAULTLIB ^
    %OBJ_FILE% ^
    kernel32.lib ntdll.lib user32.lib ^
    msvcrt.lib libcmt.lib

if errorlevel 1 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo [LINK] Linking successful
echo [BUILD] Complete! Output: %DLL_FILE%
echo.

REM List output files
dir /B %DLL_FILE% %LIB_FILE% 2>nul

echo.
echo [INFO] To test the DLL:
echo   set PATH=!CD!;!PATH!
echo   rundll32.exe %DLL_FILE%,RunLocalModel "D:\OllamaModels\BigDaddyG-UNLEASHED-Q4_K_M.gguf" "Hello world"

endlocal
