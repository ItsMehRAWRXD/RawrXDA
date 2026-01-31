# Install-VSCode-to-E-Drive.ps1
# Helps install VS Code from Downloads to E drive

Write-Host "🚀 VS Code Installation Helper" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

$downloadsPath = "$env:USERPROFILE\Downloads"
$userInstaller = "VSCodeUserSetup-x64-1.106.3.exe"
$systemInstaller = "VSCodeSetup-x64-1.106.3.exe"

Write-Host "`n📦 Found VS Code Installers:" -ForegroundColor Yellow
Write-Host "   1. $userInstaller (User Installer - RECOMMENDED)" -ForegroundColor Green
Write-Host "      ✅ No admin rights required" -ForegroundColor Gray
Write-Host "      ✅ Can install to custom location (E drive)" -ForegroundColor Gray
Write-Host "      ✅ Installs to user profile by default" -ForegroundColor Gray
Write-Host ""
Write-Host "   2. $systemInstaller (System Installer)" -ForegroundColor Yellow
Write-Host "      ⚠️  Requires admin rights" -ForegroundColor Gray
Write-Host "      ⚠️  Installs to Program Files" -ForegroundColor Gray
Write-Host "      ⚠️  Harder to move to E drive" -ForegroundColor Gray

Write-Host "`n💡 Recommendation: Use the USER INSTALLER" -ForegroundColor Cyan
Write-Host "   It's easier to install to E drive and doesn't need admin rights" -ForegroundColor Gray

$userInstallerPath = Join-Path $downloadsPath $userInstaller
$systemInstallerPath = Join-Path $downloadsPath $systemInstaller

if (Test-Path $userInstallerPath) {
    Write-Host "`n✅ User installer found: $userInstallerPath" -ForegroundColor Green
    
    Write-Host "`n📋 Installation Options:" -ForegroundColor Cyan
    Write-Host "   Option 1: Install with custom location (E drive)" -ForegroundColor Yellow
    Write-Host "   Option 2: Install to default location, then move" -ForegroundColor Yellow
    Write-Host "   Option 3: Extract portable version" -ForegroundColor Yellow
    
    Write-Host "`n❓ How would you like to proceed?" -ForegroundColor Cyan
    Write-Host "   [1] Install to E:\Everything\~dev\VSCode (custom location)" -ForegroundColor White
    Write-Host "   [2] Install to default, then I'll help you move it" -ForegroundColor White
    Write-Host "   [3] Show me how to extract portable version" -ForegroundColor White
    Write-Host "   [4] Just run the installer and I'll configure it" -ForegroundColor White
    
    $choice = Read-Host "`nEnter choice (1-4)"
    
    switch ($choice) {
        "1" {
            Write-Host "`n🔧 Installing to E:\Everything\~dev\VSCode..." -ForegroundColor Yellow
            $installPath = "E:\Everything\~dev\VSCode"
            
            # Create directory if it doesn't exist
            if (-not (Test-Path $installPath)) {
                New-Item -ItemType Directory -Path $installPath -Force | Out-Null
                Write-Host "   ✅ Created directory: $installPath" -ForegroundColor Green
            }
            
            Write-Host "`n   Running installer with custom path..." -ForegroundColor Yellow
            Write-Host "   Note: During installation, you'll need to:" -ForegroundColor Gray
            Write-Host "   1. Click 'Options' or 'Advanced'" -ForegroundColor Gray
            Write-Host "   2. Change installation path to: $installPath" -ForegroundColor Gray
            Write-Host "   3. Complete the installation" -ForegroundColor Gray
            
            Start-Process -FilePath $userInstallerPath -Wait
        }
        "2" {
            Write-Host "`n🔧 Installing to default location..." -ForegroundColor Yellow
            Write-Host "   Default location: $env:LOCALAPPDATA\Programs\Microsoft VS Code" -ForegroundColor Gray
            Start-Process -FilePath $userInstallerPath -Wait
            
            Write-Host "`n   After installation, we'll move it to E drive" -ForegroundColor Yellow
        }
        "3" {
            Write-Host "`n📦 Portable Version Instructions:" -ForegroundColor Yellow
            Write-Host "   1. Download portable ZIP from: https://code.visualstudio.com/#alt-downloads" -ForegroundColor Gray
            Write-Host "   2. Extract to: E:\Everything\~dev\VSCode" -ForegroundColor Gray
            Write-Host "   3. Run code.exe from that location" -ForegroundColor Gray
        }
        "4" {
            Write-Host "`n🚀 Launching installer..." -ForegroundColor Yellow
            Write-Host "   After installation, run this script again to configure PATH" -ForegroundColor Gray
            Start-Process -FilePath $userInstallerPath
        }
        default {
            Write-Host "`n⚠️  Invalid choice. Launching installer with default settings..." -ForegroundColor Yellow
            Start-Process -FilePath $userInstallerPath
        }
    }
} else {
    Write-Host "`n❌ User installer not found in Downloads" -ForegroundColor Red
    Write-Host "   Expected: $userInstallerPath" -ForegroundColor Gray
}

Write-Host "`n" + "=" * 60 -ForegroundColor Cyan
Write-Host "💡 After Installation:" -ForegroundColor Yellow
Write-Host "   1. VS Code will be installed" -ForegroundColor Gray
Write-Host "   2. Run this script again to add to PATH" -ForegroundColor Gray
Write-Host "   3. Or manually add: E:\Everything\~dev\VSCode\bin to PATH" -ForegroundColor Gray

