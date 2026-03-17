@echo off
REM ============================================================================
REM Subagent Batch Processor MASM x64 Build Script
REM Pure Assembly Build - NO MIXING
REM Processes 50 files per subagent batch
REM ============================================================================

echo.
echo ======================================================
echo   SUBAGENT BATCH PROCESSOR - MASM x64 Builder
echo   NO MIXING - Pure Assembly
echo ======================================================
echo.

REM Check for MASM64
where ml64 >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] ml64.exe not found. Run from VS x64 Native Tools Command Prompt.
    exit /b 1
)

echo [1/3] Assembling subagent_fix_masm_x64.asm...
ml64 /c /Fo"subagent_fix_masm_x64.obj" subagent_fix_masm_x64.asm
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Assembly failed
    exit /b 1
)
echo       OK

echo [2/3] Linking with Windows API...
link /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:subagent_fix_masm_x64.exe ^
     subagent_fix_masm_x64.obj ^
     kernel32.lib user32.lib msvcrt.lib
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linking failed
    exit /b 1
)
echo       OK

echo [3/3] Cleaning up...
del subagent_fix_masm_x64.obj 2>nul
echo       OK

echo.
echo ======================================================
echo   BUILD SUCCESS
echo   Output: subagent_fix_masm_x64.exe
echo   Features:
echo     - 50 files per subagent batch
echo     - Recodes C/C++/headers to MASM x64 stubs
echo     - NO MIXING (100%% pure assembly)
echo ======================================================
echo.
echo Run: subagent_fix_masm_x64.exe
echo.
