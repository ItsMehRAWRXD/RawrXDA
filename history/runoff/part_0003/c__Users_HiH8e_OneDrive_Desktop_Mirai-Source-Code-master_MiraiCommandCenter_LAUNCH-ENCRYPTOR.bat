@echo off
echo ========================================
echo  Rawr Encryptor - AES-256 Encryption
echo ========================================
echo.
echo Launching GUI...
powershell.exe -ExecutionPolicy Bypass -File "%~dp0Encryptors\rawr-encryptor.ps1" -GUI
