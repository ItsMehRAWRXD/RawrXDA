@echo off
REM BotBuilder C# WPF Compilation Script
REM Uses VS2022 MSBuild for .NET Framework 4.8

echo 🏗️  BOTBUILDER COMPILATION SCRIPT
echo ==================================
echo.
echo Attempting to locate and use VS2022 MSBuild...

REM Try different VS2022 paths
set "VS2022_MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if exist "%VS2022_MSBUILD%" goto :BuildWithVS2022

set "VS2022_MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
if exist "%VS2022_MSBUILD%" goto :BuildWithVS2022

set "VS2022_MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if exist "%VS2022_MSBUILD%" goto :BuildWithVS2022

set "VS2022_MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if exist "%VS2022_MSBUILD%" goto :BuildWithVS2019

set "VS2022_MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe"
if exist "%VS2022_MSBUILD%" goto :BuildWithVS2019

set "VS2022_MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
if exist "%VS2022_MSBUILD%" goto :BuildWithVS2019

REM Try Windows SDK MSBuild
set "SDK_MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe"
if exist "%SDK_MSBUILD%" goto :BuildWithSDK

echo ❌ MSBuild not found in standard locations
echo.
echo Attempting alternative compilation methods...
goto :TryCSC

:BuildWithVS2022
echo ✅ Found VS2022 MSBuild: %VS2022_MSBUILD%
echo.
echo Building BotBuilder.csproj...
"%VS2022_MSBUILD%" "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder\BotBuilder.csproj" /p:Configuration=Release /p:Platform="Any CPU" /verbosity:normal
if %ERRORLEVEL% == 0 (
    echo ✅ BotBuilder compilation successful!
) else (
    echo ❌ BotBuilder compilation failed with error %ERRORLEVEL%
    goto :TryCSC
)
goto :End

:BuildWithVS2019
echo ✅ Found VS2019 MSBuild: %VS2022_MSBUILD%
echo.
echo Building BotBuilder.csproj...
"%VS2022_MSBUILD%" "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder\BotBuilder.csproj" /p:Configuration=Release /p:Platform="Any CPU" /verbosity:normal
if %ERRORLEVEL% == 0 (
    echo ✅ BotBuilder compilation successful!
) else (
    echo ❌ BotBuilder compilation failed with error %ERRORLEVEL%
    goto :TryCSC
)
goto :End

:BuildWithSDK
echo ✅ Found SDK MSBuild: %SDK_MSBUILD%
echo.
echo Building BotBuilder.csproj...
"%SDK_MSBUILD%" "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder\BotBuilder.csproj" /p:Configuration=Release /p:Platform="Any CPU" /verbosity:normal
if %ERRORLEVEL% == 0 (
    echo ✅ BotBuilder compilation successful!
) else (
    echo ❌ BotBuilder compilation failed with error %ERRORLEVEL%
    goto :TryCSC
)
goto :End

:TryCSC
echo.
echo 🔧 Trying direct C# compiler (csc.exe)...
echo.

REM Find .NET Framework CSC
set "NET48_CSC=C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\Roslyn\csc.exe"
if exist "%NET48_CSC%" goto :CompileWithCSC

set "NET48_CSC=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\Roslyn\csc.exe"
if exist "%NET48_CSC%" goto :CompileWithCSC

set "NET48_CSC=C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if exist "%NET48_CSC%" goto :CompileWithCSC

set "NET48_CSC=C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if exist "%NET48_CSC%" goto :CompileWithCSC

echo ❌ No C# compiler found
echo.
echo 📋 Available compilation status:
echo    • MSBuild: Not found
echo    • CSC: Not found
echo    • Recommendation: Install Visual Studio 2022 or Build Tools
echo.
goto :End

:CompileWithCSC
echo ✅ Found C# Compiler: %NET48_CSC%
echo.
echo 📋 Manual compilation attempt (may have missing references)...
echo.

REM Try basic compilation (will likely fail due to WPF references)
cd /d "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder"
"%NET48_CSC%" /target:winexe /out:BotBuilder.exe /reference:"C:\Program Files (x86)\Reference Assemblies\Microsoft\Framework\.NETFramework\v4.8\*.dll" *.cs
if %ERRORLEVEL% == 0 (
    echo ✅ Basic C# compilation successful!
) else (
    echo ❌ C# compilation failed - WPF references needed
    echo.
    echo 💡 Solution: Use proper MSBuild or Visual Studio environment
)

:End
echo.
echo 📊 COMPILATION SUMMARY:
echo =======================
echo Project: BotBuilder (WPF .NET Framework 4.8)
echo Status: Build attempt completed
echo.
echo 💡 Next steps if build failed:
echo    1. Install Visual Studio 2022 Community (free)
echo    2. Install .NET Framework 4.8 Developer Pack
echo    3. Use Developer Command Prompt for VS 2022
echo.
pause