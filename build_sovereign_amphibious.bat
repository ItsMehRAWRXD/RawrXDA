@echo off
REM ============================================================================
REM RawrXD Sovereign Host - Build Script (CLI + GUI)
REM Complete autonomous agentic system with amphibious support
REM ============================================================================

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║   RawrXD SOVEREIGN HOST - AMPHIBIOUS BUILD SYSTEM             ║
echo ║   Building both CLI and GUI subsystem versions                ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

REM Check for MASM64
where ml64 >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] ml64 not found in PATH
    echo.
    echo Please run this script from:
    echo   - Visual Studio x64 Native Tools Command Prompt
    echo   - OR after running vcvars64.bat
    echo.
    pause
    exit /b 1
)

echo [1/6] Cleaning previous builds...
if exist RawrXD_CLI.exe del RawrXD_CLI.exe
if exist RawrXD_GUI.exe del RawrXD_GUI.exe
if exist *.obj del *.obj
if exist *.ilk del *.ilk
if exist *.pdb del *.pdb

echo [2/6] Assembling CLI version (Console Subsystem)...
ml64 /c /Zi /Fo:RawrXD_Sovereign_CLI.obj RawrXD_Sovereign_CLI.asm
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CLI assembly failed
    pause
    exit /b 1
)

echo [3/6] Linking CLI executable...
link /subsystem:console /entry:main /out:RawrXD_CLI.exe RawrXD_Sovereign_CLI.obj kernel32.lib user32.lib
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CLI linking failed
    pause
    exit /b 1
)

echo [4/6] Assembling GUI version (Windows Subsystem)...
ml64 /c /Zi /Fo:RawrXD_Sovereign_GUI.obj RawrXD_Sovereign_GUI.asm
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] GUI assembly failed
    pause
    exit /b 1
)

echo [5/6] Linking GUI executable...
link /subsystem:windows /entry:WinMain /out:RawrXD_GUI.exe RawrXD_Sovereign_GUI.obj kernel32.lib user32.lib gdi32.lib
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] GUI linking failed
    pause
    exit /b 1
)

echo [6/6] Verifying builds...
if exist RawrXD_CLI.exe (
    echo   [OK] RawrXD_CLI.exe created
) else (
    echo   [FAIL] CLI executable missing
)

if exist RawrXD_GUI.exe (
    echo   [OK] RawrXD_GUI.exe created
) else (
    echo   [FAIL] GUI executable missing
)

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║   BUILD COMPLETE - AMPHIBIOUS DEPLOYMENT READY                ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.
echo Run:
echo   RawrXD_CLI.exe    - Console mode (CLI)
echo   RawrXD_GUI.exe    - Windowed mode (GUI)
echo.
echo Both executables feature:
echo   • Autonomous Agentic Loops
echo   • Multi-Agent Coordination (32 agents)
echo   • Self-Healing Infrastructure
echo   • Auto-Fix Compilation Cycle
echo   • Full Pipeline: Chat → Prompt → LLM → Token → Renderer
echo.

pause
