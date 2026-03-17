@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

:: -----------------------------------------------------------------------------
:: RawrXD Assembly Build System (Complete)
:: -----------------------------------------------------------------------------
:: Clears any previous pollution of LINK variable which causes crashes
set "LINK="

:: 1. Setup Environment (Robust)
echo [Build] Setting up environment...

:: Try Custom Enterprise Path first (User Environment)
if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)

:: Verify Tools or Use Fallback Paths
where ml64.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [Build] ML64 not in path, using hardcoded fallback...
    set "ML64_PATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
    set "LINK_PATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
    
    if exist "!ML64_PATH!" (
        set "ML64="!ML64_PATH!""
        set "LINKER="!LINK_PATH!""
        set "LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64;C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64"
    ) else (
        echo [Error] Compiler tools not found.
        exit /b 1
    )
) else (
    set "ML64=ml64.exe"
    set "LINKER=link.exe"
)

:: 2. Compile Core Interconnect
echo [Build] 1/5: Core Interconnect (DLL)...
set "CORE_SOURCES=RawrXD_System_Primitives.asm RawrXD_GPU_Memory.asm RawrXD_Inference_Engine.asm RawrXD_RingBuffer_Consumer.asm RawrXD_HTTP_Router.asm RawrXD_Model_StateMachine.asm RawrXD_Swarm_Orchestrator.asm RawrXD_Agentic_Router.asm RawrXD_Streaming_Formatter.asm RawrXD_JSON_Parser.asm RawrXD_Complete_Interconnect.asm"
set "CORE_OBJS="

for %%s in (%CORE_SOURCES%) do (
    %ML64% /c /Zi /nologo /Fo"%%~ns.obj" "%%s"
    if !errorlevel! neq 0 exit /b !errorlevel!
    set "CORE_OBJS=!CORE_OBJS! %%~ns.obj"
)

%LINKER% /DLL /OUT:RawrXD_Interconnect.dll /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    !CORE_OBJS! ^
    kernel32.lib ntdll.lib ws2_32.lib user32.lib gdi32.lib
if %errorlevel% neq 0 exit /b %errorlevel%

:: 3. Compile Titan Unified
echo [Build] 2/5: Titan Unified Inference Engine (DLL)...
%ML64% /c /Zi /nologo /Fo"RawrXD_Titan_UNIFIED.obj" "RawrXD_Titan_UNIFIED.asm"
if %errorlevel% neq 0 exit /b %errorlevel%

%LINKER% /DLL /OUT:RawrXD_Titan.dll /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    RawrXD_Titan_UNIFIED.obj ^
    kernel32.lib ntdll.lib user32.lib advapi32.lib
if %errorlevel% neq 0 exit /b %errorlevel%

:: 4. Compile Native Host
echo [Build] 3/5: Native Pattern Host (EXE)...
%ML64% /c /Zi /nologo /Fo"RawrXD_NativeHost.obj" "RawrXD_NativeHost.asm"
if %errorlevel% neq 0 exit /b %errorlevel%

%LINKER% /SUBSYSTEM:CONSOLE /OUT:RawrXD_NativeHost.exe /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    RawrXD_NativeHost.obj ^
    kernel32.lib user32.lib
if %errorlevel% neq 0 exit /b %errorlevel%

:: 5. Compile Pattern Bridge
echo [Build] 4/5: Pattern Bridge (DLL)...
%ML64% /c /Zi /nologo /Fo"RawrXD_PatternBridge.obj" "RawrXD_PatternBridge.asm"
if %errorlevel% neq 0 exit /b %errorlevel%

set "DEF_FLAG="
if exist "RawrXD_PatternBridge.def" set "DEF_FLAG=/DEF:RawrXD_PatternBridge.def"

%LINKER% /DLL /OUT:RawrXD_PatternBridge.dll /OPT:REF /LTCG /DEBUG:FULL /NOLOGO ^
    !DEF_FLAG! RawrXD_PatternBridge.obj ^
    kernel32.lib
if %errorlevel% neq 0 exit /b %errorlevel%

:: 6. Compile LSP Lib
echo [Build] 5/5: LSP Integration (Static Lib)...
set "LSP_SOURCES=RawrXD_LSP_Core.asm RawrXD_LSP_Handshake_Ext.asm"
set "LSP_OBJS="
for %%s in (%LSP_SOURCES%) do (
    %ML64% /c /Zi /nologo /Fo"%%~ns.obj" "%%s"
    if !errorlevel! neq 0 exit /b !errorlevel!
    set "LSP_OBJS=!LSP_OBJS! %%~ns.obj"
)

:: Find lib.exe (using Linker path assumption)
set "LIBTOOL=lib.exe"
if defined ML64_PATH if not exist "lib.exe" (
    :: Extract path from LINK_PATH
    for %%I in (!LINK_PATH!) do set "LIBTOOL=%%~dpIlib.exe"
)

"%LIBTOOL%" /OUT:RawrXD_LSP.lib /NOLOGO !LSP_OBJS!
if %errorlevel% neq 0 exit /b %errorlevel%

echo [Success] All RawrXD assembly components built.
endlocal
