# RAWR Quick Parity Test
# One-liner to dump logits and compare

param(
    [Parameter(Mandatory=$true)]
    [string]$ModelPath,
    
    [string]$Prompt = "Hello",
    [int]$Tokens = 1,
    [string]$OutputFile = "rawr_logits.bin"
)

$RawrPath = ".\rawr_monolith_v2.exe"

if (-not (Test-Path $RawrPath)) {
    Write-Host "[ERROR] rawr_monolith_v2.exe not found" -ForegroundColor Red
    Write-Host "Build with: g++ -std=c++17 -O2 -march=native -pthread -o rawr_monolith_v2.exe rawr_monolith_v2.cpp" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $ModelPath)) {
    Write-Host "[ERROR] Model not found: $ModelPath" -ForegroundColor Red
    exit 1
}

Write-Host "[RAWR] Running parity test..." -ForegroundColor Cyan
Write-Host "  Model: $ModelPath" -ForegroundColor Gray
Write-Host "  Prompt: '$Prompt'" -ForegroundColor Gray
Write-Host "  Tokens: $Tokens" -ForegroundColor Gray
Write-Host "  Output: $OutputFile" -ForegroundColor Gray

# Run inference with logits dump
& $RawrPath --dump-logits --dump-logits-file $OutputFile $ModelPath $Prompt $Tokens

if (Test-Path $OutputFile) {
    $size = (Get-Item $OutputFile).Length
    Write-Host "`n[SUCCESS] Logits dumped to $OutputFile ($size bytes)" -ForegroundColor Green
    Write-Host "`nNext steps:" -ForegroundColor Yellow
    Write-Host "  1. Capture llama.cpp reference: ./llama-cli -m $ModelPath -p '$Prompt' -n $Tokens --logits-all ref.bin" -ForegroundColor White
    Write-Host "  2. Compare: .\e2e_parity_test.exe ref.bin $OutputFile" -ForegroundColor White
} else {
    Write-Host "`n[ERROR] Failed to create $OutputFile" -ForegroundColor Red
}
