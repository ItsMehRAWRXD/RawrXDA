# Configure-VSCode-Path.ps1
# Configures VS Code installation on E drive

$vscodePath = "E:\Everything\~dev\VSCode"
$codeExe = Join-Path $vscodePath "Code.exe"
$binPath = Join-Path $vscodePath "bin"
$codeCmd = Join-Path $binPath "code.cmd"

Write-Host "🔧 Configuring VS Code Installation" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

# Verify installation
if (-not (Test-Path $codeExe)) {
    Write-Host "`n❌ VS Code not found at: $codeExe" -ForegroundColor Red
    Write-Host "   Please verify the installation completed successfully" -ForegroundColor Yellow
    exit 1
}

Write-Host "`n✅ VS Code found at: $vscodePath" -ForegroundColor Green

# Check for code.cmd
if (Test-Path $codeCmd) {
    Write-Host "✅ Command-line launcher found: $codeCmd" -ForegroundColor Green
} else {
    Write-Host "⚠️  code.cmd not found in bin directory" -ForegroundColor Yellow
    Write-Host "   This is normal if using a portable installation" -ForegroundColor Gray
}

# Add to PATH
Write-Host "`n📋 Adding VS Code to PATH..." -ForegroundColor Yellow

$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")

if ($currentPath -like "*$binPath*") {
    Write-Host "   ✅ VS Code is already in PATH" -ForegroundColor Green
} else {
    $newPath = "$currentPath;$binPath"
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "   ✅ Added $binPath to PATH" -ForegroundColor Green
    Write-Host "   ⚠️  You may need to restart your terminal for PATH changes to take effect" -ForegroundColor Yellow
}

# Test installation
Write-Host "`n🧪 Testing VS Code installation..." -ForegroundColor Yellow

try {
    $version = & $codeExe --version 2>&1
    if ($LASTEXITCODE -eq 0 -or $version) {
        Write-Host "   ✅ VS Code is working!" -ForegroundColor Green
        Write-Host "   Version info:" -ForegroundColor Gray
        $version | ForEach-Object { Write-Host "      $_" -ForegroundColor Gray }
    } else {
        Write-Host "   ⚠️  Could not get version (may need to restart terminal)" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "   ⚠️  Could not test VS Code (may need to restart terminal)" -ForegroundColor Yellow
    Write-Host "      Error: $_" -ForegroundColor Gray
}

# Create desktop shortcut
Write-Host "`n📌 Creating desktop shortcut..." -ForegroundColor Yellow

try {
    $desktopPath = [Environment]::GetFolderPath("Desktop")
    $shortcutPath = Join-Path $desktopPath "Visual Studio Code.lnk"
    
    $WshShell = New-Object -ComObject WScript.Shell
    $Shortcut = $WshShell.CreateShortcut($shortcutPath)
    $Shortcut.TargetPath = $codeExe
    $Shortcut.WorkingDirectory = $vscodePath
    $Shortcut.Description = "Visual Studio Code"
    $Shortcut.Save()
    
    Write-Host "   ✅ Desktop shortcut created" -ForegroundColor Green
}
catch {
    Write-Host "   ⚠️  Could not create shortcut: $_" -ForegroundColor Yellow
}

# Summary
Write-Host "`n" + "=" * 60 -ForegroundColor Cyan
Write-Host "  ✅ Configuration Complete!" -ForegroundColor Green
Write-Host "=" * 60 -ForegroundColor Cyan

Write-Host "`n📋 VS Code Details:" -ForegroundColor Yellow
Write-Host "   Installation Path: $vscodePath" -ForegroundColor White
Write-Host "   Executable: $codeExe" -ForegroundColor White
if (Test-Path $codeCmd) {
    Write-Host "   Command Line: $codeCmd" -ForegroundColor White
}

Write-Host "`n💡 Usage:" -ForegroundColor Yellow
Write-Host "   • Open VS Code: Double-click Code.exe or use desktop shortcut" -ForegroundColor Gray
Write-Host "   • Command line: code . (after restarting terminal)" -ForegroundColor Gray
Write-Host "   • Open folder: code 'E:\Everything\~dev\VSCode'" -ForegroundColor Gray

Write-Host "`n⚠️  Next Steps:" -ForegroundColor Yellow
Write-Host "   1. Restart your terminal/PowerShell for PATH changes" -ForegroundColor Gray
Write-Host "   2. Test with: code --version" -ForegroundColor Gray
Write-Host "   3. Open this project: code ." -ForegroundColor Gray

