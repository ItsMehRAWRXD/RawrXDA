@echo off
set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set LINK="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"

if not exist %ML64% (
    echo ML64 not found at specific path, trying generic ml64...
    set ML64=ml64.exe
)
if not exist %LINK% (
    echo LINK not found at specific path, trying generic link...
    set LINK=link.exe
)

echo Building Native Interconnect...

cd /d d:\rawrxd\src

%ML64% /I ..\include /c /Zi /Fo..\build\System_Primitives.obj RawrXD_System_Primitives.asm
%ML64% /I ..\include /c /Zi /Fo..\build\RingBuffer_Consumer.obj RawrXD_RingBuffer_Consumer.asm
%ML64% /I ..\include /c /Zi /Fo..\build\HTTP_Router.obj RawrXD_HTTP_Router.asm
%ML64% /I ..\include /c /Zi /Fo..\build\Model_StateMachine.obj RawrXD_Model_StateMachine.asm
%ML64% /I ..\include /c /Zi /Fo..\build\Swarm_Orchestrator.obj RawrXD_Swarm_Orchestrator.asm
%ML64% /I ..\include /c /Zi /Fo..\build\Agentic_Router.obj RawrXD_Agentic_Router.asm
%ML64% /I ..\include /c /Zi /Fo..\build\Streaming_Formatter.obj RawrXD_Streaming_Formatter.asm
%ML64% /I ..\include /c /Zi /Fo..\build\JSON_Parser.obj RawrXD_JSON_Parser.asm
%ML64% /I ..\include /c /Zi /Fo..\build\GPU_Memory.obj RawrXD_GPU_Memory.asm
%ML64% /I ..\include /c /Zi /Fo..\build\Inference_Engine.obj RawrXD_Inference_Engine.asm
%ML64% /I ..\include /c /Zi /Fo..\build\Complete_Interconnect.obj RawrXD_Complete_Interconnect.asm

cd ..\build

set LIBPATH_MSVC="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64"
set LIBPATH_SDK_UM="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"
set LIBPATH_SDK_UCRT="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"

%LINK% /DLL /OUT:RawrXD_Interconnect.dll /OPT:REF /LTCG /DEBUG:FULL ^
    /LIBPATH:%LIBPATH_MSVC% /LIBPATH:%LIBPATH_SDK_UM% /LIBPATH:%LIBPATH_SDK_UCRT% ^
    System_Primitives.obj RingBuffer_Consumer.obj HTTP_Router.obj ^
    Model_StateMachine.obj Swarm_Orchestrator.obj Agentic_Router.obj ^
    Streaming_Formatter.obj JSON_Parser.obj GPU_Memory.obj ^
    Inference_Engine.obj Complete_Interconnect.obj ^
    kernel32.lib ntdll.lib ws2_32.lib user32.lib gdi32.lib

echo Build complete: RawrXD_Interconnect.dll
