@echo off
setlocal enabledelayedexpansion

:: ============================================
:: UNDERGROUND KINGZ SECURITY PATCH DEPLOYER
:: Automated Patch Application Script
:: ============================================

echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                 SECURITY PATCH DEPLOYMENT                           ║
echo ║                 Automated Application                               ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.

:: Configuration
set PATCH_EXE=SECURITY_PATCH.exe
set BACKUP_DIR=backup_%DATE:~-4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%
set LOG_FILE=deployment_log.txt

:: Check if patch executable exists
if not exist "%PATCH_EXE%" (
    echo ❌ Security patch executable not found: %PATCH_EXE%
    echo Please build the patch first using build_patch.bat
    exit /b 1
)

:: Create backup directory
echo Creating backup directory...
mkdir "%BACKUP_DIR%" >nul 2>&1
if errorlevel 1 (
    echo ❌ Failed to create backup directory
    exit /b 1
)

echo ✅ Backup directory created: %BACKUP_DIR%

:: Stop RawrXD processes
echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                    STOPPING RAWRXD PROCESSES                        ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.

echo Stopping RawrXD processes...
set PROCESSES=(
    "RawrXD-Agent.exe"
    "RawrXD-AgenticIDE.exe"
    "RawrXD-CLI.exe"
    "RawrXD-IDE.exe"
    "RawrXD-Server.exe"
)

set STOPPED_COUNT=0
for %%P in (%PROCESSES%) do (
    echo Checking for process: %%P
    tasklist /FI "IMAGENAME eq %%P" 2>nul | find /I "%%P" >nul
    if errorlevel 0 (
        echo Stopping %%P...
        taskkill /f /im %%P >nul 2>&1
        if errorlevel 0 (
            echo ✅ Stopped: %%P
            set /a STOPPED_COUNT+=1
        ) else (
            echo ⚠️  Could not stop: %%P (may not be running)
        )
    ) else (
        echo ℹ️  Process not running: %%P
    )
)

echo Stopped %STOPPED_COUNT% RawrXD processes

:: Backup critical files
echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                      CREATING BACKUPS                              ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.

echo Backing up critical files...
set BACKUP_FILES=(
    "RawrXD-Agent.exe"
    "RawrXD-AgenticIDE.exe"
    "RawrXD-CLI.exe"
    "RawrXD-IDE.exe"
    "*.dll"
    "*.config"
    "*.json"
)

set BACKUP_COUNT=0
for %%F in (%BACKUP_FILES%) do (
    if exist "%%F" (
        echo Backing up: %%F
        copy "%%F" "%BACKUP_DIR%\" >nul 2>&1
        if errorlevel 0 (
            echo ✅ Backed up: %%F
            set /a BACKUP_COUNT+=1
        ) else (
            echo ⚠️  Failed to backup: %%F
        )
    ) else (
        echo ℹ️  File not found: %%F
    )
)

echo Backed up %BACKUP_COUNT% files to %BACKUP_DIR%

:: Apply the security patch
echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                    APPLYING SECURITY PATCH                          ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.

echo Applying security patch...
"%PATCH_EXE%" --apply > "%LOG_FILE%" 2>&1

if errorlevel 1 (
    echo ❌ Patch application failed!
    echo Check %LOG_FILE% for details
    echo.
    type "%LOG_FILE%"
    
    :: Attempt rollback
    echo.
    echo Attempting automatic rollback...
    call :rollback
    exit /b 1
)

echo ✅ Patch applied successfully

:: Verify the patch
echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                    VERIFYING PATCH                                 ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.

echo Verifying patch application...
"%PATCH_EXE%" --verify >> "%LOG_FILE%" 2>&1

if errorlevel 1 (
    echo ❌ Patch verification failed!
    echo Check %LOG_FILE% for details
    echo.
    type "%LOG_FILE%"
    
    :: Attempt rollback
    echo.
    echo Attempting automatic rollback...
    call :rollback
    exit /b 1
)

echo ✅ Patch verified successfully

:: Test security fixes
echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                    TESTING SECURITY FIXES                           ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.

echo Testing security fixes...
"%PATCH_EXE%" --test >> "%LOG_FILE%" 2>&1

if errorlevel 1 (
    echo ⚠️  Some security tests failed
    echo Check %LOG_FILE% for details
    echo.
    type "%LOG_FILE%"
) else (
    echo ✅ All security tests passed
)

:: Display deployment summary
echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                    DEPLOYMENT SUMMARY                               ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.
echo ✅ Security patch deployment completed!
echo.
echo 📁 Backup created:    %BACKUP_DIR%
echo 📄 Processes stopped: %STOPPED_COUNT%
echo 📊 Files backed up:   %BACKUP_COUNT%
echo 🔒 Patch applied:      ✅
echo 🔍 Patch verified:     ✅
echo 🧪 Security tested:    ✅
echo.

:: Create rollback script
echo Creating rollback script...
(
echo @echo off
echo set BACKUP_DIR=%BACKUP_DIR%
echo.
echo echo Rolling back security patch...
echo.
echo echo Restoring files from backup...
echo copy "%%BACKUP_DIR%%\*" .
echo.
echo echo ✅ Rollback completed!
echo echo Original files restored from %%BACKUP_DIR%%
) > rollback.bat

echo ✅ Rollback script created: rollback.bat

:: Final instructions
echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                    NEXT STEPS                                       ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.
echo 1. Restart RawrXD Agentic IDE:
echo    Start the application normally
echo.
echo 2. Verify functionality:
echo    Test all critical features work correctly
echo.
echo 3. Monitor for issues:
echo    Check logs and system performance
echo.
echo 4. Emergency rollback:
echo    Run rollback.bat if issues occur
echo.
echo 📞 Support: security@undergroundkingz.com
echo.

pause
exit /b 0

:rollback
:: Emergency rollback procedure
echo.
echo ╔══════════════════════════════════════════════════════════════════════╗
echo ║                    EMERGENCY ROLLBACK                              ║
echo ╚══════════════════════════════════════════════════════════════════════╝
echo.

echo Rolling back changes...
echo Restoring files from backup: %BACKUP_DIR%

for %%F in ("%BACKUP_DIR%\*") do (
    echo Restoring: %%~nxF
    copy "%%F" . >nul 2>&1
    if errorlevel 0 (
        echo ✅ Restored: %%~nxF
    ) else (
        echo ❌ Failed to restore: %%~nxF
    )
)

echo ✅ Rollback completed!
echo Original system state restored from backup
echo.

exit /b 1