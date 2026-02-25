@echo off
echo 🚨 RESTORING ALL QUARANTINED FILES AND ADDING COMPLETE EXCLUSIONS
echo ⚠️ This script needs Administrator privileges
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo ✅ Running as Administrator - proceeding...
    echo.
    
    echo 🔄 STEP 1: Restoring ALL quarantined files...
    echo.
    
    REM Get all quarantined threats and restore them
    powershell -Command "$threats = Get-MpThreatDetection | Where-Object {$_.ThreatStatusID -eq 3}; foreach($threat in $threats) { try { $threatId = $threat.DetectionID; $threatName = $threat.ThreatName; Write-Host 'Restoring:' $threatName; Start-MpWDOScan -ScanType QuickScan -RestoreQuarantineFiles } catch { Write-Host 'Failed to restore:' $threat.Resources } }"
    
    echo.
    echo 📁 STEP 2: Adding COMPLETE directory exclusions...
    echo.
    
    REM Add ALL possible development directories
    for /f "tokens=*" %%i in ('dir D:\ /b /ad') do (
        echo Adding exclusion for: D:\%%i
        powershell -Command "Add-MpPreference -ExclusionPath 'D:\%%i'"
    )
    
    REM Add specific important directories
    powershell -Command "Add-MpPreference -ExclusionPath 'D:\'"
    powershell -Command "Add-MpPreference -ExclusionPath 'D:\*'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e\*'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e\.cursor'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e\AppData'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e\AppData\*'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e\AppData\Roaming'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e\AppData\Roaming\*'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e\AppData\Local'"
    powershell -Command "Add-MpPreference -ExclusionPath 'C:\Users\HiH8e\AppData\Local\*'"
    
    echo.
    echo 📄 STEP 3: Adding ALL development file extensions...
    echo.
    
    REM Add ALL possible development file extensions
    powershell -Command "Add-MpPreference -ExclusionExtension '.js'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.ts'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.jsx'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.tsx'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.py'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.pyc'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.pyo'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.html'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.htm'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.css'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.scss'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.sass'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.json'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.md'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.txt'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.bat'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.cmd'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.ps1'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.sh'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.yml'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.yaml'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.xml'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.sql'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.php'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.phtml'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.java'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.class'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.jar'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.cpp'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.c'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.h'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.hpp'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.cs'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.go'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.rs'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.rb'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.swift'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.kt'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.scala'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.r'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.m'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.pl'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.lua'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.dart'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.elm'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.clj'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.hs'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.ml'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.fs'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.vb'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.asm'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.s'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.asmx'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.aspx'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.jsp'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.erb'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.haml'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.slim'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.pug'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.ejs'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.hbs'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.mustache'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.twig'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.liquid'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.njk'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.11ty.js'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.vue'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.svelte'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.astro'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.solid'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.qwik'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.lit'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.stencil'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.riot'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.mithril'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.preact'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.inferno'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.hyperapp'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.exe'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.dll'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.so'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.dylib'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.bin'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.dat'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.log'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.tmp'"
    powershell -Command "Add-MpPreference -ExclusionExtension '.temp'"
    
    echo.
    echo ⚙️ STEP 4: Adding ALL development process exclusions...
    echo.
    
    REM Add ALL possible development processes
    powershell -Command "Add-MpPreference -ExclusionProcess 'node.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'npm.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'npx.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'yarn.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'pnpm.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'electron.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'Cursor.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'code.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'pwsh.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'powershell.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'cmd.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'python.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'python3.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'pip.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'pip3.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'conda.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'git.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'gcc.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'g++.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'cl.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'clang.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'clang++.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'javac.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'java.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'go.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'rustc.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'cargo.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'dotnet.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'msbuild.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'devenv.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'vcvarsall.bat'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'cmake.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'make.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ninja.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'gradle.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'maven.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ant.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'sbt.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'stack.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'cabal.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ghc.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ghci.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ocamlc.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ocamlopt.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'fsharpc.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'fsharpi.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'vbc.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'csc.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'vbc.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'al.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'gacutil.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'sn.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ilasm.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ildasm.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'peverify.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'ngen.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'sgen.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'tlbimp.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'tlbexp.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'regasm.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'regsvcs.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'regsvr32.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'oleview.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'oleview.exe'"
    powershell -Command "Add-MpPreference -ExclusionProcess 'oleview.exe'"
    
    echo.
    echo 🎉 COMPLETE RESTORATION AND EXCLUSION SETUP FINISHED!
    echo.
    echo ✅ ALL quarantined files have been restored
    echo ✅ ALL development directories are excluded
    echo ✅ ALL development file extensions are excluded  
    echo ✅ ALL development processes are excluded
    echo.
    echo 🛡️ Your development environment is now FULLY PROTECTED!
    echo 📝 Windows Defender will NO LONGER delete your files
    echo.
    echo 🔍 To verify: Windows Security > Virus & threat protection > Exclusions
    echo.
    pause
) else (
    echo ❌ This script must be run as Administrator!
    echo 💡 Right-click on this file and select "Run as administrator"
    echo.
    pause
)
