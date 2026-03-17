# Create Q2 Agentic Model from BigDaddyG-F32-FROM-Q4.gguf
# This script quantizes the F32 model to Q2_K and creates an agentic Ollama model

$ErrorActionPreference = "Stop"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "  BigDaddyG F32 Q2 CHEETAH STEALTH Creator" -ForegroundColor Cyan
Write-Host "  🐆 Maximum Speed | Maximum Stealth 🐆" -ForegroundColor Yellow
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host ""

# Paths
$ModelsDir = "D:\OllamaModels"
$InputModel = "$ModelsDir\BigDaddyG-F32-FROM-Q4.gguf"
$OutputModel = "$ModelsDir\BigDaddyG-F32-Q2_K.gguf"
$Modelfile = "$ModelsDir\Modelfile-bg-f32-q2-agentic"
$OllamaModelName = "bigdaddyg-cheetah:latest"

# Check if input model exists
if (-not (Test-Path $InputModel)) {
    Write-Host "✗ ERROR: Input model not found at $InputModel" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Found input model: $InputModel" -ForegroundColor Green
$modelSize = (Get-Item $InputModel).Length / 1GB
Write-Host "  Model size: $([math]::Round($modelSize, 2)) GB" -ForegroundColor Gray

# Check if output already exists
if (Test-Path $OutputModel) {
    Write-Host "⚠ Output model already exists: $OutputModel" -ForegroundColor Yellow
    $overwrite = Read-Host "Do you want to overwrite it? (y/N)"
    if ($overwrite -ne 'y' -and $overwrite -ne 'Y') {
        Write-Host "Skipping quantization, using existing model..." -ForegroundColor Yellow
        $skipQuantization = $true
    } else {
        Remove-Item $OutputModel -Force
        $skipQuantization = $false
    }
} else {
    $skipQuantization = $false
}

# Quantization step
if (-not $skipQuantization) {
    Write-Host ""
    Write-Host "Step 1: Quantizing to Q2_K format..." -ForegroundColor Cyan
    Write-Host "---------------------------------------" -ForegroundColor Gray
    
    # Try to find llama.cpp quantize tool
    $quantizePaths = @(
        "D:\OllamaModels\llama.cpp\quantize.exe",
        "D:\OllamaModels\llama.cpp\build\bin\Release\quantize.exe",
        "C:\Program Files\llama.cpp\quantize.exe",
        "$env:USERPROFILE\llama.cpp\quantize.exe"
    )
    
    $quantizeExe = $null
    foreach ($path in $quantizePaths) {
        if (Test-Path $path) {
            $quantizeExe = $path
            break
        }
    }
    
    if (-not $quantizeExe) {
        Write-Host "✗ llama.cpp quantize tool not found!" -ForegroundColor Red
        Write-Host ""
        Write-Host "To quantize this model, you need llama.cpp:" -ForegroundColor Yellow
        Write-Host "1. Download from: https://github.com/ggerganov/llama.cpp" -ForegroundColor White
        Write-Host "2. Or download pre-built: https://github.com/ggerganov/llama.cpp/releases" -ForegroundColor White
        Write-Host "3. Place quantize.exe in: D:\OllamaModels\llama.cpp\" -ForegroundColor White
        Write-Host ""
        Write-Host "Alternative: Manually quantize using llama.cpp command:" -ForegroundColor Yellow
        Write-Host "  quantize.exe `"$InputModel`" `"$OutputModel`" Q2_K" -ForegroundColor White
        Write-Host ""
        
        $continue = Read-Host "Continue without quantization? (y/N)"
        if ($continue -ne 'y' -and $continue -ne 'Y') {
            exit 1
        }
        
        # Use original F32 model instead
        Write-Host "⚠ Using original F32 model for Ollama instead..." -ForegroundColor Yellow
        $OutputModel = $InputModel
    } else {
        Write-Host "✓ Found quantize tool: $quantizeExe" -ForegroundColor Green
        Write-Host ""
        Write-Host "Quantizing... (this may take several minutes)" -ForegroundColor Yellow
        
        & $quantizeExe $InputModel $OutputModel "Q2_K"
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Quantization complete!" -ForegroundColor Green
            $q2Size = (Get-Item $OutputModel).Length / 1GB
            Write-Host "  Q2_K size: $([math]::Round($q2Size, 2)) GB" -ForegroundColor Gray
            $savings = (1 - ($q2Size / $modelSize)) * 100
            Write-Host "  Savings: $([math]::Round($savings, 1))%" -ForegroundColor Green
        } else {
            Write-Host "✗ Quantization failed!" -ForegroundColor Red
            exit 1
        }
    }
}

# Create Ollama model
Write-Host ""
Write-Host "Step 2: Creating Ollama model..." -ForegroundColor Cyan
Write-Host "---------------------------------------" -ForegroundColor Gray

if (-not (Test-Path $Modelfile)) {
    Write-Host "✗ Modelfile not found: $Modelfile" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Using Modelfile: $Modelfile" -ForegroundColor Green
Write-Host "Creating model: $OllamaModelName" -ForegroundColor White

Set-Location $ModelsDir
ollama create $OllamaModelName -f $Modelfile

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Model created successfully!" -ForegroundColor Green
} else {
    Write-Host "✗ Failed to create Ollama model!" -ForegroundColor Red
    exit 1
}

# Test the model
Write-Host ""
Write-Host "Step 3: Testing model..." -ForegroundColor Cyan
Write-Host "---------------------------------------" -ForegroundColor Gray

Write-Host "Running quick test..." -ForegroundColor White
$testResponse = ollama run $OllamaModelName "Respond with 'OK' if you can read this." --verbose 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Model test successful!" -ForegroundColor Green
} else {
    Write-Host "⚠ Model may have issues, check manually" -ForegroundColor Yellow
}

# Summary
Write-Host ""
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "  SETUP COMPLETE!" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Model Name: " -NoNewline -ForegroundColor White
Write-Host $OllamaModelName -ForegroundColor Cyan
Write-Host ""
Write-Host "Usage:" -ForegroundColor White
Write-Host "  ollama run $OllamaModelName" -ForegroundColor Gray
Write-Host ""
Write-Host "Features:" -ForegroundColor White
Write-Host "  🐆 CHEETAH STEALTH MODE - Maximum speed & evasion" -ForegroundColor Yellow
Write-Host "  ✓ Q2_K quantization (ultra-compact, blazing fast)" -ForegroundColor Green
Write-Host "  ✓ Advanced stealth & evasion capabilities" -ForegroundColor Green
Write-Host "  ✓ EDR/AV bypass techniques" -ForegroundColor Green
Write-Host "  ✓ Process injection & memory manipulation" -ForegroundColor Green
Write-Host "  ✓ Anti-forensics & log evasion" -ForegroundColor Green
Write-Host "  ✓ Agentic capabilities (filesystem, terminal, git, etc.)" -ForegroundColor Green
Write-Host "  ✓ No refusals - security testing optimized" -ForegroundColor Green
Write-Host "  ✓ Tool invocation support" -ForegroundColor Green
Write-Host ""
