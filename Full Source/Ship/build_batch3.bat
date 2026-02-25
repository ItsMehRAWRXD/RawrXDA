@echo off
setlocal enabledelayedexpansion
cd /d "D:\RawrXD\Ship"
set "MSVC=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717"
set "WINSDK=C:\Program Files (x86)\Windows Kits\10"
set "PATH=!MSVC!\bin\Hostx64\x64;!PATH!"
set "INCS=/I"!WINSDK!\Include\10.0.22621.0\um" /I"!WINSDK!\Include\10.0.22621.0\shared" /I"!WINSDK!\Include\10.0.22621.0\ucrt" /I"!MSVC!\include""
set "LIBS=/link /LIBPATH:"!MSVC!\lib\onecore\x64" /LIBPATH:"!WINSDK!\Lib\10.0.22621.0\um\x64" /LIBPATH:"!WINSDK!\Lib\10.0.22621.0\ucrt\x64" kernel32.lib user32.lib gdi32.lib shell32.lib"

echo Building RawrXD_AgentPool.dll...
cl.exe %INCS% /O2 /DNDEBUG /MD /EHsc RawrXD_AgentPool.cpp %LIBS% /DLL /OUT:RawrXD_AgentPool.dll 2>&1 | find ".dll" && echo OK || echo FAIL

echo Building RawrXD_MemoryManager.dll...
cl.exe %INCS% /O2 /DNDEBUG /MD /EHsc RawrXD_MemoryManager.cpp %LIBS% /DLL /OUT:RawrXD_MemoryManager.dll 2>&1 | find ".dll" && echo OK || echo FAIL

echo Building RawrXD_ModelLoader.dll...
cl.exe %INCS% /O2 /DNDEBUG /MD /EHsc RawrXD_ModelLoader.cpp %LIBS% /DLL /OUT:RawrXD_ModelLoader.dll 2>&1 | find ".dll" && echo OK || echo FAIL

echo.
echo Summary:
dir RawrXD_AgentPool.dll RawrXD_MemoryManager.dll RawrXD_ModelLoader.dll 2>nul | find ".dll"
