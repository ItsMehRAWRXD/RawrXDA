#!/usr/bin/env pwsh
# Fix Windows Defender Exclusions for BigDaddyG Development
# This script adds exclusions to prevent Defender from deleting your dev files

Write-Host "🛡️ Fixing Windows Defender Exclusions for BigDaddyG Development..." -ForegroundColor Cyan

# Function to add exclusion
function Add-DefenderExclusion {
    param([string]$Path, [string]$Type = "Path")
    try {
        Add-MpPreference -ExclusionPath $Path -ExclusionType $Type
        Write-Host "✅ Added exclusion for: $Path" -ForegroundColor Green
    } catch {
        Write-Host "❌ Failed to add exclusion for: $Path - $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Add exclusions for all your development directories
$exclusions = @(
    "D:\",
    "D:\puppeteer-agent",
    "D:\BigDaddyG*",
    "D:\code-supernova*",
    "D:\agents",
    "D:\core",
    "D:\frontend",
    "D:\config",
    "D:\tools",
    "D:\generated",
    "D:\BIGDADDYG-RECOVERY",
    "D:\00-Development",
    "D:\01-AI-Models",
    "D:\02-IDE-Projects",
    "D:\03-Documentation",
    "D:\04-Testing",
    "D:\05-Utilities",
    "D:\06-Data",
    "D:\07-Executables",
    "D:\08-Temp",
    "D:\09-Archives",
    "D:\10-Data-Configs",
    "D:\11-Temp-Working",
    "D:\12-Archives-Backups",
    "D:\13-Recovery-Files",
    "D:\14-Desktop-Files",
    "D:\15-Downloads-Files",
    "C:\Users\HiH8e\.cursor",
    "C:\Users\HiH8e\AppData\Roaming\Cursor",
    "C:\Users\HiH8e\AppData\Local\Cursor"
)

Write-Host "📁 Adding directory exclusions..." -ForegroundColor Yellow
foreach ($exclusion in $exclusions) {
    Add-DefenderExclusion -Path $exclusion -Type "Path"
}

# Add file extension exclusions
$extensions = @(".js", ".ts", ".jsx", ".tsx", ".py", ".html", ".css", ".json", ".md", ".txt", ".bat", ".ps1", ".sh", ".yml", ".yaml", ".xml", ".sql", ".php", ".java", ".cpp", ".c", ".h", ".hpp", ".cs", ".go", ".rs", ".rb", ".swift", ".kt", ".scala", ".r", ".m", ".pl", ".lua", ".dart", ".elm", ".clj", ".hs", ".ml", ".fs", ".vb", ".asm", ".s", ".asmx", ".aspx", ".jsp", ".erb", ".haml", ".slim", ".pug", ".ejs", ".hbs", ".mustache", ".twig", ".liquid", ".njk", ".11ty.js", ".vue", ".svelte", ".astro", ".solid", ".qwik", ".lit", ".stencil", ".riot", ".mithril", ".preact", ".inferno", ".hyperapp")

Write-Host "📄 Adding file extension exclusions..." -ForegroundColor Yellow
foreach ($ext in $extensions) {
    Add-DefenderExclusion -Path $ext -Type "Extension"
}

# Add process exclusions
$processes = @(
    "node.exe",
    "npm.exe",
    "npx.exe",
    "electron.exe",
    "Cursor.exe",
    "code.exe",
    "pwsh.exe",
    "powershell.exe",
    "cmd.exe",
    "python.exe",
    "python3.exe",
    "pip.exe"
)

Write-Host "⚙️ Adding process exclusions..." -ForegroundColor Yellow
foreach ($process in $processes) {
    Add-DefenderExclusion -Path $process -Type "Process"
}

# Try to restore quarantined files
Write-Host "🔄 Attempting to restore quarantined files..." -ForegroundColor Yellow
try {
    $quarantinedFiles = Get-MpThreatDetection | Where-Object {$_.ThreatStatusID -eq 3} # 3 = Quarantined
    foreach ($threat in $quarantinedFiles) {
        try {
            Restore-MpThreatDetection -ThreatID $threat.DetectionID
            Write-Host "✅ Restored: $($threat.Resources)" -ForegroundColor Green
        } catch {
            Write-Host "❌ Failed to restore: $($threat.Resources) - $($_.Exception.Message)" -ForegroundColor Red
        }
    }
} catch {
    Write-Host "❌ Failed to restore quarantined files: $($_.Exception.Message)" -ForegroundColor Red
}

# Disable real-time protection temporarily for development
Write-Host "⚠️ Temporarily disabling real-time protection for development..." -ForegroundColor Yellow
try {
    Set-MpPreference -DisableRealtimeMonitoring $true
    Write-Host "✅ Real-time protection disabled" -ForegroundColor Green
    Write-Host "⚠️ Remember to re-enable it later with: Set-MpPreference -DisableRealtimeMonitoring `$false" -ForegroundColor Yellow
} catch {
    Write-Host "❌ Failed to disable real-time protection: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "🎉 Windows Defender exclusions configured!" -ForegroundColor Green
Write-Host "📝 Your development files should now be protected from automatic deletion." -ForegroundColor Cyan
Write-Host "⚠️ Real-time protection is currently disabled - remember to re-enable it when done developing." -ForegroundColor Yellow
