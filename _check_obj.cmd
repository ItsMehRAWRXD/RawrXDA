@echo off
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if not exist obj\agentic_AgentOllamaClient.obj (
  echo missing
  exit /b 1
)
dumpbin /directives obj\agentic_AgentOllamaClient.obj > _agentic_dump.txt
for %%I in (_agentic_dump.txt) do echo dump_size=%%~zI
findstr /i /c:"DEFAULTLIB" _agentic_dump.txt
findstr /i /c:"RuntimeLibrary" _agentic_dump.txt
findstr /i /c:"LTCG" _agentic_dump.txt
