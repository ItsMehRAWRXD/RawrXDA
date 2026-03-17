@echo off
REM Set up VS environment
for /f "tokens=*" %%i in ('dir /b "C:\Program Files\Microsoft Visual Studio\2022\*"') do (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\%%i"
    goto found
)
:found
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
cd /d D:\RawrXD\Ship
cl /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /EHsc /std:c++17 /W1 RawrXD_Win32_IDE.cpp /link user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib comdlg32.lib advapi32.lib shlwapi.lib ws2_32.lib wininet.lib
pause
