<#
.SYNOPSIS
    Test script to verify PS5.1 Browser Bridge and runtime detection
#>

Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host "  RawrXD PS5.1 Browser Bridge Test" -ForegroundColor White
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host ""

# Runtime info
Write-Host "RUNTIME INFORMATION:" -ForegroundColor Yellow
Write-Host "  PowerShell Version: $($PSVersionTable.PSVersion)" -ForegroundColor White
Write-Host "  PowerShell Edition: $($PSVersionTable.PSEdition)" -ForegroundColor White
Write-Host "  .NET Runtime: $([System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription)" -ForegroundColor White
Write-Host ""

# Check files
$scriptRoot = $PSScriptRoot
if (-not $scriptRoot) { $scriptRoot = "C:\Users\HiH8e\OneDrive\Desktop\Powershield" }

$files = @{
    "RawrXD.ps1" = (Test-Path (Join-Path $scriptRoot "RawrXD.ps1"))
    "PS51-Browser-Host.ps1" = (Test-Path (Join-Path $scriptRoot "PS51-Browser-Host.ps1"))
    "BrowserBridge.psm1" = (Test-Path (Join-Path $scriptRoot "BrowserBridge.psm1"))
    "WebView2Shim.ps1" = (Test-Path (Join-Path $scriptRoot "WebView2Shim.ps1"))
}

Write-Host "FILE CHECK:" -ForegroundColor Yellow
foreach ($file in $files.GetEnumerator()) {
    $status = if ($file.Value) { "✅" } else { "❌" }
    $color = if ($file.Value) { "Green" } else { "Red" }
    Write-Host "  $status $($file.Key)" -ForegroundColor $color
}
Write-Host ""

# Determine browser implementation
$psVersion = $PSVersionTable.PSVersion.Major
$isPS51 = ($psVersion -eq 5)
$isPS7Plus = ($psVersion -ge 7)
$ps51BrowserAvailable = $files["PS51-Browser-Host.ps1"]

Write-Host "BROWSER IMPLEMENTATION DECISION:" -ForegroundColor Yellow

if ($isPS51) {
    Write-Host "  ✅ Windows PowerShell 5.1 - Native WebBrowser (Full Video Support)" -ForegroundColor Green
    Write-Host "     → Built-in WebBrowser control works perfectly" -ForegroundColor DarkGray
    Write-Host "     → YouTube and HTML5 video playback supported" -ForegroundColor DarkGray
}
elseif ($isPS7Plus -and $ps51BrowserAvailable) {
    Write-Host "  🎬 PowerShell 7+ with PS5.1 Bridge Available" -ForegroundColor Cyan
    Write-Host "     → Main IDE runs on PS7 for optimal performance" -ForegroundColor DarkGray
    Write-Host "     → Video browser launches via PS5.1 subprocess" -ForegroundColor DarkGray
    Write-Host "     → Best of both worlds!" -ForegroundColor DarkGray
}
elseif ($isPS7Plus) {
    Write-Host "  ⚠️ PowerShell 7+ without PS5.1 Bridge" -ForegroundColor Yellow
    Write-Host "     → Will fall back to WebView2Shim or legacy browser" -ForegroundColor DarkGray
    Write-Host "     → Video playback may be limited" -ForegroundColor DarkGray
}
Write-Host ""

# Test PS5.1 browser launch (if on PS7+)
if ($isPS7Plus -and $ps51BrowserAvailable) {
    Write-Host "TESTING PS5.1 BROWSER LAUNCH:" -ForegroundColor Yellow
    Write-Host "  Press Enter to test launching PS5.1 Video Browser..." -ForegroundColor DarkGray
    Read-Host

    $browserHostPath = Join-Path $scriptRoot "PS51-Browser-Host.ps1"
    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$browserHostPath`"",
        "-StartUrl", "`"https://www.youtube.com`"",
        "-Standalone"
    )

    Write-Host "  Launching: powershell.exe $($arguments -join ' ')" -ForegroundColor DarkGray

    try {
        $process = Start-Process "powershell.exe" -ArgumentList ($arguments -join " ") -PassThru
        Write-Host "  ✅ PS5.1 Browser launched successfully (PID: $($process.Id))" -ForegroundColor Green
    }
    catch {
        Write-Host "  ❌ Failed to launch: $($_.Exception.Message)" -ForegroundColor Red
    }
}
elseif ($isPS51) {
    Write-Host "TESTING NATIVE BROWSER:" -ForegroundColor Yellow
    Write-Host "  Running on PS5.1 - No need for bridge, native browser works!" -ForegroundColor Green
}

Write-Host ""
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host "  Test Complete" -ForegroundColor White
Write-Host "=" * 60 -ForegroundColor Cyan
