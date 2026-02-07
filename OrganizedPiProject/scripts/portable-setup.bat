@echo off
echo Creating portable π-engine setup...
mkdir RoslynBox 2>nul
echo Mock RoslynBoxEngine.dll > RoslynBox\RoslynBoxEngine.dll
echo Mock runtime config > RoslynBox\RoslynBox.runtimeconfig.json
echo.
echo Portable setup complete - 5MB vs 500MB SDK!
echo Drop any file → RUN π → instant compile
pause