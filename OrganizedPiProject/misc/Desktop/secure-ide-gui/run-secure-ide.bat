@echo off
echo Compiling Secure IDE GUI...
javac SecureIDEGUI.java
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b 1
)

echo Starting Secure IDE...
java SecureIDEGUI
pause