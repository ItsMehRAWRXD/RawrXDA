<#
.SYNOPSIS
    Test script for RawrXD Runtime Detection System
.DESCRIPTION
    Tests the runtime detection and WebView2/WinForms compatibility on both
    Windows PowerShell 5.1 and PowerShell 7.x
#>

param(
  [switch]$Verbose
)

Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   RAWRXD RUNTIME DETECTION TEST" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Display PowerShell version
Write-Host "POWERSHELL INFORMATION:" -ForegroundColor Yellow
Write-Host "  Version: $($PSVersionTable.PSVersion)" -ForegroundColor White
Write-Host "  Edition: $(if ($PSVersionTable.PSEdition) { $PSVersionTable.PSEdition } else { 'Desktop' })" -ForegroundColor White
Write-Host "  Host: $($Host.Name)" -ForegroundColor White
Write-Host ""

# Display .NET version
Write-Host ".NET RUNTIME INFORMATION:" -ForegroundColor Yellow
try {
  $dotnetVersion = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
  Write-Host "  Full Description: $dotnetVersion" -ForegroundColor White

  if ($dotnetVersion -match "\.NET\s+(\d+)") {
    $majorVersion = [int]$matches[1]
    Write-Host "  Major Version: $majorVersion" -ForegroundColor White

    if ($majorVersion -ge 9) {
      Write-Host "  ⚠️ .NET 9+ detected - WebView2 WinForms may have issues" -ForegroundColor Yellow
      Write-Host "     System.Windows.Forms.ContextMenu is deprecated" -ForegroundColor DarkYellow
    }
    elseif ($majorVersion -ge 6) {
      Write-Host "  ✅ .NET 6-8 detected - Should be compatible" -ForegroundColor Green
    }
  }
  elseif ($dotnetVersion -match "\.NET Framework\s+(\d+)\.(\d+)") {
    Write-Host "  Major Version: $($matches[1]).$($matches[2])" -ForegroundColor White
    Write-Host "  ✅ .NET Framework - Full compatibility" -ForegroundColor Green
  }
}
catch {
  Write-Host "  Error detecting .NET version: $($_.Exception.Message)" -ForegroundColor Red
}
Write-Host ""

# Test WebView2 Runtime
Write-Host "WEBVIEW2 RUNTIME CHECK:" -ForegroundColor Yellow
$webView2Paths = @(
  "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
  "HKLM:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
)

$webView2Found = $false
foreach ($path in $webView2Paths) {
  if (Test-Path $path) {
    $webView2Found = $true
    Write-Host "  ✅ WebView2 Runtime found in registry" -ForegroundColor Green
    break
  }
}

if (-not $webView2Found) {
  # Check file system
  $wv2Locations = @(
    "$env:ProgramFiles\Microsoft\EdgeWebView\Application",
    "$env:ProgramFiles(x86)\Microsoft\EdgeWebView\Application",
    "$env:LocalAppData\Microsoft\EdgeWebView\Application"
  )

  foreach ($loc in $wv2Locations) {
    if (Test-Path $loc) {
      $webView2Found = $true
      Write-Host "  ✅ WebView2 Runtime found at: $loc" -ForegroundColor Green
      break
    }
  }
}

if (-not $webView2Found) {
  Write-Host "  ⚠️ WebView2 Runtime NOT found" -ForegroundColor Yellow
  Write-Host "     Will use legacy browser or WebView2Shim" -ForegroundColor DarkYellow
}
Write-Host ""

# Test WebView2Shim
Write-Host "WEBVIEW2SHIM CHECK:" -ForegroundColor Yellow
$shimPath = Join-Path $PSScriptRoot "WebView2Shim.ps1"
if (Test-Path $shimPath) {
  Write-Host "  ✅ WebView2Shim.ps1 found" -ForegroundColor Green

  # Try to load it
  try {
    . $shimPath
    if (Get-Command Initialize-WebView2Shim -ErrorAction SilentlyContinue) {
      Write-Host "  ✅ WebView2Shim functions available" -ForegroundColor Green
    }
  }
  catch {
    Write-Host "  ⚠️ Error loading WebView2Shim: $($_.Exception.Message)" -ForegroundColor Yellow
  }
}
else {
  Write-Host "  ⚠️ WebView2Shim.ps1 NOT found at: $shimPath" -ForegroundColor Yellow
}
Write-Host ""

# Test Windows Forms
Write-Host "WINDOWS FORMS CHECK:" -ForegroundColor Yellow
try {
  Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
  Write-Host "  ✅ System.Windows.Forms loaded successfully" -ForegroundColor Green

  # Test creating a form
  $testForm = New-Object System.Windows.Forms.Form -ErrorAction Stop
  $testForm.Dispose()
  Write-Host "  ✅ Form creation test passed" -ForegroundColor Green

  # Test ContextMenuStrip (always available)
  $testMenu = New-Object System.Windows.Forms.ContextMenuStrip -ErrorAction Stop
  $testMenu.Dispose()
  Write-Host "  ✅ ContextMenuStrip available" -ForegroundColor Green
}
catch {
  Write-Host "  ❌ Windows Forms error: $($_.Exception.Message)" -ForegroundColor Red
}
Write-Host ""

# Recommendations
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   RECOMMENDATIONS" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan

$currentPsEdition = if ($PSVersionTable.PSEdition) { $PSVersionTable.PSEdition } else { "Desktop" }

if ($currentPsEdition -eq "Desktop") {
  Write-Host "✅ You're using Windows PowerShell 5.1" -ForegroundColor Green
  Write-Host "   Full compatibility mode - all features should work" -ForegroundColor White
}
elseif ($PSVersionTable.PSVersion.Major -ge 7) {
  if ($dotnetVersion -match "\.NET\s+9") {
    Write-Host "⚠️ You're using PowerShell 7.x with .NET 9" -ForegroundColor Yellow
    Write-Host "   WebView2 WinForms has compatibility issues on .NET 9" -ForegroundColor White
    Write-Host "   RawrXD will use WebView2Shim or legacy browser" -ForegroundColor White
    Write-Host "" -ForegroundColor White
    Write-Host "   For full WebView2 support, you can:" -ForegroundColor White
    Write-Host "   1. Use Windows PowerShell 5.1 (powershell.exe)" -ForegroundColor Gray
    Write-Host "   2. Continue with legacy browser (less features)" -ForegroundColor Gray
  }
  else {
    Write-Host "✅ You're using PowerShell 7.x" -ForegroundColor Green
    Write-Host "   Should be compatible with WebView2" -ForegroundColor White
  }
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   TEST COMPLETE" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
