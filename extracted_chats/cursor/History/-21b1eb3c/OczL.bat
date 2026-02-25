@echo off
echo Building CloudCompiler.exe...
echo ================================

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python is not installed or not in PATH
    pause
    exit /b 1
)

REM Install requirements
echo Installing requirements...
pip install -r requirements.txt

REM Build executable with PyInstaller
echo Building executable...
pyinstaller --onefile --name cloudcompiler --console --add-data "requirements.txt;." cloudcompiler.py

REM Check if build was successful
if exist "dist\cloudcompiler.exe" (
    echo.
    echo ✓ Build successful!
    echo Executable created: dist\cloudcompiler.exe
    echo.
    echo Testing executable...
    dist\cloudcompiler.exe --help
) else (
    echo.
    echo ✗ Build failed!
    echo Check the output above for errors.
)

echo.
pause
