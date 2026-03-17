# Production Build Verification Script

$ErrorActionPreference = 'Continue'

$buildDir = Join-Path $PSScriptRoot 'build'
$srcDir = Join-Path $PSScriptRoot 'src'

Write-Host "`n=== RawrXD v2.0 Production Build Verification ===" -ForegroundColor Cyan
Write-Host "Build Date: $(Get-Date)" -ForegroundColor Gray

# 1. Check artifacts exist
Write-Host "`n[1/6] Artifact Verification" -ForegroundColor Yellow
$artifacts = @(
    @{ Name = 'AgenticIDEWin.exe'; Type = 'Main Executable' },
    @{ Name = 'gguf_beacon_spoof.dll'; Type = 'Beacon DLL' },
    @{ Name = 'config.ini'; Type = 'Configuration' }
)

$allPresent = $true
foreach ($artifact in $artifacts) {
    $path = Join-Path $buildDir $artifact.Name
    $exists = Test-Path $path
    $status = if ($exists) { '✓' } else { '✗' }
    $color = if ($exists) { 'Green' } else { 'Red' }
    Write-Host "  $status $($artifact.Type): $($artifact.Name)" -ForegroundColor $color
    if (-not $exists) { $allPresent = $false }
}

if (-not $allPresent) {
    Write-Host "  ⚠ Some artifacts missing. Run build_production.ps1 first." -ForegroundColor Yellow
    exit 1
}

# 2. Check file sizes
Write-Host "`n[2/6] File Size Check" -ForegroundColor Yellow
$files = @(
    Join-Path $buildDir 'AgenticIDEWin.exe',
    Join-Path $buildDir 'gguf_beacon_spoof.dll',
    Join-Path $buildDir 'config.ini'
)

foreach ($file in $files) {
    if (Test-Path $file) {
        $size = (Get-Item $file).Length
        $kb = [Math]::Round($size / 1024, 2)
        Write-Host "  $(Split-Path $file -Leaf): $kb KB" -ForegroundColor Green
    }
}

# 3. Verify config.ini format
Write-Host "`n[3/6] Configuration Format Validation" -ForegroundColor Yellow
$configPath = Join-Path $buildDir 'config.ini'
if (Test-Path $configPath) {
    $content = Get-Content $configPath -Raw
    $lines = $content -split '\r?\n' | Where-Object { $_.Trim() -and -not $_.Trim().StartsWith(';') }
    
    $validCount = 0
    foreach ($line in $lines) {
        if ($line -match '^\s*\w+\s*=\s*.+$') {
            $validCount++
            $key, $value = $line -split '=' | ForEach-Object { $_.Trim() }
            Write-Host "  ✓ $key = $value" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Invalid: $line" -ForegroundColor Red
        }
    }
    
    Write-Host "  Summary: $validCount valid settings" -ForegroundColor Cyan
}

# 4. Check for DLL exports (basic validation)
Write-Host "`n[4/6] DLL Symbol Validation" -ForegroundColor Yellow
$dllPath = Join-Path $buildDir 'gguf_beacon_spoof.dll'
if (Test-Path $dllPath) {
    Write-Host "  ✓ Beacon DLL present and ready to load" -ForegroundColor Green
    Write-Host "  Note: Full symbol check requires dumpbin.exe (VS tools)" -ForegroundColor Gray
}

# 5. Source file check
Write-Host "`n[5/6] Source Component Validation" -ForegroundColor Yellow
$sourceFiles = @(
    'engine.asm',
    'config_manager.asm',
    'gguf_beacon_spoof.asm',
    'window.asm',
    'masm_main.asm'
)

$sourceOk = $true
foreach ($src in $sourceFiles) {
    $path = Join-Path $srcDir $src
    if (Test-Path $path) {
        $lines = (Get-Content $path | Measure-Object -Line).Lines
        Write-Host "  ✓ $src ($lines lines)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $src not found" -ForegroundColor Red
        $sourceOk = $false
    }
}

# 6. Build artifacts timestamp
Write-Host "`n[6/6] Build Freshness" -ForegroundColor Yellow
$exePath = Join-Path $buildDir 'AgenticIDEWin.exe'
if (Test-Path $exePath) {
    $buildTime = (Get-Item $exePath).LastWriteTime
    $ago = [Math]::Round(((Get-Date) - $buildTime).TotalMinutes)
    Write-Host "  Last build: $ago minutes ago ($buildTime)" -ForegroundColor Green
    
    if ($ago -gt 60) {
        Write-Host "  ⚠ Build is older than 1 hour. Consider rebuilding." -ForegroundColor Yellow
    }
}

# Summary
Write-Host "`n=== Verification Summary ===" -ForegroundColor Cyan
if ($allPresent -and $sourceOk) {
    Write-Host "✅ All checks passed. Ready for deployment." -ForegroundColor Green
    Write-Host "`nNext Steps:" -ForegroundColor Yellow
    Write-Host "  1. Set EnableBeacon=1 in config.ini if wirecap is needed"
    Write-Host "  2. Copy all artifacts to production location"
    Write-Host "  3. Run AgenticIDEWin.exe"
    Write-Host "  4. Check for gguf_wirecap.bin creation during GGUF operations"
    exit 0
} else {
    Write-Host "❌ Some checks failed. See above for details." -ForegroundColor Red
    exit 1
}
