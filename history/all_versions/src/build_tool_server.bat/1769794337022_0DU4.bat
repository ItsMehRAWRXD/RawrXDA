@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d D:\RawrXD\src
cl.exe /std:c++17 /EHsc ^
  /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um" ^
  /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared" ^
  /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt" ^
  /I"D:\RawrXD\include" ^
  /I"D:\RawrXD\src" ^
  tool_server.cpp ^
  backend\agentic_tools.cpp ^
  backend\ollama_client.cpp ^
  tools\file_ops.cpp ^
  tools\git_client.cpp ^
  /Fe:tool_server.exe ^
  /link ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" ^
  /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64" ^
  ws2_32.lib user32.lib advapi32.lib shell32.lib winhttp.lib

if %errorlevel% equ 0 (
    echo.
    echo Build successful! Starting server on port 15099...
    tool_server.exe --port 15099
)
