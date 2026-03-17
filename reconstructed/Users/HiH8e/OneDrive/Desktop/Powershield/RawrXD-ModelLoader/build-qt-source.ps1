$QtSrcDir = "C:\Users\HiH8e\Downloads\qtbase-everywhere-src-6.7.2\qtbase-everywhere-src-6.7.2"
$QtBuildDir = "C:\Users\HiH8e\Downloads\qtbase-everywhere-src-6.7.2\build-msvc2022_64"
$QtInstallDir = "C:\Qt\6.7.2\msvc2022_64"
$VcvarsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

# Ensure tools are in PATH
$env:PATH = "C:\Strawberry\perl\bin;C:\Program Files\CMake\bin;$env:LOCALAPPDATA\Microsoft\WinGet\Links;$env:PATH"

Write-Host "Checking tools..."
cmake --version
ninja --version
perl --version

# Create build directory
if (!(Test-Path $QtBuildDir)) {
    New-Item -ItemType Directory -Path $QtBuildDir | Out-Null
}

Set-Location $QtBuildDir

# Create a batch file to run the build in the MSVC environment
$batchContent = @"
@echo off
call "$VcvarsPath"
if %errorlevel% neq 0 exit /b %errorlevel%

echo Configuring Qt...
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$QtInstallDir" "$QtSrcDir"
if %errorlevel% neq 0 exit /b %errorlevel%

echo Building Qt...
cmake --build . --parallel
if %errorlevel% neq 0 exit /b %errorlevel%

echo Installing Qt...
cmake --install .
if %errorlevel% neq 0 exit /b %errorlevel%

echo Qt build and install complete.
"@

$batchContent | Out-File "build_qt.bat" -Encoding ASCII

Write-Host "Starting Qt Build (this may take a while)..."
cmd /c "build_qt.bat"
