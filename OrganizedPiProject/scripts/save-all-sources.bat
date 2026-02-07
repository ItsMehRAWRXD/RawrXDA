@echo off
echo π RawrZ Source Backup - Complete Archive
echo =====================================

set BACKUP_DIR=RawrZ_Backup_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set BACKUP_DIR=%BACKUP_DIR: =0%

mkdir "%BACKUP_DIR%"

echo Backing up Java sources...
xcopy "*.java" "%BACKUP_DIR%\java\" /s /e /i /y

echo Backing up JavaScript/Node sources...
xcopy "*.js" "%BACKUP_DIR%\javascript\" /s /e /i /y
xcopy "*.json" "%BACKUP_DIR%\config\" /s /e /i /y

echo Backing up IDE sources...
xcopy "IDE IDEAS\*" "%BACKUP_DIR%\ide\" /s /e /i /y

echo Backing up RawrZApp...
xcopy "RawrZApp\*" "%BACKUP_DIR%\rawrzapp\" /s /e /i /y

echo Backing up scripts...
xcopy "*.bat" "%BACKUP_DIR%\scripts\" /s /e /i /y
xcopy "*.ps1" "%BACKUP_DIR%\scripts\" /s /e /i /y

echo Creating archive...
powershell "Compress-Archive -Path '%BACKUP_DIR%' -DestinationPath '%BACKUP_DIR%.zip'"

echo ✅ Backup complete: %BACKUP_DIR%.zip
echo All RawrZ sources preserved with timestamp
pause