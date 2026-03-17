@echo off
REM Test launch script for NASM IDE Swarm Agent System
REM This version bypasses dependency installation for testing

echo ========================================
echo NASM IDE Swarm Agent System - TEST MODE
echo ========================================
echo.

REM Check Python installation with detailed error handling
echo [1/3] Testing Python installation...
py --version
if errorlevel 1 (
    echo ERROR: Python launcher 'py' failed
    pause
    exit /b 1
)

echo [2/3] Testing Python core modules...
py -c "import sys, os, json, logging; print('Core modules OK')"
if errorlevel 1 (
    echo ERROR: Python installation is corrupted - core modules missing
    echo Please reinstall Python from https://python.org
    pause
    exit /b 1
)

echo [3/3] Testing basic swarm controller (no dependencies)...
py -c "import swarm_controller; print('Controller import OK')"
if errorlevel 1 (
    echo WARNING: Swarm controller has dependency issues
    echo This is expected if packages aren't installed yet
)

echo.
echo ========================================
echo Basic Python test completed
echo To install dependencies, run: py -m pip install -r requirements.txt
echo ========================================
echo.
pause