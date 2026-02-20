@echo off
setlocal enabledelayedexpansion
REM ============================================================================
REM CODEX AI REVERSE ENGINE — BUILD ALL IDE TARGETS (D-DRIVE)
REM Builds: ASM engine, CLI IDE, GUI IDE, Model Server
REM All source & output on D:\rawrxd — NO C-drive source references
REM ============================================================================

echo ============================================================
echo   CODEX AI REVERSE ENGINE v7.0 — BUILD SYSTEM (D-DRIVE)
echo   Building: ASM Engine + CLI IDE + GUI IDE + Model Server
echo ============================================================
echo.

REM ---------- Auto-detect VS2022 toolchain ----------
REM Try D: first, fall back to C:, then PATH
set "VSDIR="
if exist "D:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSDIR=D:\VS2022Enterprise"
) else if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSDIR=C:\VS2022Enterprise"
)

REM Activate MSVC environment if not already active
where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    if defined VSDIR (
        echo [ENV] Activating MSVC from %VSDIR% ...
        call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        echo [WARN] VS2022 not found on D: or C: — relying on PATH
    )
)

REM If vcvars failed (vcvarsall.bat missing), set paths manually
where cl >nul 2>&1
if !ERRORLEVEL! NEQ 0 (
    REM Auto-detect MSVC Tools version
    set "_MSVC_BIN="
    for /d %%V in ("C:\VS2022Enterprise\VC\Tools\MSVC\*") do set "_MSVC_BIN=%%V\bin\Hostx64\x64"
    for /d %%V in ("D:\VS2022Enterprise\VC\Tools\MSVC\*") do set "_MSVC_BIN=%%V\bin\Hostx64\x64"
    if defined _MSVC_BIN (
        echo [ENV] Manual toolchain: !_MSVC_BIN!
        set "PATH=!_MSVC_BIN!;!PATH!"
        REM Resolve MSVC include/lib from bin path (go up 3 levels)
        for %%B in ("!_MSVC_BIN!\..\..\..") do (
            set "INCLUDE=%%~fB\include;!INCLUDE!"
            set "LIB=%%~fB\lib\x64;!LIB!"
        )
    )
    REM Windows SDK
    set "_SDK_ROOT="
    if exist "D:\Program Files (x86)\Windows Kits\10" set "_SDK_ROOT=D:\Program Files (x86)\Windows Kits\10"
    if not defined _SDK_ROOT if exist "C:\Program Files (x86)\Windows Kits\10" set "_SDK_ROOT=C:\Program Files (x86)\Windows Kits\10"
    if defined _SDK_ROOT (
        set "SDKVER=10.0.22621.0"
        set "INCLUDE=!_SDK_ROOT!\Include\!SDKVER!\ucrt;!_SDK_ROOT!\Include\!SDKVER!\um;!_SDK_ROOT!\Include\!SDKVER!\shared;!INCLUDE!"
        set "LIB=!_SDK_ROOT!\Lib\!SDKVER!\ucrt\x64;!_SDK_ROOT!\Lib\!SDKVER!\um\x64;!LIB!"
        set "PATH=!_SDK_ROOT!\bin\!SDKVER!\x64;!PATH!"
    )
)

REM ---------- Paths — everything under D:\rawrxd ----------
set "IDE_ROOT=%~dp0"
set "OUTDIR=%IDE_ROOT%build_ide_output"
set "SRC_ASM=%IDE_ROOT%CodexAIReverseEngine.asm"
set "SRC_CLI=%IDE_ROOT%ide\cli\codex_cli_ide.cpp"
set "SRC_GUI=%IDE_ROOT%ide\gui\codex_gui_ide.cpp"
set "SRC_HTML=%IDE_ROOT%ide\html\codex_html_ide.html"
set "MODEL_SRV=%IDE_ROOT%ide\model-server"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

REM ============================================================
REM 1. Build ASM Engine (MASM64)
REM ============================================================

echo [1/5] Building ASM Engine (CodexAIReverseEngine.asm)...

where ml64 >nul 2>&1
if %ERRORLEVEL% == 0 (
    ml64 /c /Fo"%OUTDIR%\CodexAIReverseEngine.obj" "%SRC_ASM%"
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
    echo       [SKIP] ml64 not in PATH — VS2022 vcvars64.bat not detected
)

echo.

REM ============================================================
REM 2. Build CLI IDE (C++)
REM ============================================================

echo [2/5] Building CLI IDE (codex_cli_ide.cpp)...

where cl >nul 2>&1
if %ERRORLEVEL% == 0 (
    cl /EHsc /O2 /W3 /Fe:"%OUTDIR%\codex_cli_ide.exe" "%SRC_CLI%" ^
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

echo [3/5] Building GUI IDE (codex_gui_ide.cpp)...

where cl >nul 2>&1
if %ERRORLEVEL% == 0 (
    cl /EHsc /O2 /W3 /Fe:"%OUTDIR%\codex_gui_ide.exe" "%SRC_GUI%" ^
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

echo [4/5] HTML IDE ready: %SRC_HTML%
echo       Integration Hub: %~dp0ide\codex_integration_hub.html

echo.

REM ============================================================
REM 5. Model Server (npm install if needed)
REM ============================================================

echo [5/5] Setting up Model Server (%MODEL_SRV%)...

where node >nul 2>&1
if %ERRORLEVEL% == 0 (
    if not exist "%MODEL_SRV%\node_modules" (
        echo       Installing npm deps...
        pushd "%MODEL_SRV%"
        npm install --omit=dev 2>nul
        popd
    )
    echo       [OK] Model server ready — run: node "%MODEL_SRV%\proxy-server.js"
) else (
    echo       [SKIP] node.js not in PATH — model server requires Node
)

echo.
echo ============================================================
echo   BUILD COMPLETE — ALL D-DRIVE
echo ============================================================
echo.
echo   ASM Engine:    %OUTDIR%\CodexAIReverseEngine.exe
echo   CLI IDE:       %OUTDIR%\codex_cli_ide.exe
echo   GUI IDE:       %OUTDIR%\codex_gui_ide.exe
echo   HTML IDE:      %SRC_HTML%
echo   Hub IDE:       %~dp0ide\codex_integration_hub.html
echo   Model Server:  %MODEL_SRV%\proxy-server.js
echo.
echo   Usage:
echo     CLI:    codex_cli_ide.exe load D:\rawrxd\model.gguf
echo             codex_cli_ide.exe scan D:\models\
echo             codex_cli_ide.exe                    (interactive REPL)
echo.
echo     GUI:    codex_gui_ide.exe [model.gguf]
echo.
echo     HTML:   Open codex_html_ide.html in browser
echo             Drag-and-drop any .gguf file
echo.
echo     HUB:    Open codex_integration_hub.html in browser
echo             Unified dashboard with all IDE modes
echo.
echo     ASM:    CodexAIReverseEngine.exe             (binary analysis)
echo.
echo     Model:  node ide\model-server\proxy-server.js
echo             curl -X POST http://localhost:3000/models/load ^
echo               -H "Content-Type: application/json" ^
echo               -d "{\"source\":\"gguf\",\"model\":\"D:\\rawrxd\\model.gguf\"}"
echo ============================================================
