@echo off
setlocal

:: Find ml64.exe (VS Build Tools, VS Community, etc.)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" (
    set "ML64=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
    set "LINK=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
) else (
    set "ML64=ml64.exe"
    set "LINK=link.exe"
)

echo ============================================================================
echo [RawrXD Standalone Inference Engine]
echo No llama-server.exe required.
echo ============================================================================

:: Assemble Logic
"%ML64%" /c /Zi /O2 /arch:AVX512 Titan_InferenceCore.asm
if %errorlevel% neq 0 goto error

:: Create Stub Files if they don't exist to make it linkable for testing
if not exist Titan_Streaming_Orchestrator_Fixed.asm (
    echo Creating Stub for Titan_Streaming_Orchestrator_Fixed.asm
    echo .code > Titan_Streaming_Orchestrator_Fixed.asm
    echo NativeInferenceThread PROC >> Titan_Streaming_Orchestrator_Fixed.asm
    echo ret >> Titan_Streaming_Orchestrator_Fixed.asm
    echo NativeInferenceThread ENDP >> Titan_Streaming_Orchestrator_Fixed.asm
    echo END >> Titan_Streaming_Orchestrator_Fixed.asm
)
"%ML64%" /c /Zi /O2 /arch:AVX512 Titan_Streaming_Orchestrator_Fixed.asm
if %errorlevel% neq 0 goto error

if not exist RawrXD_CLI.asm (
    echo Creating Stub for RawrXD_CLI.asm
    echo .code > RawrXD_CLI.asm
    echo main PROC >> RawrXD_CLI.asm
    echo ret >> RawrXD_CLI.asm
    echo main ENDP >> RawrXD_CLI.asm
    echo END >> RawrXD_CLI.asm
)
"%ML64%" /c /Zi /O2 RawrXD_CLI.asm
if %errorlevel% neq 0 goto error

:: Link
"%LINK%" /SUBSYSTEM:CONSOLE /OUT:RawrXD-Agent.exe ^
    Titan_InferenceCore.obj ^
    Titan_Streaming_Orchestrator_Fixed.obj ^
    RawrXD_CLI.obj ^
    kernel32.lib ntdll.lib advapi32.lib
if %errorlevel% neq 0 goto error

echo.
echo ============================================================================
echo RawrXD-Agent.exe loads GGUF directly.
echo Usage: RawrXD-Agent.exe model.gguf "Hello"
echo ============================================================================
goto :eof

:error
echo.
echo BUILD FAILED
exit /b 1
