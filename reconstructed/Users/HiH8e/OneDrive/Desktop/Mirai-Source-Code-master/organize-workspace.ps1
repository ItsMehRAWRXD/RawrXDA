param([string]$action = "plan")

# Get workspace root
$root = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master"
Set-Location $root

# Define file mappings
$moves = @{
    # BigDaddyG Project
    "Projects\BigDaddyG" = @(
        "bigdaddyg-launcher-interactive.ps1",
        "START-BIGDADDYG.bat",
        "BIGDADDYG-EXECUTIVE-SUMMARY.md",
        "BIGDADDYG-LAUNCHER-COMPLETE.md",
        "BIGDADDYG-LAUNCHER-GUIDE.md",
        "BIGDADDYG-LAUNCHER-SETUP.md",
        "BIGDADDYG-QUICK-REFERENCE.txt",
        "D-DRIVE-AUDIT-COMPLETE.md",
        "D-DRIVE-AUDIT-REPORT.json",
        "D-DRIVE-COMPLETE-EXPLORATION.md",
        "INTEGRATION-DECISION.md",
        "bigdaddyg-beast-mini.py",
        "BigDaddyG-Beast-Modelfile",
        "BigDaddyG-Beast-Optimized-Modelfile"
    );
    
    # Beast System
    "Projects\Beast-System" = @(
        "beast-mini-standalone.py",
        "beast-quick-start.py",
        "beast-swarm-demo.html",
        "beast-swarm-demo (2).html",
        "beast-swarm-demo.html.cust",
        "beast-swarm-demo.html.negcomp",
        "beast-swarm-system.py",
        "beast-swarm-web.js",
        "beast-training-suite.py",
        "Beast-IDEBrowser.ps1",
        "launch-beast-browser.ps1",
        "test-beast-performance.bat",
        "ModelfileBeast"
    );
    
    # RawrZ Platform
    "Projects\RawrZ" = @(
        "BRxC.html",
        "BRxC-Recovery.html",
        "RawrBrowser.ps1",
        "RawrCompress-GUI.ps1",
        "RawrZ-Payload-Builder-GUI.ps1",
        "RAWRZ-COMPONENTS-ANALYSIS.md",
        "quick-setup-rawrz-http.bat",
        "quick-build-rawrzdesktop.bat"
    );
    
    # CyberForge
    "Projects\CyberForge" = @(
        "README-CYBERFORGE.md",
        "package-cyberforge.json",
        "comprehensive-ide-fix.ps1",
        "fix-dom-errors.ps1",
        "fix-domready-function.ps1",
        "fix-js-syntax-errors.ps1",
        "ide-fixes-template.html",
        "ide-fixes.js",
        "DOM-FIXES-ANALYSIS.md",
        "JAVASCRIPT_FIXES_GUIDE.md"
    );
    
    # Build Tools
    "Tools\Build-System" = @(
        "Master-Build-All-Projects.ps1",
        "MASTER-CONTROL.bat",
        "QUICK-BUILD-ALL.bat",
        "quick-build.bat",
        "quick-build-ohgee.bat",
        "VERIFY-SYSTEM.bat",
        "COMPLETE-TEST-SYSTEM.bat",
        "Build-Windows.psm1",
        "build-mirai-windows.bat",
        "Build-Mirai-Windows.ps1",
        "Ultimate-Build-System.ps1"
    );
    
    # Utilities
    "Tools\Utilities" = @(
        "master-cli.js",
        "payload-cli.js",
        "payload_builder.py",
        "start-demo-server.py",
        "cli.js",
        "orchestral-server.js",
        "orchestra.mjs",
        "backend-server.js",
        "backend.mjs"
    );
    
    # Launchers
    "Tools\Launchers" = @(
        "Launch-IDE-Servers.bat",
        "launch-ide.bat",
        "Launch-Modern-IDE.ps1",
        "Start-IDE-Servers.ps1",
        "GUI-Test.ps1",
        "Get-ModelManifest.ps1",
        "Integrated-Weaponization-Suite.ps1"
    );
    
    # Documentation - Guides
    "Documentation\Guides" = @(
        "COMPLETE-FEATURES-GUIDE.md",
        "COMPLETE-INTEGRATION-ARSENAL.md",
        "INCOMPLETE-COMPONENTS-GUIDE.md",
        "OPTIMIZATION_GUIDE.md",
        "README-AV-SCANNERS.md",
        "README-FINAL.md",
        "README-ide-cli.md",
        "README.md",
        "ML-QUICK-START.md",
        "TESTING-GUIDE.md"
    );
    
    # Documentation - Status Reports
    "Documentation\Status-Reports" = @(
        "PHASE-2-FINAL-SUMMARY.md",
        "PHASE-2-COMPLETION-SUMMARY.md",
        "PHASE-2-DELIVERY-SUMMARY.md",
        "PHASE-2-EXTENDED-COMPLETION.md",
        "PHASE-2-EXTENSION-ANALYSIS.md",
        "PHASE-3-EXECUTION-PLAN.md",
        "PHASE-3-EXECUTION-START.md",
        "PHASE-3-OPTION-A-EXECUTION.md",
        "OPTION-A-EXECUTION-COMPLETE.md",
        "SESSION-COMPLETE-SUMMARY.md",
        "SESSION-COMPLETION-REPORT.md",
        "SESSION-FINAL-SUMMARY.md",
        "COMPLETION-PROGRESS-REPORT.md",
        "COMPLETION-STATUS-SUMMARY.md",
        "DELIVERY-SUMMARY.md"
    );
    
    # Recovery & Audit
    "Recovery-Audit-Reports\D-Drive-Audits" = @(
        "AUDIT-D-DRIVE.ps1",
        "D-DRIVE-RECOVERY-AUDIT.md",
        "explore-d-drive.ps1",
        "explore-recovery.ps1",
        "D-DRIVE-AUDIT-COMPLETE.md"
    );
    
    "Recovery-Audit-Reports\Recovery-Reports" = @(
        "COMPREHENSIVE-AUDIT-MODERNIZATION-PLAN.md",
        "COMPREHENSIVE-AUDIT-REPORT.md",
        "DETAILED-INCOMPLETE-AUDIT.md",
        "RECOVERY-AUDIT-SUMMARY.md",
        "RECOVERY-COMPONENTS-INVENTORY-PHASE-1.md",
        "RECOVERY-COMPONENTS-INVENTORY-PHASE-2.md",
        "RECOVERY-EXPLORATION-REPORT.md",
        "RECOVERED-COMPONENTS-ANALYSIS.md"
    );
    
    # Configuration
    "Configuration" = @(
        "package.json",
        "package-lock.json",
        ".hintrc",
        "sample_training_data.json"
    );
    
    # Experimental & Legacy
    "Experimental-Legacy\Legacy-Scripts" = @(
        "PowerShell-GUI-Showcase.ps1",
        "PowerShell-HTML-Browser-IDE.ps1",
        "PowerShell-Studio-Pro.clean.ps1",
        "PowerShell-Studio-Pro.ps1",
        "cleanup-duplicate-handlers.ps1",
        "nuclear-handler-cleanup.ps1",
        "analyze-rawrz-components.ps1",
        "final-ide-verification.ps1"
    );
    
    "Experimental-Legacy\Test-Files" = @(
        "test-dom-fixes.ps1",
        "verify-js-fixes.ps1",
        "test-compatibility.bat",
        "simple-integration-test.js",
        "test_file.txt",
        "debug-import-test.js",
        "test.o",
        "payload-builder.log"
    );
}

# Summary statistics
$stats = @{
    total = 0
    existing = 0
    missing = 0
}

Write-Host "`n🔍 FILE ORGANIZATION PLAN`n" -ForegroundColor Cyan

foreach ($dest in $moves.Keys | Sort-Object) {
    $files = $moves[$dest]
    $existing = 0
    $missing = 0
    
    Write-Host "📁 $dest" -ForegroundColor Yellow
    
    foreach ($file in $files) {
        $stats.total++
        if (Test-Path $file) {
            $existing++
            $stats.existing++
            if ($action -eq "verbose") {
                Write-Host "   ✅ $file"
            }
        } else {
            $missing++
            $stats.missing++
            if ($action -eq "verbose") {
                Write-Host "   ⚠️  MISSING: $file" -ForegroundColor Yellow
            }
        }
    }
    
    Write-Host "   Found: $existing / $($files.Count) files" -ForegroundColor Green
    
    if ($missing -gt 0) {
        Write-Host "   Missing: $missing files" -ForegroundColor Yellow
    }
}

Write-Host "`n📊 SUMMARY`n" -ForegroundColor Cyan
Write-Host "Total files to move: $($stats.total)"
Write-Host "Files found: $($stats.existing)" -ForegroundColor Green
Write-Host "Files missing: $($stats.missing)" -ForegroundColor Yellow

if ($action -eq "execute") {
    Write-Host "`n🚀 EXECUTING FILE MOVES..`n" -ForegroundColor Green
    
    foreach ($dest in $moves.Keys) {
        $files = $moves[$dest]
        
        foreach ($file in $files) {
            if (Test-Path $file) {
                try {
                    Move-Item -Path $file -Destination $dest -Force -ErrorAction Stop
                    Write-Host "✅ Moved: $file → $dest" -ForegroundColor Green
                } catch {
                    Write-Host "❌ Error moving $file : $_" -ForegroundColor Red
                }
            }
        }
    }
    
    Write-Host "`n✅ ORGANIZATION COMPLETE!`n" -ForegroundColor Green
}
else {
    Write-Host "`n💡 To execute moves, run: .\organize-workspace.ps1 -action execute`n" -ForegroundColor Cyan
}
