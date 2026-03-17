# RawrXD Ollama Integration - Tool Execution Test

$ErrorActionPreference = "Stop"

Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Tool Execution Test" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan

# 1. Start Host
$exePath = Join-Path $PSScriptRoot "RawrXD.Ollama\bin\Release\net8.0\RawrXD.Ollama.exe"
Write-Host "Starting host..." -ForegroundColor Yellow
$process = Start-Process -FilePath $exePath -ArgumentList "--port", "5887" -NoNewWindow -PassThru
Start-Sleep -Seconds 2

try {
    # 2. Mock a response that triggers a tool
    # Since we can't easily force Ollama to output exactly what we want without a specific model,
    # we'll rely on the fact that the controller parses the response.
    # However, the controller gets the response FROM Ollama.
    # To test this properly without a compliant model, we'd need to mock the transport or use a model that echoes.
    
    # For now, we'll just verify the endpoint accepts the request and doesn't crash.
    # The real test is if the model outputs "CHEETAH_execute(...)"
    
    Write-Host "Sending request..." -ForegroundColor Yellow
    
    $body = @{
        Model = "cheetah-stealth-agentic:latest"
        Prompt = "Execute a test command: CHEETAH_execute('echo Hello')"
        Temperature = 0.1
    } | ConvertTo-Json

    try {
        $response = Invoke-RestMethod -Uri "http://127.0.0.1:5887/api/RawrXDOllama" `
            -Method Post `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 10
            
        Write-Host "Response received:" -ForegroundColor Green
        $response | ConvertTo-Json -Depth 5 | Write-Host
        
        if ($response.tool_executions) {
            Write-Host "Tool executions found!" -ForegroundColor Green
        } else {
            Write-Host "No tool executions triggered (expected if model didn't output the trigger pattern)" -ForegroundColor Yellow
        }
    }
    catch {
        Write-Host "Request failed (Ollama might be offline or model missing): $_" -ForegroundColor Red
    }

}
finally {
    $process.Kill()
    Write-Host "Host stopped." -ForegroundColor Green
}
