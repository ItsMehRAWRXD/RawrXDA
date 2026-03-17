@echo off
echo ========================================
echo  Quick Build: OhGee AI Assistant Hub
echo ========================================
echo.

cd /d "D:\Security Research aka GitHub Repos\OhGee\ItsMehRAWRXD-OhGee-86e21b2"

if not exist KimiAppNative.csproj (
    echo ERROR: KimiAppNative.csproj not found!
    exit /b 1
)

echo Restoring NuGet packages...
dotnet restore

echo Building Release configuration...
dotnet build --configuration Release

if %errorlevel% equ 0 (
    echo.
    echo SUCCESS! Executable built.
    echo.
    echo Output: bin\Release\net8.0-windows\KimiAppNative.exe
    dir bin\Release\net8.0-windows\KimiAppNative.exe
) else (
    echo.
    echo FAILED! Check errors above.
    exit /b 1
)
