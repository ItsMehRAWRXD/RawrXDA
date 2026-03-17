@echo off
REM Link RawrXD-SovereignLoader.dll with updated MASM kernels

cd /d D:\temp\RawrXD-agentic-ide-production\build-sovereign

call C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat >nul 2>&1

echo Linking DLL with updated MASM kernels...

link.exe ^
  /DLL ^
  /OUT:bin\RawrXD-SovereignLoader.dll ^
  /IMPLIB:bin\RawrXD-SovereignLoader.lib ^
  sovereign_loader.obj ^
  universal_quant_kernel.obj ^
  beaconism_dispatcher.obj ^
  dimensional_pool.obj ^
  kernel32.lib

if %ERRORLEVEL% EQU 0 (
  echo SUCCESS: DLL rebuilt with symbol aliases
  echo Symbols enabled:
  echo   - load_model_beacon ^> ManifestVisualIdentity
  echo   - validate_beacon_signature ^> ProcessSignal  
  echo   - quantize_tensor_zmm ^> EncodeToPoints
  echo   - dequantize_tensor_zmm ^> DecodeFromPoints
  echo   - dimensional_pool_init ^> CreateWeightPool
  echo.
  echo Running test_loader.exe...
  bin\test_loader.exe
) else (
  echo FAILED: Link error
  exit /b 1
)
