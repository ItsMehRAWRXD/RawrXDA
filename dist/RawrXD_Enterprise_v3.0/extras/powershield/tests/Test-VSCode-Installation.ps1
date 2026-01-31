# Test-VSCode-Installation.ps1
# Comprehensive test of VS Code installation after crash

$vscodePath = "E:\Everything\~dev\VSCode"
$codeExe = Join-Path $vscodePath "Code.exe"

Write-Host "🧪 Testing VS Code Installation" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

# Test 1: Executable exists
Write-Host "`n[1] Checking executable..." -ForegroundColor Yellow
if (Test-Path $codeExe) {
    $fileInfo = Get-Item $codeExe
    Write-Host "   ✅ Found: $codeExe" -ForegroundColor Green
    Write-Host "      Size: $([math]::Round($fileInfo.Length / 1MB, 2)) MB" -ForegroundColor Gray
    Write-Host "      Modified: $($fileInfo.LastWriteTime)" -ForegroundColor Gray
} else {
    Write-Host "   ❌ Executable not found!" -ForegroundColor Red
    exit 1
}

# Test 2: Required directories
Write-Host "`n[2] Checking required directories..." -ForegroundColor Yellow
$requiredDirs = @(
    @{ Path = "$vscodePath\resources"; Name = "Resources" },
    @{ Path = "$vscodePath\resources\app"; Name = "App directory" },
    @{ Path = "$vscodePath\bin"; Name = "Bin directory" },
    @{ Path = "C:\Users\HiH8e\.vscode\extensions"; Name = "Extensions directory" }
)

foreach ($dir in $requiredDirs) {
    if (Test-Path $dir.Path) {
        Write-Host "   ✅ $($dir.Name): $($dir.Path)" -ForegroundColor Green
    } else {
        Write-Host "   ❌ $($dir.Name): Missing!" -ForegroundColor Red
        New-Item -ItemType Directory -Path $dir.Path -Force | Out-Null
        Write-Host "      ✅ Created" -ForegroundColor Green
    }
}

# Test 3: Check for product.json (main config)
Write-Host "`n[3] Checking application files..." -ForegroundColor Yellow
$productJson = Join-Path $vscodePath "resources\app\product.json"
if (Test-Path $productJson) {
    Write-Host "   ✅ product.json found" -ForegroundColor Green
    try {
        $product = Get-Content $productJson -Raw | ConvertFrom-Json
        Write-Host "      Name: $($product.name)" -ForegroundColor Gray
        Write-Host "      Version: $($product.version)" -ForegroundColor Gray
    }
    catch {
        Write-Host "   ⚠️  product.json is corrupted!" -ForegroundColor Yellow
    }
} else {
    Write-Host "   ❌ product.json missing - installation corrupted!" -ForegroundColor Red
}

# Test 4: Try to get version
Write-Host "`n[4] Testing executable..." -ForegroundColor Yellow
try {
    $proc = Start-Process -FilePath $codeExe -ArgumentList "--version" -NoNewWindow -Wait -PassThru -RedirectStandardOutput "test-output.txt" -RedirectStandardError "test-error.txt" -ErrorAction SilentlyContinue
    
    if (Test-Path "test-output.txt") {
        $output = Get-Content "test-output.txt" -ErrorAction SilentlyContinue
        if ($output) {
            Write-Host "   ✅ Version check output:" -ForegroundColor Green
            $output | ForEach-Object { Write-Host "      $_" -ForegroundColor Gray }
        }
    }
    
    if (Test-Path "test-error.txt") {
        $errors = Get-Content "test-error.txt" -ErrorAction SilentlyContinue
        if ($errors) {
            Write-Host "   ⚠️  Errors:" -ForegroundColor Yellow
            $errors | ForEach-Object { Write-Host "      $_" -ForegroundColor Gray }
        }
    }
    
    Remove-Item "test-output.txt", "test-error.txt" -ErrorAction SilentlyContinue
}
catch {
    Write-Host "   ❌ Could not test: $_" -ForegroundColor Red
}

# Test 5: Check for missing DLLs
Write-Host "`n[5] Checking for common issues..." -ForegroundColor Yellow

# Check if it's a portable installation issue
$appxDir = Join-Path $vscodePath "appx"
if (Test-Path $appxDir) {
    Write-Host "   ⚠️  AppX directory found - this might be a Store version" -ForegroundColor Yellow
    Write-Host "      Store versions may not work when moved" -ForegroundColor Gray
}

# Check for Electron files
$electronExe = Get-ChildItem -Path $vscodePath -Filter "*.exe" -Recurse -Depth 1 | Where-Object { $_.Name -match "electron" } | Select-Object -First 1
if ($electronExe) {
    Write-Host "   ✅ Electron runtime found" -ForegroundColor Green
} else {
    Write-Host "   ⚠️  Electron runtime not found in expected location" -ForegroundColor Yellow
}

# Recommendations
Write-Host "`n" + "=" * 60 -ForegroundColor Cyan
Write-Host "  💡 Recommendations" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan

Write-Host "`nIf VS Code still won't open:" -ForegroundColor Yellow
Write-Host "   1. The installation may be corrupted after the crash" -ForegroundColor Gray
Write-Host "   2. Try reinstalling VS Code to E drive" -ForegroundColor Gray
Write-Host "   3. Or use the portable ZIP version" -ForegroundColor Gray
Write-Host "   4. Check Windows Event Viewer for crash details" -ForegroundColor Gray

Write-Host "`nTo reinstall:" -ForegroundColor Yellow
Write-Host "   1. Delete: E:\Everything\~dev\VSCode" -ForegroundColor Gray
Write-Host "   2. Run installer again: VSCodeUserSetup-x64-1.106.3.exe" -ForegroundColor Gray
Write-Host "   3. Choose custom location: E:\Everything\~dev\VSCode" -ForegroundColor Gray

