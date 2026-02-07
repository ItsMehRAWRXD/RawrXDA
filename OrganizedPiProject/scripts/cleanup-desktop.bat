@echo off
echo Desktop Project Cleanup Script
echo ==============================

REM Create organized structure
mkdir "01-active-projects" 2>nul
mkdir "02-archived-projects" 2>nul  
mkdir "03-tools-utilities" 2>nul
mkdir "04-documentation" 2>nul

REM Move active projects
echo Moving active projects...
if exist "Eng Src\SaaSEncryptionSecurity" move "Eng Src\SaaSEncryptionSecurity" "01-active-projects\"
if exist "Eng Src\Star5IDE" move "Eng Src\Star5IDE" "01-active-projects\"
if exist "UnifiedIDE" move "UnifiedIDE" "01-active-projects\"

REM Archive old versions
echo Archiving old versions...
if exist "Desktop\rawrz-http-encryptor" move "Desktop\rawrz-http-encryptor" "02-archived-projects\"
if exist "Desktop\RawrZApp" move "Desktop\RawrZApp" "02-archived-projects\"
if exist "Desktop\RawrZ-Clean" move "Desktop\RawrZ-Clean" "02-archived-projects\"

REM Move backup files
echo Moving backup files...
move "Desktop\*Backup*.zip" "02-archived-projects\" 2>nul
move "Desktop\RawrZ*.zip" "02-archived-projects\" 2>nul

REM Move tools
echo Moving tools and utilities...
move "Eng Src\*.js" "03-tools-utilities\" 2>nul
move "Desktop\build_*.bat" "03-tools-utilities\" 2>nul
move "Desktop\deploy-*.ps1" "03-tools-utilities\" 2>nul

REM Move documentation
echo Moving documentation...
if exist "AI Chat ETC" move "AI Chat ETC" "04-documentation\"
move "Desktop\*.md" "04-documentation\" 2>nul

echo.
echo Desktop organization complete!
echo.
echo Structure:
echo   01-active-projects/    - Current working projects
echo   02-archived-projects/  - Old versions and backups  
echo   03-tools-utilities/    - Scripts and tools
echo   04-documentation/      - Docs and chat logs
echo.
pause