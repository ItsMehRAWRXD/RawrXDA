@echo off
REM Workspace Organization Script
REM Moves files to their appropriate project folders

cd /d "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master"

echo.
echo ==========================================
echo    WORKSPACE ORGANIZATION IN PROGRESS
echo ==========================================
echo.

REM BigDaddyG Project
echo Moving BigDaddyG files...
if exist "bigdaddyg-launcher-interactive.ps1" move "bigdaddyg-launcher-interactive.ps1" "Projects\BigDaddyG\" >nul
if exist "START-BIGDADDYG.bat" move "START-BIGDADDYG.bat" "Projects\BigDaddyG\" >nul
if exist "BIGDADDYG-EXECUTIVE-SUMMARY.md" move "BIGDADDYG-EXECUTIVE-SUMMARY.md" "Projects\BigDaddyG\" >nul
if exist "BIGDADDYG-LAUNCHER-COMPLETE.md" move "BIGDADDYG-LAUNCHER-COMPLETE.md" "Projects\BigDaddyG\" >nul
if exist "BIGDADDYG-LAUNCHER-GUIDE.md" move "BIGDADDYG-LAUNCHER-GUIDE.md" "Projects\BigDaddyG\" >nul
if exist "BIGDADDYG-LAUNCHER-SETUP.md" move "BIGDADDYG-LAUNCHER-SETUP.md" "Projects\BigDaddyG\" >nul
if exist "BIGDADDYG-QUICK-REFERENCE.txt" move "BIGDADDYG-QUICK-REFERENCE.txt" "Projects\BigDaddyG\" >nul
if exist "D-DRIVE-AUDIT-COMPLETE.md" move "D-DRIVE-AUDIT-COMPLETE.md" "Projects\BigDaddyG\" >nul
if exist "INTEGRATION-DECISION.md" move "INTEGRATION-DECISION.md" "Projects\BigDaddyG\" >nul
if exist "bigdaddyg-beast-mini.py" move "bigdaddyg-beast-mini.py" "Projects\BigDaddyG\" >nul
if exist "BigDaddyG-Beast-Modelfile" move "BigDaddyG-Beast-Modelfile" "Projects\BigDaddyG\" >nul
if exist "BigDaddyG-Beast-Optimized-Modelfile" move "BigDaddyG-Beast-Optimized-Modelfile" "Projects\BigDaddyG\" >nul
echo [OK] BigDaddyG files organized

REM Beast System
echo Moving Beast System files...
if exist "beast-mini-standalone.py" move "beast-mini-standalone.py" "Projects\Beast-System\" >nul
if exist "beast-quick-start.py" move "beast-quick-start.py" "Projects\Beast-System\" >nul
if exist "beast-swarm-demo.html" move "beast-swarm-demo.html" "Projects\Beast-System\" >nul
if exist "beast-swarm-system.py" move "beast-swarm-system.py" "Projects\Beast-System\" >nul
if exist "beast-swarm-web.js" move "beast-swarm-web.js" "Projects\Beast-System\" >nul
if exist "beast-training-suite.py" move "beast-training-suite.py" "Projects\Beast-System\" >nul
if exist "Beast-IDEBrowser.ps1" move "Beast-IDEBrowser.ps1" "Projects\Beast-System\" >nul
if exist "launch-beast-browser.ps1" move "launch-beast-browser.ps1" "Projects\Beast-System\" >nul
if exist "ModelfileBeast" move "ModelfileBeast" "Projects\Beast-System\" >nul
echo [OK] Beast System files organized

REM RawrZ Platform
echo Moving RawrZ Platform files...
if exist "BRxC.html" move "BRxC.html" "Projects\RawrZ\" >nul
if exist "BRxC-Recovery.html" move "BRxC-Recovery.html" "Projects\RawrZ\" >nul
if exist "RawrBrowser.ps1" move "RawrBrowser.ps1" "Projects\RawrZ\" >nul
if exist "RawrCompress-GUI.ps1" move "RawrCompress-GUI.ps1" "Projects\RawrZ\" >nul
if exist "RawrZ-Payload-Builder-GUI.ps1" move "RawrZ-Payload-Builder-GUI.ps1" "Projects\RawrZ\" >nul
if exist "quick-setup-rawrz-http.bat" move "quick-setup-rawrz-http.bat" "Projects\RawrZ\" >nul
if exist "quick-build-rawrzdesktop.bat" move "quick-build-rawrzdesktop.bat" "Projects\RawrZ\" >nul
echo [OK] RawrZ files organized

REM CyberForge AV Engine
echo Moving CyberForge files...
if exist "README-CYBERFORGE.md" move "README-CYBERFORGE.md" "Projects\CyberForge\" >nul
if exist "package-cyberforge.json" move "package-cyberforge.json" "Projects\CyberForge\" >nul
if exist "comprehensive-ide-fix.ps1" move "comprehensive-ide-fix.ps1" "Projects\CyberForge\" >nul
if exist "fix-dom-errors.ps1" move "fix-dom-errors.ps1" "Projects\CyberForge\" >nul
if exist "fix-domready-function.ps1" move "fix-domready-function.ps1" "Projects\CyberForge\" >nul
if exist "fix-js-syntax-errors.ps1" move "fix-js-syntax-errors.ps1" "Projects\CyberForge\" >nul
if exist "ide-fixes-template.html" move "ide-fixes-template.html" "Projects\CyberForge\" >nul
if exist "ide-fixes.js" move "ide-fixes.js" "Projects\CyberForge\" >nul
echo [OK] CyberForge files organized

REM Build Tools
echo Moving Build Tools...
if exist "Master-Build-All-Projects.ps1" move "Master-Build-All-Projects.ps1" "Tools\Build-System\" >nul
if exist "MASTER-CONTROL.bat" move "MASTER-CONTROL.bat" "Tools\Build-System\" >nul
if exist "QUICK-BUILD-ALL.bat" move "QUICK-BUILD-ALL.bat" "Tools\Build-System\" >nul
if exist "quick-build.bat" move "quick-build.bat" "Tools\Build-System\" >nul
if exist "VERIFY-SYSTEM.bat" move "VERIFY-SYSTEM.bat" "Tools\Build-System\" >nul
if exist "Build-Windows.psm1" move "Build-Windows.psm1" "Tools\Build-System\" >nul
if exist "Build-Mirai-Windows.ps1" move "Build-Mirai-Windows.ps1" "Tools\Build-System\" >nul
if exist "build-mirai-windows.bat" move "build-mirai-windows.bat" "Tools\Build-System\" >nul
echo [OK] Build Tools organized

REM Utilities
echo Moving Utilities...
if exist "master-cli.js" move "master-cli.js" "Tools\Utilities\" >nul
if exist "payload-cli.js" move "payload-cli.js" "Tools\Utilities\" >nul
if exist "payload_builder.py" move "payload_builder.py" "Tools\Utilities\" >nul
if exist "start-demo-server.py" move "start-demo-server.py" "Tools\Utilities\" >nul
if exist "cli.js" move "cli.js" "Tools\Utilities\" >nul
if exist "orchestra-server.js" move "orchestra-server.js" "Tools\Utilities\" >nul
if exist "orchestra.mjs" move "orchestra.mjs" "Tools\Utilities\" >nul
if exist "backend-server.js" move "backend-server.js" "Tools\Utilities\" >nul
if exist "backend.mjs" move "backend.mjs" "Tools\Utilities\" >nul
echo [OK] Utilities organized

REM Launchers
echo Moving Launchers...
if exist "Launch-IDE-Servers.bat" move "Launch-IDE-Servers.bat" "Tools\Launchers\" >nul
if exist "launch-ide.bat" move "launch-ide.bat" "Tools\Launchers\" >nul
if exist "Launch-Modern-IDE.ps1" move "Launch-Modern-IDE.ps1" "Tools\Launchers\" >nul
if exist "Start-IDE-Servers.ps1" move "Start-IDE-Servers.ps1" "Tools\Launchers\" >nul
if exist "GUI-Test.ps1" move "GUI-Test.ps1" "Tools\Launchers\" >nul
echo [OK] Launchers organized

REM Configuration
echo Moving Configuration files...
if exist "package.json" move "package.json" "Configuration\" >nul
if exist "package-lock.json" move "package-lock.json" "Configuration\" >nul
if exist ".hintrc" move ".hintrc" "Configuration\" >nul
echo [OK] Configuration organized

REM Experimental and Legacy
echo Moving Experimental Legacy files...
if exist "PowerShell-GUI-Showcase.ps1" move "PowerShell-GUI-Showcase.ps1" "Experimental-Legacy\Legacy-Scripts\" >nul
if exist "PowerShell-HTML-Browser-IDE.ps1" move "PowerShell-HTML-Browser-IDE.ps1" "Experimental-Legacy\Legacy-Scripts\" >nul
if exist "PowerShell-Studio-Pro.clean.ps1" move "PowerShell-Studio-Pro.clean.ps1" "Experimental-Legacy\Legacy-Scripts\" >nul
if exist "PowerShell-Studio-Pro.ps1" move "PowerShell-Studio-Pro.ps1" "Experimental-Legacy\Legacy-Scripts\" >nul
if exist "test-dom-fixes.ps1" move "test-dom-fixes.ps1" "Experimental-Legacy\Test-Files\" >nul
if exist "verify-js-fixes.ps1" move "verify-js-fixes.ps1" "Experimental-Legacy\Test-Files\" >nul
if exist "test-compatibility.bat" move "test-compatibility.bat" "Experimental-Legacy\Test-Files\" >nul
if exist "test_file.txt" move "test_file.txt" "Experimental-Legacy\Test-Files\" >nul
if exist "debug-import-test.js" move "debug-import-test.js" "Experimental-Legacy\Test-Files\" >nul
if exist "test.o" move "test.o" "Experimental-Legacy\Test-Files\" >nul
if exist "payload-builder.log" move "payload-builder.log" "Experimental-Legacy\Test-Files\" >nul
echo [OK] Experimental Legacy organized

echo.
echo ==========================================
echo      ORGANIZATION COMPLETE!
echo ==========================================
echo.
echo Next steps:
echo 1. Review new folder structure
echo 2. Create README.md files for each project
echo 3. Update INDEX.md with new paths
echo 4. Test that all systems still work
echo.
pause
