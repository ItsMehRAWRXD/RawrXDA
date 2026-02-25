@echo off
REM ============================================================
REM RawrXD Production Build System
REM Version: 6.0.0
REM Date: February 24, 2026
REM ============================================================
REM Builds all components: ASM modules, PE writer, IDE shell
REM Requires: MSVC Build Tools (ml64, cl, link, rc)
REM ============================================================

setlocal enabledelayedexpansion

set "BUILD_ROOT=D:\rawrxd"
set "SRC=%BUILD_ROOT%\src"
set "OUT=%BUILD_ROOT%\build\release"
set "OBJ=%BUILD_ROOT%\build\obj"
set "BIN=C:\RawrXD\bin"

echo ============================================================
echo  RawrXD Production Build System v6.0.0
echo ============================================================

REM --- Detect MSVC ---
where ml64.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ml64.exe not found. Run from VS Developer Command Prompt.
    echo         Or run: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    exit /b 1
)

REM --- Create output directories ---
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%BIN%" mkdir "%BIN%"

set ERRORS=0
set BUILT=0

REM ============================================================
REM 1. Assemble GPU/DMA Production Module
REM ============================================================
echo.
echo [1/5] Assembling GPU/DMA Production Module...
ml64 /c /nologo /Fo"%OBJ%\gpu_dma_production.obj" "%SRC%\agentic\gpu_dma_production.asm"
if errorlevel 1 (
    echo   [WARN] GPU DMA assembly had warnings/errors
    set /a ERRORS+=1
) else (
    echo   [OK] gpu_dma_production.obj
    set /a BUILT+=1
)

REM ============================================================
REM 2. Assemble PE Backend Emitter
REM ============================================================
echo.
echo [2/5] Assembling PE Backend Emitter...
ml64 /c /nologo /Fo"%OBJ%\PE_Backend_Emitter.obj" "%SRC%\PE_Backend_Emitter.asm"
if errorlevel 1 (
    echo   [WARN] PE Backend Emitter assembly had warnings/errors
    set /a ERRORS+=1
) else (
    echo   [OK] PE_Backend_Emitter.obj
    set /a BUILT+=1
)

REM Link PE generator
echo   Linking pe_generator.exe...
link /nologo /subsystem:console /entry:main /out:"%OUT%\pe_generator.exe" "%OBJ%\PE_Backend_Emitter.obj" kernel32.lib
if errorlevel 1 (
    echo   [WARN] PE generator link failed
    set /a ERRORS+=1
) else (
    echo   [OK] pe_generator.exe
    copy /y "%OUT%\pe_generator.exe" "%BIN%\pe_generator.exe" >nul
    set /a BUILT+=1
)

REM ============================================================
REM 3. Assemble BareMetal PE Writer
REM ============================================================
echo.
echo [3/5] Assembling BareMetal PE Writer...
ml64 /c /nologo /Fo"%OBJ%\BareMetal_PE_Writer.obj" "%SRC%\BareMetal_PE_Writer.asm"
if errorlevel 1 (
    echo   [WARN] BareMetal PE Writer assembly had warnings/errors
    set /a ERRORS+=1
) else (
    echo   [OK] BareMetal_PE_Writer.obj
    set /a BUILT+=1
)

link /nologo /subsystem:console /entry:main /out:"%OUT%\baremetal_pe_writer.exe" "%OBJ%\BareMetal_PE_Writer.obj" kernel32.lib
if errorlevel 1 (
    echo   [WARN] BareMetal PE Writer link failed
    set /a ERRORS+=1
) else (
    echo   [OK] baremetal_pe_writer.exe
    copy /y "%OUT%\baremetal_pe_writer.exe" "%BIN%\baremetal_pe_writer.exe" >nul
    set /a BUILT+=1
)

REM ============================================================
REM 4. Compile PE Writer Production (C++)
REM ============================================================
echo.
echo [4/5] Compiling PE Writer Production Library...
set PE_SRC=%SRC%\pe_writer_production
cl /nologo /EHsc /std:c++17 /O2 /W4 /c ^
    /I"%PE_SRC%" ^
    /Fo"%OBJ%\pe_writer.obj" ^
    "%PE_SRC%\pe_writer.cpp"
if errorlevel 1 (
    echo   [WARN] PE Writer compilation failed
    set /a ERRORS+=1
) else (
    echo   [OK] pe_writer.obj
    set /a BUILT+=1
)

REM ============================================================
REM 5. Build Win32 IDE Shell
REM ============================================================
echo.
echo [5/5] Building Win32 IDE Shell...
set IDE_SRC=%SRC%\ide
if exist "%IDE_SRC%\RawrXD_IDE_Win32.cpp" (
    REM Compile resources
    rc /nologo /fo "%OBJ%\ide_resources.res" "%IDE_SRC%\RawrXD_IDE_Resources.rc" 2>nul
    
    REM Compile IDE
    cl /nologo /EHsc /std:c++17 /O2 /W4 ^
        /DUNICODE /D_UNICODE ^
        /I"%IDE_SRC%" ^
        /Fe"%OUT%\RawrXD_IDE.exe" ^
        "%IDE_SRC%\RawrXD_IDE_Win32.cpp" ^
        "%OBJ%\ide_resources.res" ^
        /link /subsystem:windows ^
        kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib ^
        shell32.lib advapi32.lib ole32.lib shlwapi.lib ^
        /MANIFEST:EMBED /MANIFESTINPUT:"%IDE_SRC%\RawrXD_IDE_Resources.rc"
    if errorlevel 1 (
        REM Try without manifest embed
        cl /nologo /EHsc /std:c++17 /O2 /W4 ^
            /DUNICODE /D_UNICODE ^
            /I"%IDE_SRC%" ^
            /Fe"%OUT%\RawrXD_IDE.exe" ^
            "%IDE_SRC%\RawrXD_IDE_Win32.cpp" ^
            /link /subsystem:windows ^
            kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib ^
            shell32.lib advapi32.lib ole32.lib shlwapi.lib
        if errorlevel 1 (
            echo   [WARN] IDE compilation failed
            set /a ERRORS+=1
        ) else (
            echo   [OK] RawrXD_IDE.exe
            copy /y "%OUT%\RawrXD_IDE.exe" "%BIN%\RawrXD_IDE.exe" >nul
            set /a BUILT+=1
        )
    ) else (
        echo   [OK] RawrXD_IDE.exe
        copy /y "%OUT%\RawrXD_IDE.exe" "%BIN%\RawrXD_IDE.exe" >nul
        set /a BUILT+=1
    )
) else (
    echo   [SKIP] IDE source not found at %IDE_SRC%
)

REM ============================================================
REM Build Summary
REM ============================================================
echo.
echo ============================================================
echo  Build Summary
echo ============================================================
echo  Components built: %BUILT%
echo  Errors:           %ERRORS%
echo  Output:           %OUT%
echo  Binaries:         %BIN%
echo ============================================================

if %ERRORS% GTR 0 (
    echo  [WARNING] Build completed with %ERRORS% error(s)
    exit /b 1
) else (
    echo  [SUCCESS] All components built successfully
    exit /b 0
)
