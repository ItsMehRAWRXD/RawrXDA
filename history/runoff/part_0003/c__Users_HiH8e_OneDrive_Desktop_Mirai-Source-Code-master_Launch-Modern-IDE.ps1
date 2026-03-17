<#
.SYNOPSIS
    Launches the Beast IDE in Microsoft Edge App Mode for a standalone experience with modern web standards support.
.DESCRIPTION
    The legacy Beast-IDEBrowser.ps1 uses the .NET WebBrowser control which is based on Internet Explorer 7-11.
    The IDEre2.html file uses modern JavaScript (ES6+, async/await) which is NOT supported by the WebBrowser control.
    This script launches the IDE in Microsoft Edge "App Mode" which provides a windowed, standalone experience
    while using the modern Chromium engine required by the IDE.
#>

$idePath = "c:\Users\HiH8e\OneDrive\Desktop\IDEre2.html"
$edgePathX86 = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
$edgePathX64 = "C:\Program Files\Microsoft\Edge\Application\msedge.exe"

$targetEdge = $null

if (Test-Path $edgePathX86) {
    $targetEdge = $edgePathX86
} elseif (Test-Path $edgePathX64) {
    $targetEdge = $edgePathX64
}

if ($targetEdge) {
    Write-Host "🚀 Launching Beast IDE in Modern App Mode..." -ForegroundColor Cyan
    # Use a separate user data dir to ensure a clean session and no interference with main browser
    $userDataDir = "$env:LOCALAPPDATA\BeastIDE_Profile"
    
    $process = Start-Process -FilePath $targetEdge -ArgumentList "--app=""file:///$idePath""", "--user-data-dir=""$userDataDir""", "--force-renderer-accessibility" -PassThru
    
    if ($process) {
        Write-Host "✅ IDE Launched successfully!" -ForegroundColor Green
        Write-Host "   PID: $($process.Id)"
    } else {
        Write-Host "❌ Failed to launch Edge." -ForegroundColor Red
    }
} else {
    Write-Host "⚠️ Microsoft Edge not found. Launching in default browser..." -ForegroundColor Yellow
    Start-Process "file:///$idePath"
}
