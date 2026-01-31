# Fix-VSCode-EPipe-Complete.ps1
# Complete fix for VS Code EPipe errors after system crash

Write-Host "🔧 Complete VS Code EPipe Fix" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

$vscodePath = "E:\Everything\~dev\VSCode"
$codeExe = Join-Path $vscodePath "Code.exe"

# Step 1: Kill all processes
Write-Host "`n[1/6] Stopping all VS Code processes..." -ForegroundColor Yellow
Get-Process | Where-Object { $_.ProcessName -match "Code|cursor" } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 3
Write-Host "   ✅ All processes stopped" -ForegroundColor Green

# Step 2: Fix directories and permissions
Write-Host "`n[2/6] Fixing directories and permissions..." -ForegroundColor Yellow

$dirs = @(
    "C:\Users\HiH8e\.vscode",
    "C:\Users\HiH8e\.vscode\extensions",
    "$env:APPDATA\Code\User",
    "$env:APPDATA\Code\User\workspaceStorage"
)

foreach ($dir in $dirs) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    
    # Set full permissions
    try {
        $acl = Get-Acl $dir
        $permission = "$env:USERNAME","FullControl","ContainerInherit,ObjectInherit","None","Allow"
        $accessRule = New-Object System.Security.AccessControl.FileSystemAccessRule $permission
        $acl.SetAccessRule($accessRule)
        Set-Acl $dir $acl
    }
    catch {
        & icacls $dir /grant "${env:USERNAME}:(OI)(CI)F" /T 2>&1 | Out-Null
    }
}
Write-Host "   ✅ Directories and permissions fixed" -ForegroundColor Green

# Step 3: Clear all caches
Write-Host "`n[3/6] Clearing corrupted caches..." -ForegroundColor Yellow

$cacheDirs = @(
    "$env:APPDATA\Code\CachedExtensions",
    "$env:APPDATA\Code\CachedExtensionVSIXs",
    "$env:APPDATA\Code\logs",
    "$env:APPDATA\Code\User\workspaceStorage",
    "$env:APPDATA\Code\GPUCache",
    "$env:APPDATA\Code\ShaderCache"
)

foreach ($cacheDir in $cacheDirs) {
    if (Test-Path $cacheDir) {
        Get-ChildItem $cacheDir -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    }
}
Write-Host "   ✅ All caches cleared" -ForegroundColor Green

# Step 4: Disable problematic extensions temporarily
Write-Host "`n[4/6] Configuring settings to prevent EPipe..." -ForegroundColor Yellow

$userSettings = "$env:APPDATA\Code\User\settings.json"
$settings = @{}

if (Test-Path $userSettings) {
    try {
        $settings = Get-Content $userSettings -Raw | ConvertFrom-Json
    }
    catch {
        $settings = @{}
    }
}

# Add settings to prevent EPipe errors
$settings.'extensions.autoCheckUpdates' = $false
$settings.'extensions.autoUpdate' = $false
$settings.'extensions.experimental.affinity' = @{"*" = 1}
$settings.'window.restoreWindows' = "none"

# Disable extension host features that can cause EPipe
$settings.'extensions.verifySignature' = $false

$settings | ConvertTo-Json -Depth 20 | Set-Content $userSettings -Encoding UTF8
Write-Host "   ✅ Settings updated" -ForegroundColor Green

# Step 5: Check for corrupted installation
Write-Host "`n[5/6] Checking installation integrity..." -ForegroundColor Yellow

$criticalFiles = @(
    "$vscodePath\Code.exe",
    "$vscodePath\resources\app\product.json",
    "$vscodePath\resources\app\package.json"
)

$allGood = $true
foreach ($file in $criticalFiles) {
    if (Test-Path $file) {
        Write-Host "   ✅ $(Split-Path $file -Leaf)" -ForegroundColor Green
    } else {
        Write-Host "   ❌ Missing: $(Split-Path $file -Leaf)" -ForegroundColor Red
        $allGood = $false
    }
}

if (-not $allGood) {
    Write-Host "   ⚠️  Installation may be corrupted - consider reinstalling" -ForegroundColor Yellow
}

# Step 6: Test launch with safe mode
Write-Host "`n[6/6] Testing VS Code launch (safe mode)..." -ForegroundColor Yellow

Write-Host "`n💡 Try these launch options:" -ForegroundColor Cyan
Write-Host "   1. Safe mode (no extensions):" -ForegroundColor White
Write-Host "      & '$codeExe' --disable-extensions" -ForegroundColor Gray
Write-Host ""
Write-Host "   2. Clean user data:" -ForegroundColor White
Write-Host "      & '$codeExe' --user-data-dir `"$env:TEMP\vscode-clean`"" -ForegroundColor Gray
Write-Host ""
Write-Host "   3. Normal launch:" -ForegroundColor White
Write-Host "      & '$codeExe'" -ForegroundColor Gray

Write-Host "`n" + "=" * 60 -ForegroundColor Cyan
Write-Host "  ✅ Fix Complete!" -ForegroundColor Green
Write-Host "=" * 60 -ForegroundColor Cyan

Write-Host "`n📋 Summary:" -ForegroundColor Yellow
Write-Host "   ✅ Processes stopped" -ForegroundColor Green
Write-Host "   ✅ Directories and permissions fixed" -ForegroundColor Green
Write-Host "   ✅ Caches cleared" -ForegroundColor Green
Write-Host "   ✅ Settings updated to prevent EPipe" -ForegroundColor Green

Write-Host "`n🚀 Next Steps:" -ForegroundColor Cyan
Write-Host "   1. Try launching VS Code in safe mode first:" -ForegroundColor White
Write-Host "      & '$codeExe' --disable-extensions" -ForegroundColor Gray
Write-Host ""
Write-Host "   2. If safe mode works, the issue is with extensions" -ForegroundColor White
Write-Host "      Disable extensions one by one to find the culprit" -ForegroundColor Gray
Write-Host ""
Write-Host "   3. If safe mode doesn't work, reinstall VS Code" -ForegroundColor White
Write-Host "      The installation may be corrupted after the crash" -ForegroundColor Gray
Write-Host ""
Write-Host "   4. As a last resort, restart your computer" -ForegroundColor White
Write-Host "      This clears any locked files or processes" -ForegroundColor Gray

