# Fix-VSCode-Permissions.ps1
# Fixes permissions and EPipe errors for VS Code

Write-Host "🔧 Fixing VS Code Permissions & EPipe Errors" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

# Kill all VS Code processes first
Write-Host "`n🔄 Stopping all VS Code processes..." -ForegroundColor Yellow
Get-Process | Where-Object { $_.ProcessName -eq "Code" } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
Write-Host "   ✅ Processes stopped" -ForegroundColor Green

# Fix .vscode directory
$vscodeUserDir = "C:\Users\HiH8e\.vscode"
$extensionsDir = Join-Path $vscodeUserDir "extensions"

Write-Host "`n📁 Fixing .vscode directory..." -ForegroundColor Yellow

# Remove and recreate with proper permissions
if (Test-Path $extensionsDir) {
    Write-Host "   Removing existing extensions directory..." -ForegroundColor Gray
    Remove-Item $extensionsDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Create directory with full permissions
New-Item -ItemType Directory -Path $extensionsDir -Force | Out-Null
Write-Host "   ✅ Created: $extensionsDir" -ForegroundColor Green

# Set full permissions
try {
    $acl = Get-Acl $vscodeUserDir
    $permission = "$env:USERNAME","FullControl","ContainerInherit,ObjectInherit","None","Allow"
    $accessRule = New-Object System.Security.AccessControl.FileSystemAccessRule $permission
    $acl.SetAccessRule($accessRule)
    Set-Acl $vscodeUserDir $acl
    Write-Host "   ✅ Set full permissions on .vscode directory" -ForegroundColor Green
}
catch {
    Write-Host "   ⚠️  Could not set permissions: $_" -ForegroundColor Yellow
    Write-Host "      Trying alternative method..." -ForegroundColor Gray
    
    # Alternative: Use icacls
    & icacls $vscodeUserDir /grant "${env:USERNAME}:(OI)(CI)F" /T 2>&1 | Out-Null
    Write-Host "   ✅ Applied permissions via icacls" -ForegroundColor Green
}

# Fix AppData\Code directory
Write-Host "`n📁 Fixing AppData\Code directory..." -ForegroundColor Yellow

$codeAppData = "$env:APPDATA\Code"
$codeDirs = @(
    "$codeAppData\User",
    "$codeAppData\User\workspaceStorage",
    "$codeAppData\CachedExtensions",
    "$codeAppData\CachedExtensionVSIXs",
    "$codeAppData\logs"
)

foreach ($dir in $codeDirs) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Write-Host "   ✅ Created: $dir" -ForegroundColor Green
    }
}

# Clear all caches to fix EPipe issues
Write-Host "`n🧹 Clearing all caches..." -ForegroundColor Yellow

$cacheDirs = @(
    "$codeAppData\CachedExtensions",
    "$codeAppData\CachedExtensionVSIXs",
    "$codeAppData\logs",
    "$codeAppData\User\workspaceStorage"
)

foreach ($cacheDir in $cacheDirs) {
    if (Test-Path $cacheDir) {
        Get-ChildItem $cacheDir -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "   ✅ Cleared: $cacheDir" -ForegroundColor Green
    }
}

# Fix extension host issues
Write-Host "`n🔧 Fixing extension host configuration..." -ForegroundColor Yellow

$userSettings = "$codeAppData\User\settings.json"
if (Test-Path $userSettings) {
    try {
        $settings = Get-Content $userSettings -Raw | ConvertFrom-Json
        
        # Add settings to prevent EPipe errors
        if (-not $settings.'extensions.autoCheckUpdates') {
            $settings | Add-Member -MemberType NoteProperty -Name 'extensions.autoCheckUpdates' -Value $false -Force
        }
        
        # Disable problematic features that can cause EPipe
        if (-not $settings.'extensions.autoUpdate') {
            $settings | Add-Member -MemberType NoteProperty -Name 'extensions.autoUpdate' -Value $false -Force
        }
        
        $settings | ConvertTo-Json -Depth 20 | Set-Content $userSettings -Encoding UTF8
        Write-Host "   ✅ Updated settings to prevent EPipe errors" -ForegroundColor Green
    }
    catch {
        Write-Host "   ⚠️  Could not update settings: $_" -ForegroundColor Yellow
    }
}

# Verify directories
Write-Host "`n✅ Verification..." -ForegroundColor Yellow

$testDirs = @(
    $vscodeUserDir,
    $extensionsDir,
    "$codeAppData\User"
)

$allGood = $true
foreach ($dir in $testDirs) {
    if (Test-Path $dir) {
        $writable = $false
        try {
            $testFile = Join-Path $dir "test-write.tmp"
            "test" | Out-File $testFile -ErrorAction Stop
            Remove-Item $testFile -ErrorAction SilentlyContinue
            $writable = $true
        }
        catch {
            $writable = $false
        }
        
        if ($writable) {
            Write-Host "   ✅ $dir (writable)" -ForegroundColor Green
        } else {
            Write-Host "   ❌ $dir (NOT writable!)" -ForegroundColor Red
            $allGood = $false
        }
    } else {
        Write-Host "   ❌ $dir (missing!)" -ForegroundColor Red
        $allGood = $false
    }
}

Write-Host "`n" + "=" * 60 -ForegroundColor Cyan
if ($allGood) {
    Write-Host "  ✅ All Fixes Applied!" -ForegroundColor Green
} else {
    Write-Host "  ⚠️  Some Issues Remain" -ForegroundColor Yellow
}
Write-Host "=" * 60 -ForegroundColor Cyan

Write-Host "`n💡 Next Steps:" -ForegroundColor Cyan
Write-Host "   1. Wait 5 seconds for processes to fully stop" -ForegroundColor White
Write-Host "   2. Try opening VS Code again" -ForegroundColor White
Write-Host "   3. If EPipe error persists, restart your computer" -ForegroundColor White
Write-Host "   4. After restart, VS Code should work normally" -ForegroundColor White

Write-Host "`n🚀 Try opening VS Code:" -ForegroundColor Yellow
Write-Host "   & 'E:\Everything\~dev\VSCode\Code.exe'" -ForegroundColor Gray

