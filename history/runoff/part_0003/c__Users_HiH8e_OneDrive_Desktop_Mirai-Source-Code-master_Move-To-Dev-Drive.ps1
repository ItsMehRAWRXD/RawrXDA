# MOVE DESKTOP MIRAI TO D:\~DEV
# This script moves the Mirai-Source-Code-master folder from Desktop to D:\~dev\

$ErrorActionPreference = "Stop"

Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "    MOVING MIRAI PROJECT TO D:\~DEV\" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$source = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master"
$destParent = "D:\~dev\01-Active-Projects"
$destFinal = Join-Path $destParent "Mirai-Bot-Framework"

# Verify source exists
if (-not (Test-Path $source)) {
    Write-Host "❌ Source not found: $source" -ForegroundColor Red
    exit 1
}

Write-Host "Source: $source" -ForegroundColor Yellow
Write-Host "Destination: $destFinal" -ForegroundColor Yellow
Write-Host ""

# Create destination parent if needed
if (-not (Test-Path $destParent)) {
    Write-Host "📁 Creating parent folder: $destParent" -ForegroundColor Cyan
    New-Item -Path $destParent -ItemType Directory -Force | Out-Null
}

# Check if destination already exists
if (Test-Path $destFinal) {
    Write-Host "⚠️  Destination already exists: $destFinal" -ForegroundColor Yellow
    $response = Read-Host "Do you want to merge/overwrite? (y/n)"
    if ($response -ne 'y') {
        Write-Host "❌ Operation cancelled" -ForegroundColor Red
        exit 0
    }
}

Write-Host ""
Write-Host "Moving Mirai project..." -ForegroundColor Cyan

try {
    # Move the folder
    Move-Item -Path $source -Destination $destFinal -Force
    
    Write-Host "✅ Successfully moved!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Mirai project is now at:" -ForegroundColor Green
    Write-Host "  $destFinal" -ForegroundColor White
    Write-Host ""
    Write-Host "You can now work from D:\~dev\01-Active-Projects\Mirai-Bot-Framework\" -ForegroundColor Cyan
    
} catch {
    Write-Host "❌ Error moving folder: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
