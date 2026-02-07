@echo off
echo Quick Project Organization
echo =========================

mkdir archived-projects 2>nul

echo Moving older RawrZ versions...
move "Desktop\01-rawrz-clean" "archived-projects\" 2>nul
move "Desktop\RawrZ-Clean" "archived-projects\" 2>nul
move "Desktop\RawrZApp" "archived-projects\" 2>nul

echo Moving older IDE versions...
move "Desktop\secure-ide-asm" "archived-projects\" 2>nul
move "Desktop\secure-ide-gui" "archived-projects\" 2>nul
move "Desktop\secure-ide-java" "archived-projects\" 2>nul
move "UnifiedAIEditor" "archived-projects\" 2>nul

echo Moving backup files...
move "Desktop\RawrZ_Backup_2025-09-19_12-20-34.zip" "archived-projects\" 2>nul
move "Desktop\RawrZApp-Backup-2025-09-20-0337.zip" "archived-projects\" 2>nul
move "Desktop\RawrZ-Security-Platform-v2.0.0-FINAL.zip" "archived-projects\" 2>nul

echo.
echo Done! Kept the most recent versions:
echo - Desktop\02-rawrz-app (newest RawrZ)
echo - Desktop\secure-ide (main IDE)
echo - custom-editor (active editor)
echo - UnifiedIDE (unified version)

pause