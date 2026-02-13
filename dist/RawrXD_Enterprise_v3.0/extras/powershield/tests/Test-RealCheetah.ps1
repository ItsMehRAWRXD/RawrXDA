# Test-RealCheetah.ps1
# Tests the full chain: C# Host -> Real CHEETAH Model -> Tool Execution

$ErrorActionPreference = "Stop"
$HostExe = ".\RawrXD.Ollama\bin\Release\net8.0\RawrXD.Ollama.exe"
$Port = 5888
$Model = "bigdaddyg-cheetah:latest"

Write-Host "🐆 STARTING REAL CHEETAH INTEGRATION TEST 🐆" -ForegroundColor Cyan

# 1. Start C# Host
Write-Host "`n[1/3] Starting C# Host on port $Port..." -ForegroundColor Yellow
$process = Start-Process -FilePath $HostExe -ArgumentList "--port", "$Port" -NoNewWindow -PassThru
Start-Sleep -Seconds 2

try {
    # 2. Send Request
    Write-Host "[2/3] Sending prompt 'execute whoami' to model '$Model'..." -ForegroundColor Yellow
    
    $payload = @{
        Model = $Model
        Prompt = "execute whoami"
        Temperature = 0.1 # Low temp for deterministic output
    } | ConvertTo-Json

    $response = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/api/RawrXDOllama" `
        -Method Post `
        -Body $payload `
        -ContentType "application/json" `
        -TimeoutSec 60 # Give the Q2 model some time

    # 3. Analyze Results
    Write-Host "`n[3/3] Analyzing Response..." -ForegroundColor Yellow
    
    Write-Host "Model Response Text:" -ForegroundColor Gray
    Write-Host $response.response -ForegroundColor White
    
    if ($response.tool_executions -and $response.tool_executions.Count -gt 0) {
        Write-Host "`n✅ TOOL EXECUTION DETECTED!" -ForegroundColor Green
        $response.tool_executions | ForEach-Object {
            Write-Host "   $_" -ForegroundColor Green
        }
        
        if ($response.tool_executions[0] -match "desktop" -or $response.tool_executions[0] -match "user" -or $response.tool_executions[0] -match "\\") {
             Write-Host "`n🎉 SUCCESS: It looks like a real user/path was returned!" -ForegroundColor Cyan
        }
    } else {
        Write-Host "`n⚠️ NO TOOLS EXECUTED" -ForegroundColor Red
        Write-Host "The model might not have output the exact 'CHEETAH_execute(""..."")' format."
        Write-Host "Raw response: $($response.response)"
    }

}
catch {
    Write-Host "`n❌ ERROR: $_" -ForegroundColor Red
    if ($_.Exception.Response) {
        $reader = New-Object System.IO.StreamReader $_.Exception.Response.GetResponseStream()
        Write-Host "Server Response: $($reader.ReadToEnd())" -ForegroundColor Red
    }
}
finally {
    $process.Kill()
    Write-Host "`nHost stopped." -ForegroundColor Gray
}
