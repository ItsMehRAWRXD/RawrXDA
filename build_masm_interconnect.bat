@echo off
setlocal enabledelayedexpansion

echo Building RawrXD Interconnect Assembly Components...
echo.

REM Set paths to MASM64
set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\ml64.exe"
if not exist %ML64% set ML64="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\ml64.exe"
if not exist %ML64% set ML64=ml64.exe

REM Check if ml64.exe exists anywhere
where %ML64% >nul 2>nul
if %errorlevel% neq 0 (
    if not exist %ML64% (
        echo ERROR: ml64.exe not found. Please run this from a VS Developer Command Prompt or check the path.
        exit /b 1
    )
)

REM Create output directory for object files
if not exist "obj" mkdir obj

echo Compiling System Primitives...
%ML64% /c /Zi /Foobj\System_Primitives.obj src\masm\interconnect\RawrXD_System_Primitives.asm
if errorlevel 1 goto :error

echo Compiling GPU Memory Manager...
%ML64% /c /Zi /Foobj\GPU_Memory.obj src\masm\interconnect\RawrXD_GPU_Memory.asm
if errorlevel 1 goto :error

echo Compiling Inference Engine...
%ML64% /c /Zi /Foobj\Inference_Engine.obj src\masm\interconnect\RawrXD_Inference_Engine.asm
if errorlevel 1 goto :error

echo Compiling Stubs...
%ML64% /c /Zi /Foobj\RingBuffer_Consumer.obj src\masm\interconnect\RawrXD_RingBuffer_Consumer.asm
%ML64% /c /Zi /Foobj\HTTP_Router.obj src\masm\interconnect\RawrXD_HTTP_Router.asm
%ML64% /c /Zi /Foobj\Model_StateMachine.obj src\masm\interconnect\RawrXD_Model_StateMachine.asm
%ML64% /c /Zi /Foobj\Swarm_Orchestrator.obj src\masm\interconnect\RawrXD_Swarm_Orchestrator.asm
%ML64% /c /Zi /Foobj\Agentic_Router.obj src\masm\interconnect\RawrXD_Agentic_Router.asm
%ML64% /c /Zi /Foobj\Streaming_Formatter.obj src\masm\interconnect\RawrXD_Streaming_Formatter.asm
%ML64% /c /Zi /Foobj\JSON_Parser.obj src\masm\interconnect\RawrXD_JSON_Parser.asm
if errorlevel 1 goto :error

echo Compiling Complete Interconnect...
%ML64% /c /Zi /Foobj\Complete_Interconnect.obj src\masm\interconnect\RawrXD_Complete_Interconnect.asm
if errorlevel 1 goto :error

echo.
echo All assembly files compiled successfully!
echo.
echo Object files created in obj\ directory:
dir obj\*.obj
echo.
echo To link these into a DLL, run the link step or update CMakeLists.txt
echo.
goto :end

:error
echo.
echo ERROR: Compilation failed!
exit /b 1

:end
echo Build script completed.
