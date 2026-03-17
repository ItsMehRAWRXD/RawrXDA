@echo off
setlocal enabledelayedexpansion

set M=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717
set W=C:\Program Files (x86)\Windows Kits\10
set SDK=10.0.22621.0

cd /d D:\RawrXD\Ship

"!M!\bin\Hostx64\x64\cl.exe" ^
  /O2 /DNDEBUG /MD /EHsc /std:c++17 ^
  /I"!M!\include" ^
  /I"!W!\Include\!SDK!\um" ^
  /I"!W!\Include\!SDK!\shared" ^
  RawrXD_InferenceEngine_Win32.cpp ^
  /link ^
  /LIBPATH:"!M!\lib\onecore\x64" ^
  /LIBPATH:"!W!\Lib\!SDK!\um\x64" ^
  /OUT:RawrXD_InferenceEngine_Win32.dll ^
  /DLL ^
  kernel32.lib user32.lib gdi32.lib shell32.lib

if %ERRORLEVEL% EQU 0 (
    dir RawrXD_InferenceEngine_Win32.dll
    echo Build successful
) else (
    echo Build failed with error %ERRORLEVEL%
)
