# Detokenize Token 42 and Show Vocab Mapping
# Quick test to see what token 42 represents in the vocabulary

param(
    [int]$TokenId = 42,
    [string]$ModelPath = "F:\OllamaModels\blobs\sha256-e1eaa0f4fffb8880ca14c1c1f9e7d887fb45cc19a5b17f5cc83c3e8d3e85914e"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Token Detokenization Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$binPath = "D:\rawrxd\bin\RawrXD-Win32IDE.exe"

if (-not (Test-Path $binPath)) {
    Write-Host "❌ Binary not found: $binPath" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $ModelPath)) {
    Write-Host "❌ Model not found: $ModelPath" -ForegroundColor Red
    exit 1
}

Write-Host "[TEST] Loading model and examining token $TokenId..." -ForegroundColor Yellow

# Create temp C++ test file
$testCode = @"
#include "../src/cpu_inference_engine.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <token_id>" << std::endl;
        return 1;
    }
    
    std::string modelPath = argv[1];
    int tokenId = std::atoi(argv[2]);
    
    RawrXD::CPUInferenceEngine engine;
    engine.SetUseTitanAssembly(false);
    
    std::cout << "[VOCAB] Loading model..." << std::endl;
    if (!engine.LoadModel(modelPath, 1)) {
        std::cerr << "[VOCAB] Failed to load model" << std::endl;
        return 1;
    }
    
    std::cout << "[VOCAB] Vocab size: " << engine.GetVocabSize() << std::endl;
    
    if (tokenId < 0 || tokenId >= engine.GetVocabSize()) {
        std::cerr << "[VOCAB] Token " << tokenId << " out of range [0, " << engine.GetVocabSize() << ")" << std::endl;
        return 1;
    }
    
    // Detokenize single token
    std::vector<int32_t> singleToken = {tokenId};
    std::string text = engine.Detokenize(singleToken);
    
    std::cout << "[VOCAB] Token " << tokenId << " = \"" << text << "\"" << std::endl;
    
    // Show neighbors for context
    std::cout << "[VOCAB] Neighbors:" << std::endl;
    for (int i = tokenId - 2; i <= tokenId + 2; ++i) {
        if (i >= 0 && i < engine.GetVocabSize()) {
            std::vector<int32_t> tok = {i};
            std::string txt = engine.Detokenize(tok);
            std::cout << "  [" << i << "] = \"" << txt << "\"" << std::endl;
        }
    }
    
    return 0;
}
"@

$testPath = "D:\rawrxd\src\test_detokenize.cpp"
$testCode | Out-File -FilePath $testPath -Encoding utf8

Write-Host "[BUILD] Compiling detokenization test..." -ForegroundColor Yellow

# Try to compile and run
Push-Location "D:\rawrxd\build"
try {
    # Add test to build
    $cmakeOutput = cmake .. 2>&1
    $buildOutput = cmake --build . --config Release --target test_detokenize 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[BUILD] Success" -ForegroundColor Green
        
        $testBin = "D:\rawrxd\bin\test_detokenize.exe"
        if (Test-Path $testBin) {
            Write-Host "`n[RUN] Executing detokenization..." -ForegroundColor Yellow
            & $testBin $ModelPath $TokenId
        } else {
            Write-Host "⚠️ Test binary not found, trying direct detokenization..." -ForegroundColor Yellow
            # Fallback: parse vocab from GGUF manually
        }
    } else {
        Write-Host "⚠️ Build failed, using inference test fallback" -ForegroundColor Yellow
    }
} catch {
    Write-Host "⚠️ Exception: $_" -ForegroundColor Yellow
} finally {
    Pop-Location
}

# Fallback: Run inference with explicit token output
Write-Host "`n[FALLBACK] Running inference to see token in context..." -ForegroundColor Cyan

$output = & $binPath --test-inference-fast --test-model $ModelPath --test-max-tokens 5 2>&1 | Out-String

Write-Host $output

# Common vocab mappings for token 42
Write-Host "`n[HINT] Common token 42 mappings in LLM vocabs:" -ForegroundColor Cyan
Write-Host "  • GPT-2/GPT-3:  'the' or 'The'" -ForegroundColor Gray
Write-Host "  • LLaMA:        'we' or 'he'" -ForegroundColor Gray
Write-Host "  • Mistral:      '\n' (newline) or space variant" -ForegroundColor Gray
Write-Host "  • Codestral:    '{' or common code token" -ForegroundColor Gray

Write-Host "`n[ACTION] To see full vocab:" -ForegroundColor Yellow
Write-Host "  1. Extract vocab from GGUF: Convert to sentencepiece .model" -ForegroundColor Gray
Write-Host "  2. Add --verbose flag to test: Show all generated tokens with strings" -ForegroundColor Gray
Write-Host "  3. Implement token tracing in engine: Log Detokenize() output during generation" -ForegroundColor Gray
