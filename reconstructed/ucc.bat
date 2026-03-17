@echo off
setlocal enabledelayedexpansion

:: Universal C/C++ Compiler (Windows) - ucc.bat
:: Usage: ucc.bat <input.cpp> [-o <output.exe>] [extra flags]

if "%~1"=="" (
  echo Usage: %~n0 source.(c^|cpp^|cc) -o out.exe [flags]
  exit /b 1
)

set "SRC=%~1"
set "OUT="
set "EXTRA="

:: Parse remaining args for -o and extras
shift
:parse_args
if "%~1"=="" goto after_parse
if /i "%~1"=="-o" (
  shift
  if "%~1"=="" (
    echo [UCC] ERROR: -o requires an output filename
    exit /b 1
  )
  set "OUT=%~1"
) else (
  set "EXTRA=%EXTRA% %~1"
)
shift
goto parse_args

:after_parse
if not defined OUT (
  for %%F in ("%SRC%") do set "OUT=%%~nF.exe"
)

set LIBS=ws2_32

:: Try MSVC cl.exe (multiple possible editions)
set VSVCVARS=
for %%P in ("C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat") do (
  if exist %%~P (
    set VSVCVARS=%%~P
    goto :found_vcvars
  )
)
:found_vcvars
if defined VSVCVARS (
  call "%VSVCVARS%" >nul 2>&1
  where cl >nul 2>&1
  if %ERRORLEVEL% EQU 0 (
    echo [UCC] Using MSVC cl.exe
    cl /nologo /std:c++17 /O2 "%SRC%" /Fe:"%OUT%" %EXTRA% %LIBS%.lib
    if %ERRORLEVEL% EQU 0 exit /b 0
  )
)

:: Try LLVM clang++
where clang++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
  echo [UCC] Using clang++
  clang++ -std=c++17 -O2 "%SRC%" -o "%OUT%" %EXTRA% -l%LIBS%
  if %ERRORLEVEL% EQU 0 exit /b 0
)

:: Try g++ on PATH
where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
  echo [UCC] Using g++
  g++ -std=c++17 -O2 "%SRC%" -o "%OUT%" %EXTRA% -l%LIBS%
  if %ERRORLEVEL% EQU 0 exit /b 0
)

:: Try MinGW default location
if exist "C:\MinGW\bin\g++.exe" (
  echo [UCC] Using MinGW g++
  "C:\MinGW\bin\g++.exe" -std=c++17 -O2 "%SRC%" -o "%OUT%" %EXTRA% -l%LIBS%
  if %ERRORLEVEL% EQU 0 exit /b 0
)

:: Try Zig as a portable C++ compiler
where zig >nul 2>&1
if %ERRORLEVEL% EQU 0 (
  echo [UCC] Using zig c++
  zig c++ -std=c++17 -O2 "%SRC%" -o "%OUT%" %EXTRA% -l%LIBS%
  if %ERRORLEVEL% EQU 0 exit /b 0
)

echo [UCC] ERROR: No suitable C/C++ compiler found.
echo Install Visual Studio Build Tools or LLVM/MinGW and try again.
exit /b 1
