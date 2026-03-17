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

:: 5. Compile & Link Components

echo [Build] 1/5: Core Interconnect (DLL)...
set "CORE_SOURCES=RawrXD_System_Primitives.asm RawrXD_GPU_Memory.asm RawrXD_Inference_Engine.asm RawrXD_RingBuffer_Consumer.asm RawrXD_HTTP_Router.asm RawrXD_Model_StateMachine.asm RawrXD_Swarm_Orchestrator.asm RawrXD_Agentic_Router.asm RawrXD_Streaming_Formatter.asm RawrXD_JSON_Parser.asm RawrXD_Complete_Interconnect.asm"
set "CORE_OBJS="

for %%s in (%CORE_SOURCES%) do (
    ml64.exe /c /Zi /nologo /Fo"%%~ns.obj" "%%s"
    if !errorlevel! neq 0 exit /b !errorlevel!
    set "CORE_OBJS=!CORE_OBJS! %%~ns.obj"
)

link.exe /DLL /OUT:RawrXD_Interconnect.dll /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    !CORE_OBJS! ^
    kernel32.lib ntdll.lib ws2_32.lib user32.lib gdi32.lib
if %errorlevel% neq 0 exit /b %errorlevel%


echo [Build] 2/5: Titan Unified Inference Engine (DLL)...
ml64.exe /c /Zi /nologo /Fo"RawrXD_Titan_UNIFIED.obj" "RawrXD_Titan_UNIFIED.asm"
if %errorlevel% neq 0 exit /b %errorlevel%

link.exe /DLL /OUT:RawrXD_Titan.dll /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    RawrXD_Titan_UNIFIED.obj ^
    kernel32.lib ntdll.lib user32.lib advapi32.lib
if %errorlevel% neq 0 exit /b %errorlevel%


echo [Build] 3/5: Native Pattern Host (EXE)...
ml64.exe /c /Zi /nologo /Fo"RawrXD_NativeHost.obj" "RawrXD_NativeHost.asm"
if %errorlevel% neq 0 exit /b %errorlevel%

link.exe /SUBSYSTEM:CONSOLE /OUT:RawrXD_NativeHost.exe /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    RawrXD_NativeHost.obj ^
    kernel32.lib user32.lib
if %errorlevel% neq 0 exit /b %errorlevel%


echo [Build] 4/5: Pattern Bridge (DLL)...
ml64.exe /c /Zi /nologo /Fo"RawrXD_PatternBridge.obj" "RawrXD_PatternBridge.asm"
if %errorlevel% neq 0 exit /b %errorlevel%

:: Check if DEF file exists
set "DEF_FLAG="
if exist "RawrXD_PatternBridge.def" set "DEF_FLAG=/DEF:RawrXD_PatternBridge.def"

link.exe /DLL /OUT:RawrXD_PatternBridge.dll /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    !DEF_FLAG! RawrXD_PatternBridge.obj ^
    kernel32.lib
if %errorlevel% neq 0 exit /b %errorlevel%


echo [Build] 5/5: LSP Integration (Static Lib)...
set "LSP_SOURCES=RawrXD_LSP_Core.asm RawrXD_LSP_Handshake_Ext.asm"
set "LSP_OBJS="
for %%s in (%LSP_SOURCES%) do (
    ml64.exe /c /Zi /nologo /Fo"%%~ns.obj" "%%s"
    if !errorlevel! neq 0 exit /b !errorlevel!
    set "LSP_OBJS=!LSP_OBJS! %%~ns.obj"
)

lib.exe /OUT:RawrXD_LSP.lib /NOLOGO !LSP_OBJS!
if %errorlevel% neq 0 exit /b %errorlevel%


echo [Success] All RawrXD assembly components built.
goto :End

:Clean
echo [Clean] Removing build artifacts...
del /q *.obj *.dll *.lib *.exp *.pdb *.ilk >nul 2>&1
echo [Clean] Done.
goto :End

:End
endlocal
