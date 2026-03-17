@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cd /d D:\rawrxd

echo === Compiling FlagshipFeatures ===
cl.exe /std:c++20 /c /EHsc /I"src" /I"src/core" /I"src/win32app" /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Fod:\flagship_test1.obj src\win32app\Win32IDE_FlagshipFeatures.cpp
echo EXIT_CODE: %ERRORLEVEL%

echo === Compiling ProvableAgent ===
cl.exe /std:c++20 /c /EHsc /I"src" /I"src/core" /I"src/win32app" /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Fod:\flagship_test2.obj src\win32app\Win32IDE_ProvableAgent.cpp
echo EXIT_CODE: %ERRORLEVEL%

echo === Compiling AirgappedEnterprise ===
cl.exe /std:c++20 /c /EHsc /I"src" /I"src/core" /I"src/win32app" /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Fod:\flagship_test3.obj src\win32app\Win32IDE_AirgappedEnterprise.cpp
echo EXIT_CODE: %ERRORLEVEL%
