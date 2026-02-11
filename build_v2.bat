@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
ml64.exe /c /Zi /Fo"d:\rawrxd\RawrXD_IDE_unified.obj" "d:\rawrxd\RawrXD_IDE_unified.asm" > "d:\rawrxd\build_v2.txt" 2>&1
echo EXIT_CODE=%ERRORLEVEL% >> "d:\rawrxd\build_v2.txt"
