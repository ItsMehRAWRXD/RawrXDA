@echo off
echo ════════════════════════════════════════════════════════════════
echo  BigDaddyG Universal Complete - Build Script
echo  Hardware-Aware + BFG9999 VRAM + Universal Auto-Scaling
echo ════════════════════════════════════════════════════════════════
echo.

echo [1/3] Assembling BigDaddyG_Universal_Complete.asm...
ml64 /c /Zi /FoBigDaddyG_Universal_Complete.obj BigDaddyG_Universal_Complete.asm
if errorlevel 1 (
    echo ❌ Assembly failed!
    pause
    exit /b 1
)
echo    ✓ Assembly complete

echo.
echo [2/3] Linking...
link /DEBUG /SUBSYSTEM:CONSOLE /ENTRY:main ^
     BigDaddyG_Universal_Complete.obj ^
     kernel32.lib msvcrt.lib user32.lib ^
     /OUT:BigDaddyG_Universal_Complete.exe
if errorlevel 1 (
    echo ❌ Linking failed!
    pause
    exit /b 1
)
echo    ✓ Linking complete

echo.
echo [3/3] Running universal deployment...
echo.
BigDaddyG_Universal_Complete.exe

echo.
echo ════════════════════════════════════════════════════════════════
echo  Build and deployment complete!
echo ════════════════════════════════════════════════════════════════
echo.
echo Next steps:
echo   1. Review generated files in D:\Everything\BigDaddyG-40GB-Torrent\
echo   2. Check hardware detection and auto-configuration results
echo   3. Verify all 7 configuration files were created
echo.
pause
