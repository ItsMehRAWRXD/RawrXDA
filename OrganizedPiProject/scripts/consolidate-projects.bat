@echo off
echo Project Consolidation Script
echo ===========================

REM Create final organized structure
mkdir "FINAL-PROJECTS" 2>nul
mkdir "ARCHIVED-DUPLICATES" 2>nul

echo.
echo Consolidating RawrZ Projects...
echo - Keeping: Desktop\02-rawrz-app (most recent)
if exist "Desktop\02-rawrz-app" (
    move "Desktop\02-rawrz-app" "FINAL-PROJECTS\RawrZ-Main" >nul 2>&1
    echo   Moved to FINAL-PROJECTS\RawrZ-Main
)

echo - Archiving duplicates:
if exist "Desktop\01-rawrz-clean" move "Desktop\01-rawrz-clean" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "Desktop\rawrz-http-encryptor" move "Desktop\rawrz-http-encryptor" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "Desktop\RawrZ-Clean" move "Desktop\RawrZ-Clean" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "Desktop\RawrZApp" move "Desktop\RawrZApp" "ARCHIVED-DUPLICATES\" >nul 2>&1

echo.
echo Consolidating IDE Projects...
echo - Keeping: UnifiedIDE (most complete)
if exist "UnifiedIDE" (
    move "UnifiedIDE" "FINAL-PROJECTS\" >nul 2>&1
    echo   Kept UnifiedIDE
)

echo - Keeping: IDE IDEAS (Pi-Engine)
if exist "IDE IDEAS" (
    move "IDE IDEAS" "FINAL-PROJECTS\" >nul 2>&1
    echo   Kept IDE IDEAS
)

echo - Archiving IDE duplicates:
if exist "ai-first-editor" move "ai-first-editor" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "custom-editor" move "custom-editor" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "UnifiedAIEditor" move "UnifiedAIEditor" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "Desktop\secure-ide" move "Desktop\secure-ide" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "Desktop\secure-ide-asm" move "Desktop\secure-ide-asm" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "Desktop\secure-ide-gui" move "Desktop\secure-ide-gui" "ARCHIVED-DUPLICATES\" >nul 2>&1
if exist "Desktop\secure-ide-java" move "Desktop\secure-ide-java" "ARCHIVED-DUPLICATES\" >nul 2>&1

echo.
echo Moving Active Projects...
if exist "Eng Src" (
    move "Eng Src" "FINAL-PROJECTS\" >nul 2>&1
    echo   Moved Eng Src (SaaSEncryptionSecurity + Star5IDE)
)

echo.
echo Archiving Backup Files...
move "*Backup*.zip" "ARCHIVED-DUPLICATES\" >nul 2>&1
move "RawrZ*.zip" "ARCHIVED-DUPLICATES\" >nul 2>&1

echo.
echo ========================================
echo Project Consolidation Complete!
echo ========================================
echo.
echo FINAL CLEAN STRUCTURE:
echo.
echo FINAL-PROJECTS/
echo ├── Eng Src/
echo │   ├── SaaSEncryptionSecurity/  ✅ (Enhanced with Pi-Engine)
echo │   └── Star5IDE/               ✅ (Security tools + IDE)
echo ├── RawrZ-Main/                 ✅ (Latest RawrZ version)
echo ├── UnifiedIDE/                 ✅ (Complete IDE system)
echo └── IDE IDEAS/                  ✅ (Pi-Engine + experiments)
echo.
echo ARCHIVED-DUPLICATES/
echo └── All old versions and backups
echo.
echo RESULT: 4 clean projects instead of 15+ duplicates!
echo.
pause