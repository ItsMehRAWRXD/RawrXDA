@echo off
echo ========================================
echo  Quick Build: RawrZDesktop (C#/.NET 9.0)
echo ========================================
echo.

cd /d "D:\Security Research aka GitHub Repos\RawrZDesktop\ItsMehRAWRXD-RawrZDesktop-ca34aa4"

if not exist RawrZDesktop.csproj (
    echo ERROR: RawrZDesktop.csproj not found!
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
    echo Output: bin\Release\net9.0\RawrZDesktop.exe
    dir bin\Release\net9.0\RawrZDesktop.exe
) else (
    echo.
    echo FAILED! Check errors above.
    exit /b 1
)
