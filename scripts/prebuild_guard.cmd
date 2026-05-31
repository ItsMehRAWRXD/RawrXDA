@echo off
setlocal

set "EXPECTED=d:\rawrxd"
for /f "delims=" %%i in ('git rev-parse --show-toplevel 2^>nul') do set "ROOT=%%i"

if not defined ROOT (
  echo [GUARD] Unable to resolve git root.
  exit /b 1
)

set "ROOT=%ROOT:/=\%"

if /I not "%ROOT%"=="%EXPECTED%" (
  echo [GUARD] Repository root mismatch.
  echo [GUARD] Expected: %EXPECTED%
  echo [GUARD] Actual:   %ROOT%
  exit /b 1
)

echo [GUARD] Repository root verified: %ROOT%
exit /b 0
