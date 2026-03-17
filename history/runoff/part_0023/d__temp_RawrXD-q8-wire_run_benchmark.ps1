$ErrorActionPreference = "Continue"
$env:PATH = "C:\Qt\6.7.3\msvc2022_64\bin;$env:PATH"

Write-Host "Starting GPU Benchmark..." -ForegroundColor Green
Write-Host "Working Directory: $PWD"
Write-Host "Executable exists: $(Test-Path '.\gpu_inference_benchmark.exe')"
Write-Host ""

try {
    $process = Start-Process -FilePath ".\gpu_inference_benchmark.exe" `
        -ArgumentList '"D:\OllamaModels"', '64' `
        -Wait `
        -PassThru `
        -RedirectStandardOutput "benchmark_output.txt" `
        -RedirectStandardError "benchmark_error.txt" `
        -NoNewWindow
    
    Write-Host "Exit Code: $($process.ExitCode)" -ForegroundColor Yellow
    
    if (Test-Path "benchmark_output.txt") {
        Write-Host "`n=== STDOUT ===" -ForegroundColor Cyan
        Get-Content "benchmark_output.txt"
    }
    
    if (Test-Path "benchmark_error.txt") {
        Write-Host "`n=== STDERR ===" -ForegroundColor Red
        Get-Content "benchmark_error.txt"
    }
} catch {
    Write-Host "Exception: $_" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
}
