@echo off
setlocal EnableDelayedExpansion

set "VCVARS64=C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
set "VSDEVCMD=C:\VS2022Enterprise\Common7\Tools\VsDevCmd.bat"

if exist "%VCVARS64%" (
  call "%VCVARS64%" >nul 2>&1
) else if exist "%VSDEVCMD%" (
  call "%VSDEVCMD%" -arch=x64 -host_arch=x64 >nul 2>&1
) else (
  echo ERROR: Could not find vcvars64.bat or VsDevCmd.bat.
  exit /b 1
)

if not defined INCLUDE (
  echo ERROR: Visual Studio environment did not set INCLUDE.
  exit /b 1
)

set "SDKINCROOT=C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0"
if exist "%SDKINCROOT%\um\Windows.h" (
  echo(!INCLUDE! | findstr /I /C:"Windows Kits\10\Include\10.0.22621.0\um" >nul
  if errorlevel 1 (
    set "INCLUDE=!INCLUDE!;%SDKINCROOT%\shared;%SDKINCROOT%\um;%SDKINCROOT%\winrt;%SDKINCROOT%\cppwinrt"
  )
)

set "SDKLIBROOT=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0"
if exist "%SDKLIBROOT%\um\x64\kernel32.lib" (
  echo(!LIB! | findstr /I /C:"Windows Kits\10\Lib\10.0.22621.0\um\x64" >nul
  if errorlevel 1 (
    set "LIB=!LIB!;%SDKLIBROOT%\ucrt\x64;%SDKLIBROOT%\um\x64"
  )
)

set "SRC=d:\rawrxd\tools\address_resolver_native.cpp"
set "OUT=d:\rawrxd\tools\address_resolver_native.exe"

cl /nologo /std:c++20 /EHsc /W3 /O2 /DUNICODE /D_UNICODE "%SRC%" /Fe:"%OUT%"
if errorlevel 1 (
  echo ERROR: Build failed.
  exit /b 1
)

echo Built: %OUT%
exit /b 0
