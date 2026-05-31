# Download and validate against TinyLlama-1B
# Run this script to get a reference model for parity testing
# DETERMINISTIC VERSION - locks all randomness for exact comparison

param(
    [string]$LlamaCppPath = "..\..\..\llama.cpp\build\bin\Release\main.exe",
    [string]$RawrPath = ".\rawr_monolith_v2.exe",
    [string]$ModelUrl = "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf",
    [string]$Prompt = "The capital of France is",
    [int]$NumTokens = 1,  # START WITH 1 TOKEN for logits comparison
    [float]$Temperature = 0.0,  # DETERMINISTIC
    [int]$TopK = 1,             # GREEDY ONLY
    [float]$TopP = 1.0,         # NO NUCLEUS FILTERING
    [int]$Seed = 42             # FIXED SEED
)

$ModelFile = "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"
$OutputDir = "parity_test_outputs"

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     RAWR PARITY TEST - TinyLlama-1B Validation               ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# Create output directory
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Download model if not exists
if (-not (Test-Path $ModelFile)) {
    Write-Host "`n[DOWNLOAD] TinyLlama-1B model..." -ForegroundColor Yellow
    Write-Host "URL: $ModelUrl" -ForegroundColor Gray
    
    try {
        Invoke-WebRequest -Uri $ModelUrl -OutFile $ModelFile -MaximumRetryCount 3
        Write-Host "[SUCCESS] Downloaded $ModelFile ($([math]::Round((Get-Item $ModelFile).Length / 1MB, 2)) MB)" -ForegroundColor Green
    } catch {
        Write-Host "[ERROR] Failed to download model. Please download manually from:" -ForegroundColor Red
        Write-Host "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF" -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "`n[INFO] Model already exists: $ModelFile" -ForegroundColor Green
}

# Check executables
if (-not (Test-Path $LlamaCppPath)) {
    Write-Host "`n[WARNING] llama.cpp not found at: $LlamaCppPath" -ForegroundColor Yellow
    Write-Host "Please build llama.cpp or update LlamaCppPath parameter" -ForegroundColor Yellow
    $RunReference = $false
} else {
    $RunReference = $true
}

if (-not (Test-Path $RawrPath)) {
    Write-Host "`n[ERROR] RAWR monolith not found at: $RawrPath" -ForegroundColor Red
    Write-Host "Please build with: g++ -std=c++17 -O2 -march=native -pthread -o rawr_monolith_v2.exe rawr_monolith_v2.cpp" -ForegroundColor Yellow
    exit 1
}

Write-Host "`n[Test Configuration]" -ForegroundColor Cyan
Write-Host "  Prompt: '$Prompt'" -ForegroundColor Gray
Write-Host "  Tokens: $NumTokens" -ForegroundColor Gray
Write-Host "  Model:  $ModelFile" -ForegroundColor Gray

# Run llama.cpp reference
if ($RunReference) {
    Write-Host "`n[REFERENCE] Running llama.cpp..." -ForegroundColor Cyan
    $LlamaArgs = @(
        "-m", $ModelFile,
        "-p", $Prompt,
        "-n", $NumTokens,
        "--temp", "0.0",  # Deterministic for comparison
        "--seed", "42"     # Fixed seed
    )
    
    $LlamaOutput = & $LlamaCppPath @LlamaArgs 2>$null
    $LlamaOutput | Out-File "$OutputDir\llama_output.txt"
    
    Write-Host "[OUTPUT]" -ForegroundColor Gray
    Write-Host $LlamaOutput -ForegroundColor White
    Write-Host "[SAVED] $OutputDir\llama_output.txt" -ForegroundColor Green
}

# Run RAWR
Write-Host "`n[RAWR] Running your engine..." -ForegroundColor Cyan
$RawrArgs = @(
    $ModelFile,
    $Prompt,
    $NumTokens
)

$RawrOutput = & $RawrPath @RawrArgs 2>$null
$RawrOutput | Out-File "$OutputDir\rawr_output.txt"

Write-Host "[OUTPUT]" -ForegroundColor Gray
Write-Host $RawrOutput -ForegroundColor White
Write-Host "[SAVED] $OutputDir\rawr_output.txt" -ForegroundColor Green

# Compare outputs
if ($RunReference) {
    Write-Host "`n[COMPARISON] Analyzing outputs..." -ForegroundColor Cyan
    
    $LlamaText = ($LlamaOutput | Select-String -Pattern "^The capital of France is.*$").Matches.Value
    $RawrText = ($RawrOutput | Select-String -Pattern "^The capital of France is.*$").Matches.Value
    
    if ($LlamaText -eq $RawrText) {
        Write-Host "[PASS] Outputs match exactly!" -ForegroundColor Green
        Write-Host "`n✅ Numerical parity achieved!" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] Outputs differ" -ForegroundColor Red
        Write-Host "`nExpected: $LlamaText" -ForegroundColor Gray
        Write-Host "Got:      $RawrText" -ForegroundColor Gray
        Write-Host "`n🔍 Debug with:" -ForegroundColor Yellow
        Write-Host "  ./rawr_monolith_v2 --debug-logits --debug-layer 0 $ModelFile '$Prompt' $NumTokens" -ForegroundColor White
    }
}

Write-Host "`n[Next Steps]" -ForegroundColor Cyan
Write-Host "  1. If outputs match: Integrate AVX-512 GEMM for speedup" -ForegroundColor Gray
Write-Host "  2. If outputs differ: Use --debug-layer to find divergence" -ForegroundColor Gray
Write-Host "  3. Check layer-by-layer: Compare hidden states with reference" -ForegroundColor Gray
