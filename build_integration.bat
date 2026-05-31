@echo off
REM Build Integration Script for Sovereign IDE + RawrXD Extensions
REM =============================================================

echo ============================================
echo Sovereign IDE Integration Build System
echo ============================================
echo.

set BUILD_DIR=build_integration
set CONFIG=Release

REM Create build directory
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

echo [1/5] Building Sovereign Core (C)...
gcc -O3 -std=c11 -c ..\sovereign_finisher.c -o sovereign_core.o -DBUILDING_CORE
if errorlevel 1 goto :error

echo [2/5] Building C/C++ Bridge...
g++ -O2 -std=c++17 -shared -DBUILDING_BRIDGE ..\src\bridge\SovereignBridge.cpp sovereign_core.o -o sovereign_bridge.dll -Wl,--out-implib,sovereign_bridge.lib
if errorlevel 1 goto :error

echo [3/5] Building LSP Server...
g++ -O2 -std=c++17 ..\src\lsp\SovereignLSP.cpp sovereign_core.o -o sovereign_lsp.exe -ljson-c
if errorlevel 1 goto :warning_lsp

echo [4/5] Building RawrXD Extensions (C#)...
cd ..\tools\inhouse\RawrXD.Extensions
dotnet build -c %CONFIG%
if errorlevel 1 goto :error
cd ..\..\..\%BUILD_DIR%

echo [5/5] Running Integration Tests...
cd ..\tools\inhouse\RawrXD.Extensions\tests\RawrXD.ExtensionTests
dotnet test -c %CONFIG% --no-build
if errorlevel 1 goto :warning_tests
cd ..\..\..\..\..\%BUILD_DIR%

echo.
echo ============================================
echo Build Complete!
echo ============================================
echo.
echo Outputs:
echo   - sovereign_bridge.dll    (C/C++ Bridge)
echo   - sovereign_lsp.exe         (LSP Server)
echo   - RawrXD.Extensions.dll     (C# Extensions)
echo.
echo Next Steps:
echo   1. Test bridge: ..\build_integration\sovereign_lsp.exe
echo   2. Run IDE: ..\sovereign_finisher.exe
echo   3. Connect VS Code to LSP server
echo.
goto :end

:error
echo.
echo [ERROR] Build failed!
echo.
exit /b 1

:warning_lsp
echo [WARNING] LSP build failed (json-c library may be missing)
echo [INFO] Install json-c: vcpkg install json-c
goto :continue

:warning_tests
echo [WARNING] Some tests failed - check output above
goto :continue

:continue
echo.
echo [INFO] Build partially completed with warnings
echo.

:end
cd ..
pause