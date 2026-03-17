@echo off
cd /d "%~dp0"
echo Building RawrXD CLI Compiler...

if not exist build mkdir build
cd build

echo Check if CMake cache exists...
if not exist CMakeCache.txt (
    echo Configuring CMake...
    cmake .. -G "Visual Studio 17 2022" -A x64 -DRAWRXD_NO_QT=ON
)

echo Building rawrxd-cli...
cmake --build . --target rawrxd-cli --config Release

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo Build successful!
echo Copying binaries...
copy /Y src\cli\Release\rawrxd-cli.exe ..\rawrxd-cli.exe
copy /Y ..\src\RawrXD_Interconnect.dll ..\RawrXD_Interconnect.dll

echo Done.
cd ..
