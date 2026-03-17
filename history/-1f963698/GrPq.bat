@echo off
setlocal EnableDelayedExpansion

:: -----------------------------------------------------------------------------
:: RawrXD Assembly Build System
:: -----------------------------------------------------------------------------

:: 1. Handle "clean" argument
if /i "%1"=="clean" goto :Clean

:: 2. Setup Environment
echo [Build] Setting up environment...
if defined VCToolsInstallDir goto :EnvReady

:: Try standard VS2022 locations
set "VS_PATH="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VS_PATH=C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"

if defined VS_PATH (
    call "!VS_PATH!"
) else (
    echo [Error] Could not locate vcvars64.bat. Please run from VS Developer Command Prompt.
    exit /b 1
)

:EnvReady

:: 3. Verify Tools
where ml64.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [Error] ml64.exe not found in PATH.
    exit /b 1
)
where link.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [Error] link.exe not found in PATH.
    exit /b 1
)

:: 4. Define Source Files
set "SOURCES=RawrXD_System_Primitives.asm RawrXD_GPU_Memory.asm RawrXD_Inference_Engine.asm RawrXD_RingBuffer_Consumer.asm RawrXD_HTTP_Router.asm RawrXD_Model_StateMachine.asm RawrXD_Swarm_Orchestrator.asm RawrXD_Agentic_Router.asm RawrXD_Streaming_Formatter.asm RawrXD_JSON_Parser.asm RawrXD_Complete_Interconnect.asm"

set "OBJ_FILES="

:: 5. Compile Loop
echo [Build] Compiling Assembly Units...
for %%s in (%SOURCES%) do (
    set "SRC=%%s"
    set "OBJ=%%~ns.obj"
    
    echo   - Compiling !SRC!...
    ml64.exe /c /Zi /nologo /Fo"!OBJ!" "!SRC!"
    
    if !errorlevel! neq 0 (
        echo [Error] Failed to compile !SRC!
        exit /b !errorlevel!
    )
    
    set "OBJ_FILES=!OBJ_FILES! !OBJ!"
)

:: 6. Link
echo [Build] Linking RawrXD_Interconnect.dll...
link.exe /DLL /OUT:RawrXD_Interconnect.dll /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    !OBJ_FILES! ^
    kernel32.lib ntdll.lib ws2_32.lib user32.lib gdi32.lib

if %errorlevel% neq 0 (
    echo [Error] Linking failed.
    exit /b %errorlevel%
)

echo [Success] RawrXD_Interconnect.dll created successfully.
goto :End

:Clean
echo [Clean] Removing build artifacts...
del /q *.obj *.dll *.lib *.exp *.pdb *.ilk >nul 2>&1
echo [Clean] Done.
goto :End

:End
endlocal
