@echo off
setlocal enabledelayedexpansion

set "SOURCE=E:\Everything\cursor\extensions\bigdaddyg-copilot-1.0.0"
set "TARGET=C:\Users\HiH8e\.cursor\extensions\bigdaddyg.bigdaddyg-copilot-1.0.0"

echo Cleaning target directory...
if exist "%TARGET%" rmdir /s /q "%TARGET%" 2>nul
timeout /t 1 /nobreak >nul

echo Creating target directory...
mkdir "%TARGET%" 2>nul

echo Copying files...
copy "%SOURCE%\package.json" "%TARGET%\package.json" /Y >nul
if exist "%SOURCE%\out" xcopy "%SOURCE%\out" "%TARGET%\out" /E /Y /Q >nul
if exist "%SOURCE%\media" xcopy "%SOURCE%\media" "%TARGET%\media" /E /Y /Q >nul

echo.
echo ✅ Installation complete!
echo Package.json copied: %TARGET%\package.json
echo.
echo Verifying...
if exist "%TARGET%\package.json" (
    echo ✅ package.json is present
) else (
    echo ❌ package.json is MISSING
)
if exist "%TARGET%\out\extension.js" (
    echo ✅ extension.js is present
) else (
    echo ❌ extension.js is MISSING
)

timeout /t 3 /nobreak >nul
