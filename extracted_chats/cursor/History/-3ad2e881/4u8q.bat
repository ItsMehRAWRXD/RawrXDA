@echo off
REM Quick Reset - Just remove and reinstall essentials
echo 🚀 Quick Reset Starting...
echo.

REM Stop processes
echo 🛑 Stopping processes...
taskkill /F /IM "node.exe" 2>nul
taskkill /F /IM "npm.exe" 2>nul
taskkill /F /IM "go.exe" 2>nul
taskkill /F /IM "python.exe" 2>nul
taskkill /F /IM "code.exe" 2>nul
taskkill /F /IM "Cursor.exe" 2>nul

REM Quick clean
echo 🧹 Quick cleanup...
if exist "%APPDATA%\npm" rmdir /s /q "%APPDATA%\npm" 2>nul
if exist "%USERPROFILE%\go" rmdir /s /q "%USERPROFILE%\go" 2>nul
if exist "%USERPROFILE%\AppData\Local\Programs\Python" rmdir /s /q "%USERPROFILE%\AppData\Local\Programs\Python" 2>nul

REM Reinstall Node.js
echo 📦 Reinstalling Node.js...
winget install OpenJS.NodeJS

REM Reinstall Go
echo 📦 Reinstalling Go...
winget install GoLang.Go

REM Reinstall Python
echo 📦 Reinstalling Python...
winget install Python.Python.3.12

REM Install global packages
echo 📦 Installing packages...
call refreshenv
npm install -g npm@latest typescript @vscode/vsce
pip install requests beautifulsoup4

echo ✅ Quick reset complete!
pause
