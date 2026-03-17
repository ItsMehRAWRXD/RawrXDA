# Quick organization script - moves files to projects
$root = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master"
Push-Location $root

# BigDaddyG
"bigdaddyg-launcher-interactive.ps1", "START-BIGDADDYG.bat", "BIGDADDYG-EXECUTIVE-SUMMARY.md", "BIGDADDYG-LAUNCHER-COMPLETE.md", "BIGDADDYG-LAUNCHER-GUIDE.md", "BIGDADDYG-LAUNCHER-SETUP.md", "BIGDADDYG-QUICK-REFERENCE.txt", "D-DRIVE-AUDIT-COMPLETE.md", "INTEGRATION-DECISION.md" | ForEach-Object {
    if (Test-Path $_) {
        Move-Item $_ "Projects\BigDaddyG\" -Force
        Write-Host "✅ $_"
    }
}

Write-Host "BigDaddyG files organized!"
Pop-Location
