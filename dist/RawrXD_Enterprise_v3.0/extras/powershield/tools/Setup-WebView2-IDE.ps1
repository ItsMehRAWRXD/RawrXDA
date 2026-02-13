<#
.SYNOPSIS
    Setup script for RawrXD WebView2 Edition

.DESCRIPTION
    Installs and configures WebView2 dependencies for optimal performance
#>

Write-Host @"
🚀 RawrXD WebView2 Setup
=========================

Setting up your PowerShell + HTML hybrid IDE...
This gives you Electron-like capabilities but with native Windows performance!

Features:
✅ Modern HTML/CSS/JavaScript UI
✅ Native Windows WebView2 engine
✅ PowerShell backend integration
✅ 4K @ 160Hz optimized
✅ No Node.js overhead
✅ Direct Ollama integration

"@ -ForegroundColor Cyan

# Check Windows version
$winVersion = [System.Environment]::OSVersion.Version
if ($winVersion.Major -lt 10) {
    Write-Host "❌ Windows 10 or later required for WebView2" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Windows version compatible" -ForegroundColor Green

# Check for WebView2 Runtime
function Test-WebView2Runtime {
    $webView2Paths = @(
        "$env:ProgramFiles\Microsoft\EdgeWebView\Application",
        "$env:ProgramFiles(x86)\Microsoft\EdgeWebView\Application",
        "$env:LOCALAPPDATA\Microsoft\EdgeWebView\Application"
    )

    foreach ($path in $webView2Paths) {
        if (Test-Path $path) {
            $versions = Get-ChildItem $path -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' }
            if ($versions) {
                $latest = $versions | Sort-Object { [Version]$_.Name } -Descending | Select-Object -First 1
                Write-Host "✅ WebView2 Runtime found: $($latest.Name)" -ForegroundColor Green
                return $true
            }
        }
    }
    return $false
}

# Install WebView2 Runtime if needed
if (-not (Test-WebView2Runtime)) {
    Write-Host "📦 WebView2 Runtime not found, installing..." -ForegroundColor Yellow

    try {
        # Download WebView2 bootstrapper
        $url = "https://go.microsoft.com/fwlink/p/?LinkId=2124703"
        $installer = "$env:TEMP\MicrosoftEdgeWebview2Setup.exe"

        Write-Host "⬇️  Downloading WebView2..." -ForegroundColor Cyan
        Invoke-WebRequest -Uri $url -OutFile $installer

        Write-Host "🔧 Installing WebView2 Runtime..." -ForegroundColor Cyan
        Start-Process -FilePath $installer -ArgumentList "/silent", "/install" -Wait

        Remove-Item $installer -ErrorAction SilentlyContinue

        if (Test-WebView2Runtime) {
            Write-Host "✅ WebView2 Runtime installed successfully!" -ForegroundColor Green
        }
        else {
            Write-Host "⚠️  WebView2 installation may have failed" -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "⚠️  Could not auto-install WebView2: $($_.Exception.Message)" -ForegroundColor Yellow
        Write-Host "📥 Please install manually from: https://developer.microsoft.com/en-us/microsoft-edge/webview2/" -ForegroundColor Cyan
    }
}

# Check for .NET WebView2 package
function Install-WebView2NuGet {
    Write-Host "📦 Installing WebView2 .NET package..." -ForegroundColor Cyan

    try {
        # Check if NuGet is available
        if (-not (Get-Command "nuget.exe" -ErrorAction SilentlyContinue)) {
            Write-Host "📥 Installing NuGet..." -ForegroundColor Yellow

            # Install NuGet provider
            if (-not (Get-PackageProvider -Name NuGet -ErrorAction SilentlyContinue)) {
                Install-PackageProvider -Name NuGet -Force -Scope CurrentUser
            }

            # Install PowerShellGet if needed
            if (-not (Get-Module PowerShellGet -ListAvailable)) {
                Install-Module PowerShellGet -Force -Scope CurrentUser
            }
        }

        # Create a temp project to install WebView2
        $tempDir = "$env:TEMP\RawrXD-WebView2-Setup"
        if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
        New-Item -Path $tempDir -ItemType Directory | Out-Null

        Push-Location $tempDir

        # Create a minimal project file
        @"
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net48</TargetFramework>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="Microsoft.Web.WebView2" Version="1.0.2420.47" />
  </ItemGroup>
</Project>
"@ | Out-File "WebView2Setup.csproj" -Encoding UTF8

        # Install package
        if (Get-Command "dotnet.exe" -ErrorAction SilentlyContinue) {
            dotnet restore

            # Copy to user directory
            $userPackages = "$env:USERPROFILE\.nuget\packages"
            if (Test-Path ".\packages\Microsoft.Web.WebView2*") {
                $srcPath = Get-ChildItem ".\packages\Microsoft.Web.WebView2*" | Select-Object -First 1
                $destPath = "$userPackages\microsoft.web.webview2"

                if (-not (Test-Path $destPath)) {
                    New-Item -Path $destPath -ItemType Directory -Force | Out-Null
                }
                Copy-Item $srcPath.FullName "$destPath\$($srcPath.Name)" -Recurse -Force

                Write-Host "✅ WebView2 .NET package installed" -ForegroundColor Green
            }
        }
        else {
            Write-Host "⚠️  .NET CLI not found, WebView2 will use runtime fallback" -ForegroundColor Yellow
        }

        Pop-Location
        Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    catch {
        Write-Host "⚠️  Could not install WebView2 .NET package: $($_.Exception.Message)" -ForegroundColor Yellow
        Write-Host "📝 IDE will use WebView2 runtime directly" -ForegroundColor Cyan
        Pop-Location
        Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Install-WebView2NuGet

# Check for Ollama (for AI backend)
if (Get-Command "ollama.exe" -ErrorAction SilentlyContinue) {
    Write-Host "✅ Ollama found - AI features will be available" -ForegroundColor Green

    # Test Ollama connection
    try {
        $ollamaVersion = ollama --version 2>$null
        Write-Host "🧠 Ollama version: $ollamaVersion" -ForegroundColor Cyan

        # List models
        $models = ollama list 2>$null | Where-Object { $_ -notmatch "NAME.*SIZE.*MODIFIED" -and $_.Trim() }
        if ($models) {
            Write-Host "🤖 Available models:" -ForegroundColor Cyan
            $models | ForEach-Object { Write-Host "   • $($_.Split()[0])" -ForegroundColor White }
        }
    }
    catch {
        Write-Host "⚠️  Ollama found but not responding" -ForegroundColor Yellow
    }
}
else {
    Write-Host "⚠️  Ollama not found - AI features will be limited" -ForegroundColor Yellow
    Write-Host "📥 Install from: https://ollama.ai" -ForegroundColor Cyan
}

# Optimize for 4K displays
Write-Host "`n🖥️  Display Optimization" -ForegroundColor Magenta
$screen = [System.Windows.Forms.Screen]::PrimaryScreen
Write-Host "📊 Primary Display: $($screen.Bounds.Width)x$($screen.Bounds.Height)" -ForegroundColor Cyan

if ($screen.Bounds.Width -ge 3840) {
    Write-Host "✅ 4K display detected - optimizations will be applied" -ForegroundColor Green
}
else {
    Write-Host "ℹ️  Non-4K display - standard optimizations will be used" -ForegroundColor Cyan
}

# Check HTML IDE file
$htmlPath = "D:\HTML-Projects\IDEre2.html"
if (Test-Path $htmlPath) {
    $fileSize = [math]::Round((Get-Item $htmlPath).Length / 1MB, 1)
    Write-Host "✅ HTML IDE found: $fileSize MB" -ForegroundColor Green
}
else {
    Write-Host "⚠️  HTML IDE not found at: $htmlPath" -ForegroundColor Yellow
    Write-Host "📁 Checking for other IDE files..." -ForegroundColor Cyan

    $htmlFiles = Get-ChildItem "D:\HTML-Projects\*.html" -ErrorAction SilentlyContinue
    if ($htmlFiles) {
        Write-Host "📄 Available HTML files:" -ForegroundColor Cyan
        $htmlFiles | ForEach-Object { Write-Host "   • $($_.Name)" -ForegroundColor White }
    }
}

# Create launch shortcut
$shortcutPath = "$env:USERPROFILE\Desktop\RawrXD WebView2.lnk"
try {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = "powershell.exe"
    $shortcut.Arguments = "-ExecutionPolicy Bypass -File `"C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-WebView.ps1`""
    $shortcut.WorkingDirectory = "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
    $shortcut.IconLocation = "shell32.dll,77"
    $shortcut.Description = "RawrXD WebView2 - HTML IDE with PowerShell Backend"
    $shortcut.Save()
    Write-Host "✅ Desktop shortcut created" -ForegroundColor Green
}
catch {
    Write-Host "⚠️  Could not create desktop shortcut: $($_.Exception.Message)" -ForegroundColor Yellow
}

Write-Host @"

🎉 Setup Complete!
==================

Your PowerShell + HTML hybrid IDE is ready!

🚀 Launch Options:
• Double-click desktop shortcut: "RawrXD WebView2"
• Run: .\RawrXD-WebView.ps1
• 4K mode: .\RawrXD-WebView.ps1 -Width 3200 -Height 1800
• Full screen: .\RawrXD-WebView.ps1 -FullScreen
• Debug mode: .\RawrXD-WebView.ps1 -Debug

💡 Features:
✅ Modern web UI (like Electron)
✅ Native Windows performance
✅ PowerShell backend integration
✅ Direct Ollama AI integration
✅ File system access
✅ 4K @ 160Hz optimized

🎯 This gives you the best of both worlds:
• Visual appeal of web technologies
• Performance of native Windows apps
• Full PowerShell ecosystem access

"@ -ForegroundColor Green

Read-Host "`nPress Enter to launch the IDE"

# Launch the IDE
& "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-WebView.ps1" -Debug
