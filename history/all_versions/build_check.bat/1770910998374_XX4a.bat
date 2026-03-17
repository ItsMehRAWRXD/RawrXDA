@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
cd /d D:\build_verification\build_nmake
cmake --build . --target RawrEngine 2>&1
echo EXIT_CODE=%ERRORLEVEL%
