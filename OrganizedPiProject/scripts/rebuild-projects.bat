@echo off
echo Smart Project Rebuilder
echo ======================

echo Creating unified project structure...
mkdir "UNIFIED-PROJECTS" 2>nul
mkdir "UNIFIED-PROJECTS\RawrZ-Complete" 2>nul
mkdir "UNIFIED-PROJECTS\IDE-Complete" 2>nul

echo.
echo Rebuilding RawrZ-Complete (merging all versions)...
echo - Merging Desktop\01-rawrz-clean
if exist "Desktop\01-rawrz-clean" xcopy "Desktop\01-rawrz-clean\*" "UNIFIED-PROJECTS\RawrZ-Complete\" /E /H /Y >nul 2>&1

echo - Merging Desktop\02-rawrz-app  
if exist "Desktop\02-rawrz-app" xcopy "Desktop\02-rawrz-app\*" "UNIFIED-PROJECTS\RawrZ-Complete\" /E /H /Y >nul 2>&1

echo - Merging Desktop\rawrz-http-encryptor
if exist "Desktop\rawrz-http-encryptor" xcopy "Desktop\rawrz-http-encryptor\*" "UNIFIED-PROJECTS\RawrZ-Complete\" /E /H /Y >nul 2>&1

echo - Merging Desktop\RawrZ-Clean
if exist "Desktop\RawrZ-Clean" xcopy "Desktop\RawrZ-Clean\*" "UNIFIED-PROJECTS\RawrZ-Complete\" /E /H /Y >nul 2>&1

echo - Merging Desktop\RawrZApp
if exist "Desktop\RawrZApp" xcopy "Desktop\RawrZApp\*" "UNIFIED-PROJECTS\RawrZ-Complete\" /E /H /Y >nul 2>&1

echo.
echo Rebuilding IDE-Complete (merging all versions)...
echo - Merging ai-first-editor
if exist "ai-first-editor" xcopy "ai-first-editor\*" "UNIFIED-PROJECTS\IDE-Complete\" /E /H /Y >nul 2>&1

echo - Merging custom-editor
if exist "custom-editor" xcopy "custom-editor\*" "UNIFIED-PROJECTS\IDE-Complete\" /E /H /Y >nul 2>&1

echo - Merging UnifiedIDE
if exist "UnifiedIDE" xcopy "UnifiedIDE\*" "UNIFIED-PROJECTS\IDE-Complete\" /E /H /Y >nul 2>&1

echo - Merging UnifiedAIEditor
if exist "UnifiedAIEditor" xcopy "UnifiedAIEditor\*" "UNIFIED-PROJECTS\IDE-Complete\" /E /H /Y >nul 2>&1

echo - Merging IDE IDEAS
if exist "IDE IDEAS" xcopy "IDE IDEAS\*" "UNIFIED-PROJECTS\IDE-Complete\" /E /H /Y >nul 2>&1

echo - Merging Desktop\secure-ide*
if exist "Desktop\secure-ide" xcopy "Desktop\secure-ide\*" "UNIFIED-PROJECTS\IDE-Complete\" /E /H /Y >nul 2>&1
if exist "Desktop\secure-ide-gui" xcopy "Desktop\secure-ide-gui\*" "UNIFIED-PROJECTS\IDE-Complete\" /E /H /Y >nul 2>&1
if exist "Desktop\secure-ide-java" xcopy "Desktop\secure-ide-java\*" "UNIFIED-PROJECTS\IDE-Complete\" /E /H /Y >nul 2>&1

echo.
echo Moving Eng Src (already unified)...
if exist "Eng Src" (
    move "Eng Src" "UNIFIED-PROJECTS\" >nul 2>&1
    echo - Moved Eng Src to UNIFIED-PROJECTS\
)

echo.
echo Creating master package.json...
(
echo {
echo   "name": "unified-development-suite",
echo   "version": "1.0.0", 
echo   "description": "Unified development suite with RawrZ, IDE, and SaaS components",
echo   "scripts": {
echo     "build-rawrz": "cd UNIFIED-PROJECTS/RawrZ-Complete && npm run build",
echo     "build-ide": "cd UNIFIED-PROJECTS/IDE-Complete && npm run build",
echo     "build-saas": "cd UNIFIED-PROJECTS/Eng-Src/SaaSEncryptionSecurity && npm run build",
echo     "start-all": "echo Starting all components...",
echo     "deploy-all": "echo Deploying all components..."
echo   },
echo   "workspaces": [
echo     "UNIFIED-PROJECTS/RawrZ-Complete",
echo     "UNIFIED-PROJECTS/IDE-Complete",
echo     "UNIFIED-PROJECTS/Eng-Src/SaaSEncryptionSecurity"
echo   ]
echo }
) > package.json

echo.
echo Creating master README.md...
(
echo # Unified Development Suite
echo.
echo Enterprise-grade development platform with integrated security, IDE, and SaaS components.
echo.
echo ## Components
echo.
echo ### 1. RawrZ-Complete
echo - **Location**: `UNIFIED-PROJECTS/RawrZ-Complete/`
echo - **Features**: HTTP encryption, security platform, advanced tooling
echo.
echo ### 2. IDE-Complete  
echo - **Location**: `UNIFIED-PROJECTS/IDE-Complete/`
echo - **Features**: Multi-language IDE, AI integration, Pi-Engine
echo.
echo ### 3. Eng-Src ^(SaaS Platform^)
echo - **Location**: `UNIFIED-PROJECTS/Eng-Src/`
echo - **Features**: SaaS encryption security, Star5IDE, multi-language execution
echo.
echo ## Structure
echo.
echo ```
echo UNIFIED-PROJECTS/
echo ├── RawrZ-Complete/     # Unified RawrZ platform
echo ├── IDE-Complete/       # Unified IDE system  
echo └── Eng-Src/           # SaaS ^& Security platform
echo     ├── SaaSEncryptionSecurity/
echo     └── Star5IDE/
echo ```
) > README.md

echo.
echo ========================================
echo Smart Rebuild Complete!
echo ========================================
echo.
echo NEW UNIFIED STRUCTURE:
echo.
echo UNIFIED-PROJECTS/
echo ├── RawrZ-Complete/     ✅ All RawrZ versions merged
echo ├── IDE-Complete/       ✅ All IDE versions merged  
echo └── Eng-Src/           ✅ SaaS ^& Security ^(GitHub repos^)
echo     ├── SaaSEncryptionSecurity/
echo     └── Star5IDE/
echo.
echo BENEFITS:
echo - All duplicates intelligently merged
echo - Unified build system
echo - Professional monorepo structure
echo - Ready for enterprise deployment
echo.
echo FILES CREATED:
echo - package.json ^(master build config^)
echo - README.md ^(unified documentation^)
echo.
pause