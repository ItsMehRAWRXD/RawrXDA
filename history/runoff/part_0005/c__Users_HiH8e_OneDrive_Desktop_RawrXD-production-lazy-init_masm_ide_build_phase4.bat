@ECHO OFF
REM ===========================================================================
REM RawrXD IDE - Phase 4 Build Script (LLM Integration & Agentic Loop)
REM ===========================================================================
setlocal enabledelayedexpansion

ECHO.
ECHO ========================================================================
ECHO   RAWXD IDE - PHASE 4 BUILD: LLM Integration ^& Agentic Loop
ECHO ========================================================================
ECHO.

REM Check MASM32
if not exist "C:\masm32" (
    ECHO ERROR: MASM32 not found at C:\masm32
    ECHO Please install MASM32 from: http://www.masm32.com
    PAUSE
    EXIT /B 1
)

REM Setup paths
SET MASM32=C:\masm32
SET ML=%MASM32%\bin\ml.exe
SET LINK=%MASM32%\bin\link.exe
SET LIBPATH=%MASM32%\lib
SET INCLUDE=%MASM32%\include;include
SET SRC=src
SET OUT=build\obj

REM Create output directory
IF NOT EXIST "build" MKDIR "build"
IF NOT EXIST "%OUT%" MKDIR "%OUT%"

ECHO [1/5] Compiling LLM Client System...
cmd /c "C:\masm32\bin\ml.exe /c /coff /Cp /Zi /IC:\masm32\include /Fo%OUT%\llm_client.obj %SRC%\llm_client.asm"
IF ERRORLEVEL 1 (
    ECHO ERROR: Failed to compile llm_client.asm
    PAUSE
    EXIT /B 1
)
ECHO   ✓ llm_client.obj created successfully

ECHO.
ECHO [2/5] Compiling Agentic Loop System...
cmd /c "C:\masm32\bin\ml.exe /c /coff /Cp /Zi /IC:\masm32\include /Fo%OUT%\agentic_loop.obj %SRC%\agentic_loop.asm"
IF ERRORLEVEL 1 (
    ECHO ERROR: Failed to compile agentic_loop.asm
    PAUSE
    EXIT /B 1
)
ECHO   ✓ agentic_loop.obj created successfully

ECHO.
ECHO [3/5] Compiling Chat Interface...
cmd /c "C:\masm32\bin\ml.exe /c /coff /Cp /Zi /IC:\masm32\include /Fo%OUT%\chat_interface.obj %SRC%\chat_interface.asm"
IF ERRORLEVEL 1 (
    ECHO ERROR: Failed to compile chat_interface.asm
    PAUSE
    EXIT /B 1
)
ECHO   ✓ chat_interface.obj created successfully

ECHO.
ECHO [4/5] Compiling Phase 4 Integration...
cmd /c "C:\masm32\bin\ml.exe /c /coff /Cp /Zi /IC:\masm32\include /Fo%OUT%\phase4_integration.obj %SRC%\phase4_integration.asm"
IF ERRORLEVEL 1 (
    ECHO ERROR: Failed to compile phase4_integration.asm
    PAUSE
    EXIT /B 1
)
ECHO   ✓ phase4_integration.obj created successfully

ECHO.
ECHO [5/5] Verifying Build...
IF NOT EXIST "%OUT%\llm_client.obj" (
    ECHO ERROR: llm_client.obj not found!
    PAUSE
    EXIT /B 1
)
IF NOT EXIST "%OUT%\agentic_loop.obj" (
    ECHO ERROR: agentic_loop.obj not found!
    PAUSE
    EXIT /B 1
)
IF NOT EXIST "%OUT%\chat_interface.obj" (
    ECHO ERROR: chat_interface.obj not found!
    PAUSE
    EXIT /B 1
)
IF NOT EXIST "%OUT%\phase4_integration.obj" (
    ECHO ERROR: phase4_integration.obj not found!
    PAUSE
    EXIT /B 1
)

ECHO.
ECHO ========================================================================
ECHO   ✓ PHASE 4 BUILD SUCCESSFUL!
ECHO ========================================================================
ECHO.
ECHO Phase 4 Modules Compiled:
ECHO   • llm_client.obj         - Multi-backend LLM client
ECHO   • agentic_loop.obj       - Complete agentic reasoning system
ECHO   • chat_interface.obj     - Professional chat UI
ECHO   • phase4_integration.obj - Menu and integration layer
ECHO.
ECHO Next Steps:
ECHO   1. Add Phase 4 modules to main IDE build (build.bat)
ECHO   2. Call InitializePhase4Integration from main.asm
ECHO   3. Add HandlePhase4Command to message loop
ECHO   4. Test AI menu and chat interface
ECHO.
ECHO Note: Phase 4 modules are compiled but not yet linked into main IDE.
ECHO       See PHASE4_INTEGRATION_GUIDE.md for integration instructions.
ECHO.
PAUSE

