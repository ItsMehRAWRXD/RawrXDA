@echo off
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
cl /c /std:c++20 /EHsc /nologo /Od /Zi /RTC1 /MDd /DDEBUG /D_DEBUG /FS /Fdobj\manual-debug.pdb /Foobj\manual_AgentOllamaClient.obj /ID:\rawrxd\src /ID:\rawrxd\include /ID:\rawrxd\3rdparty /ID:\rawrxd\3rdparty\ggml\include /DRAWXD_BUILD /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX D:\rawrxd\src\agentic\AgentOllamaClient.cpp > manual_compile_out.txt 2>&1
set EC=%ERRORLEVEL%
if exist obj\manual_AgentOllamaClient.obj dumpbin /headers obj\manual_AgentOllamaClient.obj > manual_obj_headers.txt
exit /b %EC%
