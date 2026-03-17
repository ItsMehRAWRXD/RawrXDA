@echo off
set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\ml64.exe"
set LINK="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\link.exe"

%ML64% /c /Zi /FoSystem_Primitives.obj RawrXD_System_Primitives.asm
%ML64% /c /Zi /FoGPU_Memory.obj RawrXD_GPU_Memory.asm
%ML64% /c /Zi /FoInference_Engine.obj RawrXD_Inference_Engine.asm
%ML64% /c /Zi /FoComplete_Interconnect.obj RawrXD_Complete_Interconnect.asm

echo Linking...
%LINK% /DLL /OUT:RawrXD_Interconnect.dll /OPT:REF /LTCG /DEBUG:FULL ^
    System_Primitives.obj GPU_Memory.obj ^
    Inference_Engine.obj Complete_Interconnect.obj ^
    kernel32.lib ntdll.lib ws2_32.lib user32.lib gdi32.lib

echo Build complete: RawrXD_Interconnect.dll