@echo off
echo Starting RawrXD-QtShell with error capture...
echo Current directory: %CD%
echo Executable exists: 
if exist "RawrXD-QtShell.exe" (echo YES) else (echo NO)
echo.
echo DLL check:
dir /b *.dll | findstr /i "qt6core"
echo.
echo Starting application...
set QT_LOGGING_RULES=*=true
set QT_DEBUG_CONSOLE=1
RawrXD-QtShell.exe 2>&1
echo Exit code: %ERRORLEVEL%
pause