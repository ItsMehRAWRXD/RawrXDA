#!/usr/bin/env pwsh
<#
.SYNOPSIS
Easy setup script for model quantization.
Downloads quantize tool if needed and converts model.
#>

Write-Host @"
╔════════════════════════════════════════════╗
║  🛠️  MODEL CONVERSION SETUP WIZARD 🛠️      ║
║    Convert 70B → Smaller B Model          ║
╚════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# Check if quantize.exe exists
$quantizeExe = "C:\llama\quantize.exe"
if (-not (Test-Path $quantizeExe)) {
    Write-Host "`n📥 Downloading llama.cpp quantize tool..." -ForegroundColor Yellow
    Write-Host "   (This is a one-time download, ~50MB)" -ForegroundColor Yellow
    
    $downloadUrl = "https://github.com/ggerganov/llama.cpp/releases/download/b3055/llama-b3055-bin-win-avx2.zip"
    $zipPath = "$env:TEMP\llama-quantize.zip"
    $extractPath = "C:\llama"
    
    try {
        Write-Host "⏳ Downloading..." -ForegroundColor Yellow
        # Use System.Net.ServicePointManager for better compatibility
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $downloadUrl -OutFile $zipPath -TimeoutSec 300
        
        Write-Host "📦 Extracting..." -ForegroundColor Yellow
        Expand-Archive -Path $zipPath -DestinationPath $extractPath -Force
        
        Write-Host "✅ Downloaded and extracted to $extractPath" -ForegroundColor Green
        Remove-Item $zipPath -Force
    }
    catch {
        Write-Host "⚠️  Could not auto-download. Please manually:" -ForegroundColor Yellow
        Write-Host "   1. Download from: https://github.com/ggerganov/llama.cpp/releases" -ForegroundColor Yellow
        Write-Host "   2. Extract quantize.exe to: C:\llama\" -ForegroundColor Yellow
        Write-Host "   3. Run this script again" -ForegroundColor Yellow
        exit 1
    }
}

# Menu for model size selection
Write-Host "`n📊 Select target model size:" -ForegroundColor Cyan
Write-Host "   1. 7B  (Ultra-compressed, ~2.4GB, fast)" -ForegroundColor White
Write-Host "   2. 13B (Highly compressed, ~4.8GB, balanced)" -ForegroundColor White
Write-Host "   3. 30B (Well compressed, ~11GB, better quality)" -ForegroundColor White
$choice = Read-Host "Enter choice (1-3)"

$targetSize = @{ "1" = "7B"; "2" = "13B"; "3" = "30B" }[$choice]
if (-not $targetSize) {
    Write-Host "❌ Invalid choice" -ForegroundColor Red
    exit 1
}

# Menu for quantization method
Write-Host "`n🎯 Select quantization method:" -ForegroundColor Cyan
Write-Host "   1. Q1_K (Ultra-aggressive, smallest, quality loss)" -ForegroundColor White
Write-Host "   2. Q2_K (Aggressive, good balance, recommended)" -ForegroundColor White
Write-Host "   3. Q3_K (Moderate, better quality)" -ForegroundColor White
Write-Host "   4. Q4_K (Minimal, best quality, larger)" -ForegroundColor White
$qChoice = Read-Host "Enter choice (1-4)"

$quantization = @{ "1" = "Q1_K"; "2" = "Q2_K"; "3" = "Q3_K"; "4" = "Q4_K" }[$qChoice]
if (-not $quantization) {
    Write-Host "❌ Invalid choice" -ForegroundColor Red
    exit 1
}

Write-Host "`n⚙️  Configuration:" -ForegroundColor Green
Write-Host "   Target Size: $targetSize" -ForegroundColor Green
Write-Host "   Quantization: $quantization" -ForegroundColor Green
Write-Host "   Input: D:\OllamaModels\BigDaddyG-Q2_K-CHEETAH.gguf" -ForegroundColor Green

$confirm = Read-Host "`n▶️  Start conversion? (y/n)"
if ($confirm -ne "y") {
    Write-Host "❌ Cancelled" -ForegroundColor Red
    exit 0
}

# Run the conversion
$scriptDir = Split-Path $PSCommandPath
$convertScript = Join-Path $scriptDir "Convert-To-B-Model.ps1"

& $convertScript `
    -InputModel "D:\OllamaModels\BigDaddyG-Q2_K-CHEETAH.gguf" `
    -TargetSize $targetSize `
    -Quantization $quantization

Write-Host "`n✨ Done! Your new model is ready to use." -ForegroundColor Green
