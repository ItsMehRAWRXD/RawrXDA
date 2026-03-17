@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
set ML64="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set LINK="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"

%ML64% /c /Zi /FoSystem_Primitives.obj RawrXD_System_Primitives.asm
%ML64% /c /Zi /FoGPU_Memory.obj RawrXD_GPU_Memory.asm
%ML64% /c /Zi /FoInference_Engine.obj RawrXD_Inference_Engine.asm
%ML64% /c /Zi /FoRingBuffer_Consumer.obj RawrXD_RingBuffer_Consumer.asm
%ML64% /c /Zi /FoHTTP_Router.obj RawrXD_HTTP_Router.asm
%ML64% /c /Zi /FoModel_StateMachine.obj RawrXD_Model_StateMachine.asm
%ML64% /c /Zi /FoSwarm_Orchestrator.obj RawrXD_Swarm_Orchestrator.asm
%ML64% /c /Zi /FoAgentic_Router.obj RawrXD_Agentic_Router.asm
%ML64% /c /Zi /FoStreaming_Formatter.obj RawrXD_Streaming_Formatter.asm
%ML64% /c /Zi /FoJSON_Parser.obj RawrXD_JSON_Parser.asm
%ML64% /c /Zi /FoComplete_Interconnect.obj RawrXD_Complete_Interconnect.asm

echo Linking...
%LINK% /DLL /OUT:RawrXD_Interconnect.dll /OPT:REF /LTCG /DEBUG:FULL ^
    System_Primitives.obj GPU_Memory.obj Inference_Engine.obj ^
    RingBuffer_Consumer.obj HTTP_Router.obj Model_StateMachine.obj ^
    Swarm_Orchestrator.obj Agentic_Router.obj Streaming_Formatter.obj ^
    JSON_Parser.obj Complete_Interconnect.obj ^
    kernel32.lib ntdll.lib ws2_32.lib user32.lib gdi32.lib

echo Build complete: RawrXD_Interconnect.dll
