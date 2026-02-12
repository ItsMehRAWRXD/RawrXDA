@echo off
REM Auto-detect VS toolchain: try D: first, then C:
set "_VCVARS="
if exist "D:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "_VCVARS=D:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined _VCVARS if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "_VCVARS=C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined _VCVARS if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if defined _VCVARS (
    call "%_VCVARS%"
) else (
    echo [WARN] vcvars64.bat not found — relying on PATH
)
cd /d D:\rawrxd
cmake -S . -B build -G Ninja 2>&1
