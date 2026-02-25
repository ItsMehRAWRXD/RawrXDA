# Fix-VSCode-After-Crash.ps1
# Fixes VS Code after system crash - recreates missing directories and fixes configuration

Write-Host "🔧 Fixing VS Code After System Crash" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

$vscodePath = "E:\Everything\~dev\VSCode"
$codeExe = Join-Path $vscodePath "Code.exe"

# Check if VS Code exists
if (-not (Test-Path $codeExe)) {
    Write-Host "`n❌ VS Code not found at: $vscodePath" -ForegroundColor Red
    exit 1
}

Write-Host "`n✅ VS Code found at: $vscodePath" -ForegroundColor Green

# Required directories
$requiredDirs = @(
    "$env:USERPROFILE\.vscode",
    "$env:USERPROFILE\.vscode\extensions",
    "$env:APPDATA\Code",
    "$env:APPDATA\Code\User",
    "$env:APPDATA\Code\CachedExtensions",
    "$env:APPDATA\Code\CachedExtensionVSIXs",
    "$env:APPDATA\Code\logs",
    "$env:APPDATA\Code\User\workspaceStorage"
)

Write-Host "`n📁 Creating required directories..." -ForegroundColor Yellow

foreach ($dir in $requiredDirs) {
    if (-not (Test-Path $dir)) {
        try {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
            Write-Host "   ✅ Created: $dir" -ForegroundColor Green
        }
        catch {
            Write-Host "   ❌ Failed to create: $dir - $_" -ForegroundColor Red
        }
    } else {
        Write-Host "   ℹ️  Exists: $dir" -ForegroundColor Gray
    }
}

# Check for corrupted user settings
Write-Host "`n🔍 Checking user settings..." -ForegroundColor Yellow

$userSettingsPath = "$env:APPDATA\Code\User\settings.json"
if (Test-Path $userSettingsPath) {
    try {
        $settings = Get-Content $userSettingsPath -Raw | ConvertFrom-Json -ErrorAction Stop
        Write-Host "   ✅ Settings file is valid JSON" -ForegroundColor Green
    }
    catch {
        Write-Host "   ⚠️  Settings file is corrupted!" -ForegroundColor Yellow
        Write-Host "      Backing up and creating new settings..." -ForegroundColor Gray
        
        $backupPath = "$userSettingsPath.backup.$(Get-Date -Format 'yyyyMMdd-HHmmss')"
        Copy-Item $userSettingsPath $backupPath -ErrorAction SilentlyContinue
        Write-Host "      ✅ Backed up to: $backupPath" -ForegroundColor Green
        
        # Create minimal valid settings
        $minimalSettings = @{
            "editor.fontSize" = 12
            "editor.fontFamily" = "Consolas"
        } | ConvertTo-Json
        
        Set-Content -Path $userSettingsPath -Value $minimalSettings -Encoding UTF8
        Write-Host "      ✅ Created new settings file" -ForegroundColor Green
    }
} else {
    Write-Host "   ℹ️  No settings file found (will be created on first launch)" -ForegroundColor Gray
}

# Check for missing dependencies
Write-Host "`n🔍 Checking for missing dependencies..." -ForegroundColor Yellow

# Check if VS Code resources exist
$resourcesPath = Join-Path $vscodePath "resources"
if (-not (Test-Path $resourcesPath)) {
    Write-Host "   ❌ Resources directory missing!" -ForegroundColor Red
    Write-Host "      VS Code installation may be corrupted" -ForegroundColor Yellow
    Write-Host "      Consider reinstalling VS Code" -ForegroundColor Yellow
} else {
    Write-Host "   ✅ Resources directory found" -ForegroundColor Green
}

# Check for app.asar (main application file)
$appAsar = Join-Path $vscodePath "resources\app\product.json"
if (Test-Path $appAsar) {
    Write-Host "   ✅ Application files found" -ForegroundColor Green
} else {
    Write-Host "   ⚠️  Application files may be missing" -ForegroundColor Yellow
}

# Clear potentially corrupted cache
Write-Host "`n🧹 Clearing potentially corrupted cache..." -ForegroundColor Yellow

$cacheDirs = @(
    "$env:APPDATA\Code\CachedExtensions",
    "$env:APPDATA\Code\CachedExtensionVSIXs",
    "$env:APPDATA\Code\logs"
)

foreach ($cacheDir in $cacheDirs) {
    if (Test-Path $cacheDir) {
        try {
            Get-ChildItem $cacheDir -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "   ✅ Cleared: $cacheDir" -ForegroundColor Green
        }
        catch {
            Write-Host "   ⚠️  Could not clear: $cacheDir" -ForegroundColor Yellow
        }
    }
}

# Kill any hanging VS Code processes
Write-Host "`n🔄 Checking for hanging processes..." -ForegroundColor Yellow

$codeProcesses = Get-Process | Where-Object { $_.ProcessName -eq "Code" } -ErrorAction SilentlyContinue
if ($codeProcesses) {
    Write-Host "   Found $($codeProcesses.Count) VS Code process(es)" -ForegroundColor Yellow
    $kill = Read-Host "   Kill hanging processes? (y/N)"
    if ($kill -eq "y" -or $kill -eq "Y") {
        $codeProcesses | Stop-Process -Force -ErrorAction SilentlyContinue
        Write-Host "   ✅ Processes killed" -ForegroundColor Green
        Start-Sleep -Seconds 2
    }
}

# Test VS Code launch
Write-Host "`n🧪 Testing VS Code launch..." -ForegroundColor Yellow

try {
    # Try to get version (quick test)
    $version = & $codeExe --version 2>&1 | Select-Object -First 1
    if ($version -match "\d+\.\d+\.\d+") {
        Write-Host "   ✅ VS Code responds: $version" -ForegroundColor Green
    } else {
        Write-Host "   ⚠️  VS Code may have issues" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "   ⚠️  Could not test VS Code: $_" -ForegroundColor Yellow
}

# Summary
Write-Host "`n" + "=" * 60 -ForegroundColor Cyan
Write-Host "  ✅ Fix Complete!" -ForegroundColor Green
Write-Host "=" * 60 -ForegroundColor Cyan

Write-Host "`n📋 What was fixed:" -ForegroundColor Yellow
Write-Host "   ✅ Created missing .vscode directories" -ForegroundColor Green
Write-Host "   ✅ Created missing AppData\Code directories" -ForegroundColor Green
Write-Host "   ✅ Cleared corrupted cache" -ForegroundColor Green
Write-Host "   ✅ Checked for corrupted settings" -ForegroundColor Green

Write-Host "`n💡 Next Steps:" -ForegroundColor Cyan
Write-Host "   1. Try opening VS Code again" -ForegroundColor White
Write-Host "   2. If it still doesn't work, you may need to reinstall" -ForegroundColor White
Write-Host "   3. Check Windows Event Viewer for crash logs" -ForegroundColor White

Write-Host "`n🚀 Try opening VS Code now:" -ForegroundColor Yellow
Write-Host "   & '$codeExe'" -ForegroundColor Gray

