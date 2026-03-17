#!/usr/bin/env pwsh
<#
.SYNOPSIS
Convert a 70B model to smaller B model variants using aggressive quantization.

.DESCRIPTION
Creates multiple model variants:
- 7B equivalent (Q1_K or Q2_K ultra-compressed)
- 13B equivalent (Q2_K compressed)
- 30B equivalent (Q3_K compressed)

Uses GGML quantization tools to reduce model size while maintaining quality.
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$InputModel,
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("7B", "13B", "30B")]
    [string]$TargetSize = "7B",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("Q1_K", "Q2_K", "Q3_K", "Q4_K")]
    [string]$Quantization = "Q2_K",
    
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "D:\OllamaModels"
)

Write-Host @"
╔════════════════════════════════════════════╗
║  🔄 MODEL QUANTIZATION & CONVERSION 🔄    ║
║     70B → $TargetSize Model ($Quantization)     ║
╚════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# Validate input model
if (-not (Test-Path $InputModel)) {
    Write-Host "❌ Input model not found: $InputModel" -ForegroundColor Red
    exit 1
}

$inputInfo = Get-Item $InputModel
$inputSizeGB = [math]::Round($inputInfo.Length / 1GB, 2)
Write-Host "📦 Input Model: $(Split-Path $InputModel -Leaf)" -ForegroundColor Green
Write-Host "   Size: $inputSizeGB GB" -ForegroundColor Green

# Calculate target compression ratios
$compressionRatios = @{
    "7B"  = @{ ratio = 0.10; desc = "Ultra-compressed (10x smaller)" }
    "13B" = @{ ratio = 0.19; desc = "Highly compressed (5x smaller)" }
    "30B" = @{ ratio = 0.43; desc = "Well compressed (2.3x smaller)" }
}

$targetInfo = $compressionRatios[$TargetSize]
$estimatedSize = $inputSizeGB * $targetInfo.ratio
Write-Host "🎯 Target: $TargetSize - $($targetInfo.desc)" -ForegroundColor Yellow
Write-Host "   Estimated output: $([math]::Round($estimatedSize, 2)) GB" -ForegroundColor Yellow

# Create output filename
$outputName = "BigDaddyG-$TargetSize-$Quantization.gguf"
$outputPath = Join-Path $OutputDir $outputName

Write-Host "`n⚙️  Quantization Settings:" -ForegroundColor Cyan
Write-Host "   Method: $Quantization" -ForegroundColor Cyan
Write-Host "   Output: $outputPath" -ForegroundColor Cyan

# Check for llama.cpp or GGML tools
Write-Host "`n🔍 Looking for quantization tools..." -ForegroundColor Yellow

$quantizeTools = @(
    "C:\Program Files\llama.cpp\quantize.exe",
    "C:\llama\quantize.exe",
    "$PSScriptRoot\quantize.exe",
    "quantize.exe"
)

$quantizeTool = $null
foreach ($tool in $quantizeTools) {
    if (Test-Path $tool) {
        $quantizeTool = $tool
        Write-Host "✅ Found quantize tool: $tool" -ForegroundColor Green
        break
    }
}

if (-not $quantizeTool) {
    Write-Host ❌ Quantization tool not found. Please install llama.cpp: -ForegroundColor Red
    Write-Host "   https://github.com/ggerganov/llama.cpp/releases" -ForegroundColor Yellow
    Write-Host "`n   Or copy quantize.exe to:" -ForegroundColor Yellow
    Write-Host "   - $($quantizeTools[0])" -ForegroundColor Yellow
    Write-Host "   - $($quantizeTools[1])" -ForegroundColor Yellow
    exit 1
}

# Perform quantization
Write-Host "`n▶️  Starting quantization..." -ForegroundColor Yellow
Write-Host "   This may take 30-60 minutes depending on hardware" -ForegroundColor Yellow
$startTime = Get-Date

try {
    & $quantizeTool $InputModel $outputPath $Quantization
    
    if ($LASTEXITCODE -eq 0) {
        $endTime = Get-Date
        $duration = $endTime - $startTime
        
        $outputInfo = Get-Item $outputPath
        $outputSizeGB = [math]::Round($outputInfo.Length / 1GB, 2)
        
        Write-Host "`n✅ Quantization successful!" -ForegroundColor Green
        Write-Host "📊 Results:" -ForegroundColor Green
        Write-Host "   Original size: $inputSizeGB GB" -ForegroundColor Green
        Write-Host "   New size: $outputSizeGB GB" -ForegroundColor Green
        Write-Host "   Compression: $([math]::Round(($inputSizeGB - $outputSizeGB) / $inputSizeGB * 100))%" -ForegroundColor Green
        Write-Host "   Time taken: $([math]::Round($duration.TotalMinutes, 1)) minutes" -ForegroundColor Green
        Write-Host "   Output: $(Split-Path $outputPath -Leaf)" -ForegroundColor Green
        
        Write-Host "`n🎉 New model ready to use!" -ForegroundColor Cyan
        Write-Host "   Location: $outputPath" -ForegroundColor Cyan
        Write-Host "`n   To use with LLamaSharp:" -ForegroundColor Cyan
        Write-Host "   dotnet run -- --model=`"$outputPath`" --gpu-layers=32" -ForegroundColor Cyan
    }
    else {
        Write-Host "❌ Quantization failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
}
catch {
    Write-Host "❌ Error during quantization: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`n📝 Next steps:" -ForegroundColor Cyan
Write-Host "   1. Update Program.cs to use: $outputPath" -ForegroundColor Cyan
Write-Host "   2. Adjust gpu-layers based on VRAM (typically 32-40 for 7B on RTX 3090)" -ForegroundColor Cyan
Write-Host "   3. Test inference speed and quality" -ForegroundColor Cyan
