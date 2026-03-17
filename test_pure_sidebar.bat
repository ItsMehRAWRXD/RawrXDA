@echo off
echo Testing Pure MASM64 Sidebar Integration...

cd /d D:\rawrxd

echo Starting RawrXD with pure sidebar...
start bin\Release\RawrXD-Win32IDE.exe

echo Monitoring log output...
timeout /t 3 > nul

if exist "logs\sidebar_debug.log" (
    echo. 
    echo Latest sidebar log entries:
    echo ================================
    tail -n 10 logs\sidebar_debug.log 2>nul || (
        powershell "Get-Content logs\sidebar_debug.log -Tail 10"
    )
    echo ================================
) else (
    echo WARNING: No sidebar log found at logs\sidebar_debug.log
)

echo.
echo Test complete. Check IDE for pure MASM sidebar functionality.
pause
