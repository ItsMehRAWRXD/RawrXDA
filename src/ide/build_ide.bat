@echo off
REM ==========================================================================
REM  build_ide.bat — Build RawrXD IDE (Win32 GUI Shell)
REM
REM  Usage:
REM    build_ide.bat              — MSVC release build
REM    build_ide.bat debug        — MSVC debug build
REM    build_ide.bat mingw        — MinGW g++ build
REM    build_ide.bat clean        — Remove build artefacts
REM ==========================================================================

setlocal enabledelayedexpansion

set "SRCDIR=%~dp0"
set "OUTDIR=%SRCDIR%..\..\bin"
set "SRC=%SRCDIR%RawrXD_IDE_Win32.cpp"
set "RC=%SRCDIR%RawrXD_IDE_Resources.rc"
set "OUT=%OUTDIR%\RawrXD_IDE.exe"
set "OBJ=%OUTDIR%\RawrXD_IDE_Win32.obj"
set "RES=%OUTDIR%\RawrXD_IDE_Resources.res"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM ── Clean ─────────────────────────────────────────────────────────────────
if /I "%1"=="clean" (
    echo [CLEAN] Removing build artefacts...
    if exist "%OUT%" del /q "%OUT%"
    if exist "%OBJ%" del /q "%OBJ%"
    if exist "%RES%" del /q "%RES%"
    if exist "%OUTDIR%\RawrXD_IDE.pdb" del /q "%OUTDIR%\RawrXD_IDE.pdb"
    if exist "%OUTDIR%\RawrXD_IDE.ilk" del /q "%OUTDIR%\RawrXD_IDE.ilk"
    echo Done.
    goto :eof
)

REM ── MinGW build ───────────────────────────────────────────────────────────
if /I "%1"=="mingw" (
    echo [MINGW] Building RawrXD IDE with g++...
    
    REM Try windres for resources
    where windres >nul 2>&1
    if !errorlevel! equ 0 (
        echo [RC] Compiling resources...
        windres "%RC%" -o "%OUTDIR%\RawrXD_IDE_Resources.o"
        set "RESOBJ=%OUTDIR%\RawrXD_IDE_Resources.o"
    ) else (
        set "RESOBJ="
    )
    
    g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32 -D_WIN32 ^
        -Wall -Wextra ^
        "%SRC%" !RESOBJ! ^
        -o "%OUT%" ^
        -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -ladvapi32 -lole32 ^
        -mwindows
    
    if !errorlevel! equ 0 (
        echo [OK] MinGW build succeeded: %OUT%
    ) else (
        echo [FAIL] MinGW build failed.
        exit /b 1
    )
    goto :eof
)

REM ── Locate MSVC ───────────────────────────────────────────────────────────
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo [INFO] cl.exe not in PATH, trying VS Developer environment...
    
    REM Try VS 2022
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        echo [ERROR] Could not find Visual Studio. Install MSVC or use: build_ide.bat mingw
        exit /b 1
    )
)

REM ── Detect build mode ────────────────────────────────────────────────────
set "CFLAGS=/nologo /W4 /DUNICODE /D_UNICODE /DWIN32 /D_WIN32 /EHsc /std:c++17"
set "LFLAGS=/link user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib shlwapi.lib advapi32.lib ole32.lib"

if /I "%1"=="debug" (
    echo [MSVC] Debug build...
    set "CFLAGS=!CFLAGS! /Od /Zi /DDEBUG /D_DEBUG /MDd"
    set "LFLAGS=!LFLAGS! /DEBUG"
) else (
    echo [MSVC] Release build...
    set "CFLAGS=!CFLAGS! /O2 /DNDEBUG /MD /GL"
    set "LFLAGS=!LFLAGS! /LTCG /OPT:REF /OPT:ICF"
)

REM ── Compile resources ────────────────────────────────────────────────────
if exist "%RC%" (
    echo [RC] Compiling resources...
    rc.exe /nologo /fo "%RES%" "%RC%"
    if !errorlevel! neq 0 (
        echo [WARN] Resource compilation failed — building without resources.
        set "RES="
    )
) else (
    set "RES="
)

REM ── Compile and link ─────────────────────────────────────────────────────
echo [CL] Compiling %SRC%...
cl.exe !CFLAGS! /Fe:"%OUT%" "%SRC%" !RES! !LFLAGS!

if !errorlevel! equ 0 (
    echo.
    echo =============================================
    echo   BUILD SUCCEEDED: %OUT%
    echo =============================================
    
    REM Show file size
    for %%A in ("%OUT%") do echo   Size: %%~zA bytes
    echo.
) else (
    echo.
    echo =============================================
    echo   BUILD FAILED
    echo =============================================
    exit /b 1
)

REM ── Cleanup .obj if release ──────────────────────────────────────────────
if /I not "%1"=="debug" (
    if exist "%OBJ%" del /q "%OBJ%"
)

endlocal
