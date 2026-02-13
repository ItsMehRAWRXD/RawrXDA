# MiniSmoke.ps1 - One-click smoke test for PoshLLM (MiniGPT)
# - Imports local module
# - Trains 1 epoch on micro-corpus
# - Generates 12 tokens with max seq len 64
# - Prints generated text
# - Exits 0 on success, non-zero on error

$ErrorActionPreference = 'Stop'
try {
    # Import local module
    Import-Module "$PSScriptRoot/PoshLLM.psd1" -Force

    # Micro-corpus
    $corpus = @(
        'the quick brown fox',
        'the quick script in powershell',
        'powershell makes scripting enjoyable'
    )

    # Initialize and train (1 epoch), max length 64
    Initialize-PoshLLM -Name mini -Corpus $corpus -Epochs 1 -MaxSeqLen 64 | Out-Null

    # Generate 12 tokens
    $result = Invoke-PoshLLM -Name mini -Prompt 'the quick' -MaxTokens 12 -Temperature 0.9

    # Print output
    Write-Host "Model: $($result.Model)" -ForegroundColor Cyan
    Write-Host "Prompt: $($result.Prompt)" -ForegroundColor Cyan
    Write-Host "Output: $($result.Output)" -ForegroundColor Green

    exit 0
}
catch {
    Write-Host "Smoke test FAILED:" -ForegroundColor Red
    Write-Host ($_ | Out-String)
    exit 1
}