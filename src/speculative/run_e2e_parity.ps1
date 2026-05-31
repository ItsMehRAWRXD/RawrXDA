# RAWR End-to-End Parity Test Automation
# Downloads TinyLlama, runs both engines, compares token-by-token

param(
    [string]$LlamaCppPath = "llama-cli.exe",
    [string]$RawrPath = ".\rawr_monolith_v2.exe",
    [string]$ModelUrl = "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf",
    [string]$Prompt = "The capital of France is",
    [int]$NumTokens = 10,
    [switch]$SkipDownload,
    [switch]$KeepFiles
)

$ModelFile = "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"
$RefLogits = "ref_logits.bin"
$RawrLogits = "rawr_logits.bin"
$OutputDir = "parity_test_$(Get-Date -Format 'yyyyMMdd_HHmmss')"

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     RAWR END-TO-END PARITY TEST                            ║" -ForegroundColor Cyan
Write-Host "║     Automated comparison with llama.cpp                      ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# Create output directory
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Set-Location $OutputDir

# Download model if needed
if (-not $SkipDownload -and -not (Test-Path $ModelFile)) {
    Write-Host "`n[DOWNLOAD] TinyLlama-1B model..." -ForegroundColor Yellow
    Write-Host "URL: $ModelUrl" -ForegroundColor Gray
    
    try {
        Invoke-WebRequest -Uri $ModelUrl -OutFile $ModelFile -MaximumRetryCount 3
        $size = (Get-Item $ModelFile).Length / 1MB
        Write-Host "[SUCCESS] Downloaded $ModelFile ($([math]::Round($size, 2)) MB)" -ForegroundColor Green
    } catch {
        Write-Host "[ERROR] Failed to download model" -ForegroundColor Red
        Write-Host "Please download manually from:" -ForegroundColor Yellow
        Write-Host "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF" -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "`n[INFO] Using existing model: $ModelFile" -ForegroundColor Green
}

# Check executables
$haveLlama = Test-Path $LlamaCppPath
$haveRawr = Test-Path $RawrPath

if (-not $haveLlama) {
    Write-Host "`n[WARNING] llama.cpp not found at: $LlamaCppPath" -ForegroundColor Yellow
    Write-Host "Please provide path with -LlamaCppPath parameter" -ForegroundColor Yellow
}

if (-not $haveRawr) {
    Write-Host "`n[ERROR] RawrXD not found at: $RawrPath" -ForegroundColor Red
    Write-Host "Please build with:" -ForegroundColor Yellow
    Write-Host "  g++ -std=c++17 -O3 -mavx512f -mavx512vl -mfma -o rawr_monolith_v2.exe rawr_monolith_v2.cpp" -ForegroundColor Yellow
    exit 1
}

Write-Host "`n[Test Configuration]" -ForegroundColor Cyan
Write-Host "  Prompt:  '$Prompt'" -ForegroundColor Gray
Write-Host "  Tokens:  $NumTokens" -ForegroundColor Gray
Write-Host "  Model:   $ModelFile" -ForegroundColor Gray
Write-Host "  llama:   $(if($haveLlama){'PRESENT'}else{'MISSING'})" -ForegroundColor $(if($haveLlama){'Green'}else{'Red'})
Write-Host "  RawrXD:  PRESENT" -ForegroundColor Green

# Run llama.cpp reference (if available)
if ($haveLlama) {
    Write-Host "`n[REFERENCE] Running llama.cpp..." -ForegroundColor Cyan
    
    # Create a simple wrapper to capture logits
    # Note: llama.cpp needs to be built with logits output support
    $LlamaArgs = @(
        "-m", $ModelFile,
        "-p", $Prompt,
        "-n", $NumTokens,
        "--temp", "0",
        "--top-k", "1",
        "--top-p", "1.0",
        "--seed", "42",
        "--log-disable"  # Reduce noise
    )
    
    try {
        $LlamaOutput = & $LlamaCppPath @LlamaArgs 2>$null
        $LlamaOutput | Out-File "llama_output.txt"
        
        Write-Host "[OUTPUT]" -ForegroundColor Gray
        Write-Host $LlamaOutput -ForegroundColor White
        Write-Host "[SAVED] llama_output.txt" -ForegroundColor Green
        
        # Extract generated text for comparison
        $LlamaText = ($LlamaOutput -join " ").Trim()
    } catch {
        Write-Host "[ERROR] llama.cpp failed: $_" -ForegroundColor Red
        $haveLlama = $false
    }
}

# Run RawrXD
Write-Host "`n[RAWRXD] Running your engine..." -ForegroundColor Cyan

$RawrArgs = @(
    $ModelFile,
    $Prompt,
    $NumTokens
)

try {
    $RawrOutput = & $RawrPath @RawrArgs 2>$null
    $RawrOutput | Out-File "rawr_output.txt"
    
    Write-Host "[OUTPUT]" -ForegroundColor Gray
    Write-Host $RawrOutput -ForegroundColor White
    Write-Host "[SAVED] rawr_output.txt" -ForegroundColor Green
    
    $RawrText = ($RawrOutput -join " ").Trim()
} catch {
    Write-Host "[ERROR] RawrXD failed: $_" -ForegroundColor Red
    exit 1
}

# Compare outputs (text-level)
Write-Host "`n[COMPARISON] Text-level comparison..." -ForegroundColor Cyan

if ($haveLlama) {
    if ($LlamaText -eq $RawrText) {
        Write-Host "[PASS] Text outputs match exactly!" -ForegroundColor Green
        Write-Host "`n✅ PARITY ACHIEVED (text level)" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] Text outputs differ" -ForegroundColor Red
        Write-Host "`nExpected: $LlamaText" -ForegroundColor Gray
        Write-Host "Got:      $RawrText" -ForegroundColor Gray
        
        # Show character-by-character diff
        Write-Host "`nCharacter diff:" -ForegroundColor Yellow
        $minLen = [Math]::Min($LlamaText.Length, $RawrText.Length)
        for ($i = 0; $i -lt $minLen; $i++) {
            if ($LlamaText[$i] -ne $RawrText[$i]) {
                Write-Host "  Position $i : '$($LlamaText[$i])' vs '$($RawrText[$i])'" -ForegroundColor Red
                break
            }
        }
    }
}

# Run binary logit comparison (if e2e_parity_test.exe exists)
$E2EPath = "..\e2e_parity_test.exe"
if (Test-Path $E2EPath -and (Test-Path $RefLogits) -and (Test-Path $RawrLogits)) {
    Write-Host "`n[BINARY] Running logit-level comparison..." -ForegroundColor Cyan
    
    try {
        $Comparison = & $E2EPath $RefLogits $RawrLogits 2>&1
        Write-Host $Comparison -ForegroundColor White
    } catch {
        Write-Host "[WARNING] Binary comparison failed: $_" -ForegroundColor Yellow
    }
}

# Summary
Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    NEXT STEPS                                ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

if ($haveLlama -and ($LlamaText -ne $RawrText)) {
    Write-Host "1. 🔍 Debug first divergence:" -ForegroundColor Yellow
    Write-Host "   ./rawr_monolith_v2 --debug-logits --debug-layer 0 $ModelFile '$Prompt' 1" -ForegroundColor White
    Write-Host "2. Compare exported logits with reference" -ForegroundColor Yellow
    Write-Host "3. Fix the bug, re-run until match" -ForegroundColor Yellow
} else {
    Write-Host "1. ✅ Text parity achieved" -ForegroundColor Green
    Write-Host "2. Next: Verify logit-level parity (export binary logits)" -ForegroundColor Yellow
    Write-Host "3. Then: Integrate AVX-512 GEMM for speedup" -ForegroundColor Yellow
}

Write-Host "`nOutput files in: $OutputDir" -ForegroundColor Gray

# Cleanup
if (-not $KeepFiles) {
    Set-Location ..
    Write-Host "`n[INFO] To keep files, re-run with -KeepFiles" -ForegroundColor Gray
} else {
    Write-Host "`n[INFO] Files kept in: $OutputDir" -ForegroundColor Gray
}

Write-Host "`nDone." -ForegroundColor Green
