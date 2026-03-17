@echo off
REM ============================================================================
REM CODEX AI REVERSE ENGINE — BUILD ALL IDE TARGETS
REM Builds: ASM engine, CLI IDE, GUI IDE
REM Opens:  HTML IDE in default browser
REM ============================================================================

echo ============================================================
echo   CODEX AI REVERSE ENGINE v7.0 — BUILD SYSTEM
echo   Building: ASM Engine + CLI IDE + GUI IDE + HTML IDE
echo ============================================================
echo.

set VSDIR=C:\VS2022Enterprise
set MASM_INC=\masm64\include64
set MASM_LIB=\masm64\lib64
set OUTDIR=%~dp0build_ide_output

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM ============================================================
REM 1. Build ASM Engine (MASM64)
REM ============================================================

echo [1/4] Building ASM Engine (CodexAIReverseEngine.asm)...

where ml64 >nul 2>&1
if %ERRORLEVEL% == 0 (
    ml64 /c /Fo"%OUTDIR%\CodexAIReverseEngine.obj" "%~dp0CodexAIReverseEngine.asm"
    if %ERRORLEVEL% == 0 (
        echo       [OK] ASM assembled successfully
        link /SUBSYSTEM:CONSOLE /ENTRY:main "%OUTDIR%\CodexAIReverseEngine.obj" ^
            kernel32.lib user32.lib advapi32.lib shlwapi.lib psapi.lib ^
            /OUT:"%OUTDIR%\CodexAIReverseEngine.exe"
        if %ERRORLEVEL% == 0 (
            echo       [OK] ASM linked: %OUTDIR%\CodexAIReverseEngine.exe
        ) else (
            echo       [!!] ASM link failed
        )
    ) else (
        echo       [!!] ASM assembly failed
    )
) else (
    echo       [SKIP] ml64 not in PATH — set up VS Developer Command Prompt
    echo              or run: "%%VSDIR%%\VC\Auxiliary\Build\vcvars64.bat"
)

echo.

REM ============================================================
REM 2. Build CLI IDE (C++)
REM ============================================================

echo [2/4] Building CLI IDE (codex_cli_ide.cpp)...

where cl >nul 2>&1
if %ERRORLEVEL% == 0 (
    cl /EHsc /O2 /W3 /Fe:"%OUTDIR%\codex_cli_ide.exe" "%~dp0ide\cli\codex_cli_ide.cpp" ^
        /link kernel32.lib user32.lib
    if %ERRORLEVEL% == 0 (
        echo       [OK] CLI IDE built: %OUTDIR%\codex_cli_ide.exe
    ) else (
        echo       [!!] CLI IDE build failed
    )
) else (
    echo       [SKIP] cl.exe not in PATH
)

echo.

REM ============================================================
REM 3. Build GUI IDE (C++ Win32)
REM ============================================================

echo [3/4] Building GUI IDE (codex_gui_ide.cpp)...

where cl >nul 2>&1
if %ERRORLEVEL% == 0 (
    cl /EHsc /O2 /W3 /Fe:"%OUTDIR%\codex_gui_ide.exe" "%~dp0ide\gui\codex_gui_ide.cpp" ^
        /link /SUBSYSTEM:WINDOWS kernel32.lib user32.lib gdi32.lib ^
        comdlg32.lib shell32.lib comctl32.lib
    if %ERRORLEVEL% == 0 (
        echo       [OK] GUI IDE built: %OUTDIR%\codex_gui_ide.exe
    ) else (
        echo       [!!] GUI IDE build failed
    )
) else (
    echo       [SKIP] cl.exe not in PATH
)

echo.

REM ============================================================
REM 4. HTML IDE (no build needed)
REM ============================================================

echo [4/4] HTML IDE ready: %~dp0ide\html\codex_html_ide.html

echo.
echo ============================================================
echo   BUILD COMPLETE
echo ============================================================
echo.
echo   ASM Engine:  %OUTDIR%\CodexAIReverseEngine.exe
echo   CLI IDE:     %OUTDIR%\codex_cli_ide.exe
echo   GUI IDE:     %OUTDIR%\codex_gui_ide.exe
echo   HTML IDE:    %~dp0ide\html\codex_html_ide.html
echo.
echo   Usage:
echo     CLI:   codex_cli_ide.exe load model.gguf
echo            codex_cli_ide.exe scan C:\models\
echo            codex_cli_ide.exe                    (interactive REPL)
echo.
echo     GUI:   codex_gui_ide.exe [model.gguf]
echo.
echo     HTML:  Open codex_html_ide.html in browser
echo            Drag-and-drop any .gguf file
echo.
echo     ASM:   CodexAIReverseEngine.exe             (binary analysis)
echo ============================================================
