@echo off
echo ════════════════════════════════════════════════════════════════
echo  BigDaddyG 40GB Deployment - Build Script
echo ════════════════════════════════════════════════════════════════
echo.

echo [1/3] Assembling BigDaddyG_Deployment.asm...
ml64 /c /Fo"BigDaddyG_Deployment.obj" "BigDaddyG_Deployment.asm"
if errorlevel 1 (
    echo ❌ Assembly failed!
    pause
    exit /b 1
)
echo    ✓ Assembly complete

echo.
echo [2/3] Linking...
link /RELEASE /SUBSYSTEM:CONSOLE /ENTRY:main "BigDaddyG_Deployment.obj" kernel32.lib msvcrt.lib /OUT:"BigDaddyG_Deployment.exe"
if errorlevel 1 (
    echo ❌ Linking failed!
    pause
    exit /b 1
)
echo    ✓ Linking complete

echo.
echo [3/3] Running deployment...
echo.
BigDaddyG_Deployment.exe

echo.
echo ════════════════════════════════════════════════════════════════
echo  Build and deployment complete!
echo ════════════════════════════════════════════════════════════════
pause
