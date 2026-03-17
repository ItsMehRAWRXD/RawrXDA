@echo off
echo ========================================
echo  VERIFYING COMPLETE SYSTEM
echo ========================================
echo.

set ERRORS=0

echo Checking files...
echo.

echo [1/13] MASTER-CONTROL.bat
if exist "MASTER-CONTROL.bat" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [2/13] Bot Builder GUI
if exist "MiraiCommandCenter\BotBuilder\MainWindow.xaml" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [3/13] Rawr Encryptor
if exist "MiraiCommandCenter\Encryptors\rawr-encryptor.ps1" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [4/13] Rawr Decryptor
if exist "MiraiCommandCenter\Encryptors\rawr-decryptor.ps1" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [5/13] Scanner Implementation
if exist "mirai\bot\scanner_windows.c" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [6/13] C^&C Server
if exist "MiraiCommandCenter\Server\Program.cs" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [7/13] Control Panel
if exist "MiraiCommandCenter\ControlPanel\index.html" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [8/13] Cure System
if exist "mirai\bot\cure_windows.c" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [9/13] Attack Engine
if exist "mirai\bot\attack_windows.c" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [10/13] Process Killer
if exist "mirai\bot\killer_windows.c" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [11/13] Complete Features Guide
if exist "COMPLETE-FEATURES-GUIDE.md" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [12/13] Testing Guide
if exist "TESTING-GUIDE.md" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo [13/13] Launcher Scripts
if exist "MiraiCommandCenter\LAUNCH-BOT-BUILDER.bat" (
    echo    ✅ Found
) else (
    echo    ❌ Missing
    set /a ERRORS+=1
)

echo.
echo ========================================
if %ERRORS% EQU 0 (
    echo  ✅ ALL FILES PRESENT - SYSTEM READY!
    echo ========================================
    echo.
    echo  You can now run: MASTER-CONTROL.bat
    echo.
) else (
    echo  ❌ ERRORS DETECTED: %ERRORS% files missing
    echo ========================================
    echo.
    echo  Please check the installation
    echo.
)

pause
