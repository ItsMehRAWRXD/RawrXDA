#!/usr/bin/env pwsh
# Quick wrapper to run spec bench and save output
$env:OLLAMA_MODELS = "F:\OllamaModels"
$env:OLLAMA_VULKAN = "false"
Remove-Item env:GGML_VK_VISIBLE_DEVICES -ErrorAction SilentlyContinue
Remove-Item env:HIP_VISIBLE_DEVICES -ErrorAction SilentlyContinue

$outFile = "D:\rawrxd\logs\spec_bench_full.txt"

# Capture all output
$null = Start-Transcript -Path $outFile -Force

& "D:\rawrxd\src\Beyond200\RawrXD_SmokeTest_v270_Spec.ps1" `
    -DraftModel "gemma3:1b" `
    -VerifyModel "qwen2.5:7b" `
    -Burst 4 `
    -Runs 3 `
    -TargetTokens 32 `
    -Verbose

Stop-Transcript

Write-Host "BENCHMARK COMPLETE — output in $outFile"
