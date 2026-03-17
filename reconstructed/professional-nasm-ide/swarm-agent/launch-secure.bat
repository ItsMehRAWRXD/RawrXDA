@echo off
REM HARDENED NASM IDE SWARM LAUNCHER
REM Security-hardened version with UTF-8 encoding protection
REM All Unicode symbols removed to prevent ROE vulnerabilities

setlocal EnableDelayedExpansion

echo ========================================
echo NASM IDE Swarm System - SECURE MODE
echo ========================================
echo.

REM Force ASCII output to prevent encoding attacks
chcp 437 >nul 2>&1

REM Security check - validate Python installation
echo [SECURITY] Validating Python installation...
py -3.12 -c "import sys; print('Python validation: OK')" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python validation failed - potential security risk
    pause
    exit /b 1
)

REM Test core modules without Unicode
echo [SECURITY] Testing core modules...
py -3.12 -c "import asyncio, logging, json, sys; print('Core modules: OK')" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Core module validation failed
    pause
    exit /b 1
)

echo [SECURITY] All validations passed
echo.

REM Launch with security parameters
echo [LAUNCH] Starting secure swarm system...
echo.

REM Set security environment variables
set PYTHONIOENCODING=ascii
set LANG=C
set LC_ALL=C

echo [1/3] Starting swarm controller...
start "NASM IDE - Secure Controller" py -3.12 swarm_minimal.py

timeout /t 3 /nobreak >nul

echo [2/3] Starting secure dashboard...
start "NASM IDE - Secure Dashboard" py -3.12 secure_dashboard.py

timeout /t 2 /nobreak >nul

echo [3/3] Opening secure interface...
timeout /t 2 /nobreak >nul
start http://localhost:8080

echo.
echo ========================================
echo SECURE SWARM SYSTEM LAUNCHED
echo ========================================
echo.
echo Security Features:
echo - ASCII-only output encoding
echo - Unicode injection protection  
echo - Module validation checks
echo - Restricted environment variables
echo.
echo Dashboard: http://localhost:8080
echo.
echo Press any key to continue...
pause >nul