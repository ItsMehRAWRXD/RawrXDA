@echo off
REM RawrXD Engine Runner - v0.1.0 MVP
REM
REM Usage: run_engine.bat path\to\model.gguf
REM Example: run_engine.bat ..\models\Mistral-7B-Instruct-v0.1.Q4_K_M.gguf

setlocal enabledelayedexpansion

if "%~1"=="" (
  echo.
  echo ========================================
  echo RawrXD Engine v0.1.0 MVP
  echo ========================================
  echo.
  echo Usage: run_engine.bat ^<path-to-model.gguf^>
  echo.
  echo Example:
  echo   run_engine.bat ..\models\Mistral-7B-Instruct-v0.1.Q4_K_M.gguf
  echo.
  echo The engine will start on http://localhost:8080
  echo The IDE will connect to this endpoint.
  echo.
  echo Model requirements:
  echo  - GGUF format (quantized recommended for speed)
  echo  - 7B-13B parameter models work best
  echo  - 4GB+ VRAM for Mistral 7B Q4
  echo.
  exit /b 1
)

set MODEL=%~1
set PORT=8080

if not exist "%MODEL%" (
  echo Error: Model file not found: %MODEL%
  exit /b 1
)

echo.
echo ========================================
echo Starting RawrXD Engine
echo ========================================
echo Model:  %MODEL%
echo Port:   %PORT%
echo.
echo Loading... this may take 30-60 seconds
echo.

RawrEngine.exe --model "%MODEL%" --port %PORT%

pause
