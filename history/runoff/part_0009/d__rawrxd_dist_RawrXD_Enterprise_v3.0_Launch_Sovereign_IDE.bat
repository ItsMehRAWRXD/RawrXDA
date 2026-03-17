@echo off
title RawrXD Sovereign Enterprise v3.0 - Launch Sequence
color 0A

echo.
echo  ╔══════════════════════════════════════════════════════════════╗
echo  ║  RAWRXD SOVEREIGN ENTERPRISE v3.0 - LAUNCH SEQUENCE          ║
echo  ║  800B Inference │ 0.7 t/s │ NVMe Thermal Dashboard           ║
echo  ╚══════════════════════════════════════════════════════════════╝
echo.

:: Step 1: Initialize thermal telemetry + MMF pipes
echo  [1/3] Initializing pocket_lab.exe (MMF: SOVEREIGN_STATS + NVME_TEMPS)...
start /B "" "%~dp0pocket_lab.exe"
timeout /t 2 /nobreak >nul

:: Step 2: Verify MMF pipes are online
echo  [2/3] Verifying thermal telemetry online...
timeout /t 1 /nobreak >nul

:: Step 3: Launch the IDE with Thermal HUD auto-dock
echo  [3/3] Launching RawrXD-AgenticIDE.exe...
start "" "%~dp0RawrXD-AgenticIDE.exe"

echo.
echo  ████████████████████████████████████████████████████████████████
echo  ██  ✓ SOVEREIGN ENTERPRISE v3.0 - ONLINE                      ██
echo  ██  ✓ Thermal Dashboard: View → Model ^& Inference → Thermal   ██
echo  ████████████████████████████████████████████████████████████████
echo.

timeout /t 3 /nobreak >nul
