@echo off
echo ========================================
echo  Mirai Bot Builder
echo ========================================
echo.
echo Building Bot Builder...
cd BotBuilder

dotnet restore
if %errorlevel% neq 0 (
    echo ERROR: Failed to restore packages
    pause
    exit /b 1
)

dotnet build -c Release
if %errorlevel% neq 0 (
    echo ERROR: Failed to build Bot Builder
    pause
    exit /b 1
)

echo.
echo Launching Bot Builder...
dotnet run -c Release

pause
