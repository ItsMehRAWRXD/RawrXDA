@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\rawrxd
echo === Building RawrXD Amphibious Full Working ===
ml64 /c /Zi RawrXD_Amphibious_Full_Working.asm
if %errorlevel% neq 0 exit /b 1
link /subsystem:console /entry:main /out:RawrXD_Amphibious_Full.exe RawrXD_Amphibious_Full_Working.obj "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\kernel32.lib" "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\user32.lib"
if %errorlevel% neq 0 exit /b 1
echo === Build Complete - Executing ===
echo.
RawrXD_Amphibious_Full.exe
echo.
echo === Checking telemetry artifact ===
if exist D:\rawrxd\amphibious_full.json (
    echo --- Telemetry JSON ---
    type D:\rawrxd\amphibious_full.json
)
