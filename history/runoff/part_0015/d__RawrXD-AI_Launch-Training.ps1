<# 
.SYNOPSIS
    RawrXD-AI Quick Launch - Sets up environment and runs the pipeline
.DESCRIPTION
    One-click script to install dependencies and run the full training pipeline.
    Creates all 4 GGUF variants: Q2_K, Q3_K_M, Q4_K_M, Q5_K_M
    Output: D:\OllamaModels\RawrXD-AI\
#>

$ErrorActionPreference = "Continue"
$PipelineDir = "D:\RawrXD-AI"

Write-Host "=" * 72 -ForegroundColor Cyan
Write-Host "  RawrXD-AI Training Pipeline Launcher" -ForegroundColor Cyan
Write-Host "  Building 4 custom GGUF models from your D: drive" -ForegroundColor Cyan  
Write-Host "=" * 72 -ForegroundColor Cyan

# Check Python
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Write-Host "  ERROR: Python not found. Install Python 3.10+ first." -ForegroundColor Red
    exit 1
}
Write-Host "  Python: $($python.Source)" -ForegroundColor Green

# Phase 1 & 2 don't need ML packages, run them first
Write-Host "`n  PHASE 1: Extracting corpus from D: drive..." -ForegroundColor Yellow
python "$PipelineDir\01_extract_corpus.py"
if ($LASTEXITCODE -ne 0) { Write-Host "  Phase 1 failed!" -ForegroundColor Red; exit 1 }

Write-Host "`n  PHASE 2: Formatting training instructions..." -ForegroundColor Yellow
python "$PipelineDir\02_format_instructions.py"
if ($LASTEXITCODE -ne 0) { Write-Host "  Phase 2 failed!" -ForegroundColor Red; exit 1 }

# Install ML dependencies
Write-Host "`n  Installing ML dependencies..." -ForegroundColor Yellow
Write-Host "  (PyTorch ROCm for AMD GPU, transformers, peft, etc.)"

# AMD ROCm PyTorch
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/rocm6.0 2>&1 | Select-Object -Last 5

# Rest of deps
pip install -r "$PipelineDir\requirements.txt" 2>&1 | Select-Object -Last 5

# Phase 3: Training (longest step)
Write-Host "`n  PHASE 3: QLoRA fine-tuning (this takes 48-72 hours)..." -ForegroundColor Yellow
Write-Host "  You can safely close this window - training saves checkpoints." -ForegroundColor Gray
python "$PipelineDir\03_qlora_finetune.py" --merge
if ($LASTEXITCODE -ne 0) { Write-Host "  Phase 3 failed!" -ForegroundColor Red; exit 1 }

# Phase 4: Quantize all 4 variants
Write-Host "`n  PHASE 4: Creating all 4 GGUF quantizations..." -ForegroundColor Yellow
python "$PipelineDir\04_merge_and_quantize.py"
if ($LASTEXITCODE -ne 0) { Write-Host "  Phase 4 failed!" -ForegroundColor Red; exit 1 }

Write-Host "`n" 
Write-Host "=" * 72 -ForegroundColor Green
Write-Host "  ALL 4 MODELS READY!" -ForegroundColor Green
Write-Host "  Location: D:\OllamaModels\RawrXD-AI\" -ForegroundColor Green
Write-Host ""
Write-Host "  Models:" -ForegroundColor White
Write-Host "    RawrXD-IDE-33B-Q2_K.gguf   (~12 GB) - Daily driver, full VRAM" -ForegroundColor White
Write-Host "    RawrXD-IDE-33B-Q3_K_M.gguf (~15 GB) - Better accuracy" -ForegroundColor White
Write-Host "    RawrXD-IDE-33B-Q4_K_M.gguf (~19 GB) - Near-original quality" -ForegroundColor White
Write-Host "    RawrXD-IDE-33B-Q5_K_M.gguf (~23 GB) - Maximum quality" -ForegroundColor White
Write-Host ""
Write-Host "  Quick test:" -ForegroundColor Yellow
Write-Host "    D:\OllamaModels\llama.cpp\llama-server.exe ``" -ForegroundColor Gray
Write-Host "      -m D:\OllamaModels\RawrXD-AI\RawrXD-IDE-33B-Q2_K.gguf ``" -ForegroundColor Gray
Write-Host "      -ngl 99 -c 8192 --port 8080" -ForegroundColor Gray
Write-Host "=" * 72 -ForegroundColor Green
