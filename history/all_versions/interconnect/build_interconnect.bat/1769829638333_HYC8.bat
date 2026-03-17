@echo off
set ML64=ml64.exe
set LINK=link.exe
set OUTDIR=build

if not exist %OUTDIR% mkdir %OUTDIR%

echo [1/7] Assembling RawrXD_RingBuffer_Consumer...
%ML64% /c /Fo%OUTDIR%\RawrXD_RingBuffer_Consumer.obj /W3 /nologo /Zi /Zd RawrXD_RingBuffer_Consumer.asm

echo [2/7] Assembling RawrXD_HTTP_Router...
%ML64% /c /Fo%OUTDIR%\RawrXD_HTTP_Router.obj /W3 /nologo /Zi /Zd RawrXD_HTTP_Router.asm

echo [3/7] Assembling RawrXD_Model_StateMachine...
%ML64% /c /Fo%OUTDIR%\RawrXD_Model_StateMachine.obj /W3 /nologo /Zi /Zd RawrXD_Model_StateMachine.asm

echo [4/7] Assembling RawrXD_Swarm_Orchestrator...
%ML64% /c /Fo%OUTDIR%\RawrXD_Swarm_Orchestrator.obj /W3 /nologo /Zi /Zd RawrXD_Swarm_Orchestrator.asm

echo [5/7] Assembling RawrXD_Agentic_Router...
%ML64% /c /Fo%OUTDIR%\RawrXD_Agentic_Router.obj /W3 /nologo /Zi /Zd RawrXD_Agentic_Router.asm

echo [6/7] Assembling RawrXD_Streaming_Formatter...
%ML64% /c /Fo%OUTDIR%\RawrXD_Streaming_Formatter.obj /W3 /nologo /Zi /Zd RawrXD_Streaming_Formatter.asm

echo [7/7] Assembling RawrXD_JSON_Parser...
%ML64% /c /Fo%OUTDIR%\RawrXD_JSON_Parser.obj /W3 /nologo /Zi /Zd RawrXD_JSON_Parser.asm

echo [8/8] Assembling RawrXD_Shared_Data...
%ML64% /c /Fo%OUTDIR%\RawrXD_Shared_Data.obj /W3 /nologo /Zi /Zd RawrXD_Shared_Data.asm

echo Linking Interconnect DLL...
%LINK% /DLL /OUT:%OUTDIR%\RawrXD_Interconnect.dll /OPT:REF /LTCG %OUTDIR%\*.obj kernel32.lib ws2_32.lib user32.lib gdi32.lib ntdll.lib

echo Done.
pause
