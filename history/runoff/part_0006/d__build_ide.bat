@echo off
cd /d D:\RawrXD\Ship
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
cl /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /EHsc /std:c++17 /W1 RawrXD_Win32_IDE.cpp /link user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib comdlg32.lib advapi32.lib shlwapi.lib ws2_32.lib wininet.lib
