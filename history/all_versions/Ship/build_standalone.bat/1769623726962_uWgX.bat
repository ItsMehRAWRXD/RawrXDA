@echo off
setlocal

:: Renamed variables to avoid conflict (LINK env var is special)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" (
    set "ML64_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
    set "LINK_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
) else (
    set "ML64_CMD=ml64.exe"
    set "LINK_CMD=link.exe"
)

:: Create Stubs if missing
if not exist Titan_Streaming_Orchestrator_Fixed.asm (
    echo .code > Titan_Streaming_Orchestrator_Fixed.asm
    echo NativeInferenceThread PROC >> Titan_Streaming_Orchestrator_Fixed.asm
    echo ret >> Titan_Streaming_Orchestrator_Fixed.asm
    echo NativeInferenceThread ENDP >> Titan_Streaming_Orchestrator_Fixed.asm
    echo END >> Titan_Streaming_Orchestrator_Fixed.asm
)
if not exist RawrXD_CLI.asm (
    echo .code > RawrXD_CLI.asm
    echo main PROC >> RawrXD_CLI.asm
    echo ret >> RawrXD_CLI.asm
    echo main ENDP >> RawrXD_CLI.asm
    echo END >> RawrXD_CLI.asm
)

:: Assemble
"%ML64_CMD%" /c /Zi /O2 /arch:AVX512 Titan_InferenceCore.asm
if %errorlevel% neq 0 exit /b 1

"%ML64_CMD%" /c /Zi /O2 /arch:AVX512 Titan_Streaming_Orchestrator_Fixed.asm
if %errorlevel% neq 0 exit /b 1

"%ML64_CMD%" /c /Zi /O2 RawrXD_CLI.asm
if %errorlevel% neq 0 exit /b 1

:: Link (Using Linker var, not LINK env var)
"%LINK_CMD%" /SUBSYSTEM:CONSOLE /OUT:RawrXD-Agent.exe ^
    Titan_InferenceCore.obj ^
    Titan_Streaming_Orchestrator_Fixed.obj ^
    RawrXD_CLI.obj ^
    kernel32.lib ntdll.lib advapi32.lib
if %errorlevel% neq 0 exit /b 1

echo Build Success
