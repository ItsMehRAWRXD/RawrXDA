# 🔥 Live Ollama API Test
# Test actual connectivity and response from Ollama service

Write-Host "🤖 Testing Live Ollama API Connection..." -ForegroundColor Cyan

try {
    # Test basic connectivity
    $testConnection = Test-NetConnection -ComputerName "localhost" -Port 11434 -InformationLevel Quiet -WarningAction SilentlyContinue
    
    if ($testConnection) {
        Write-Host "✅ Ollama service is running on localhost:11434" -ForegroundColor Green
        
        # Test API endpoint
        try {
            $response = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET -TimeoutSec 5
            Write-Host "✅ Ollama API accessible" -ForegroundColor Green
            
            if ($response.models -and $response.models.Count -gt 0) {
                Write-Host "✅ Available models detected: $($response.models.Count) models" -ForegroundColor Green
                Write-Host "📋 Models:" -ForegroundColor Yellow
                foreach ($model in $response.models[0..2]) {  # Show first 3 models
                    Write-Host "   • $($model.name)" -ForegroundColor Gray
                }
                if ($response.models.Count -gt 3) {
                    Write-Host "   • ... and $($response.models.Count - 3) more" -ForegroundColor Gray
                }
                
                # Test a simple generation
                Write-Host "`n🎯 Testing simple generation..." -ForegroundColor Cyan
                $testModel = $response.models[0].name
                
                $generateRequest = @{
                    model = $testModel
                    prompt = "Hello! Please respond with exactly 'OLLAMA_TEST_SUCCESS' if you can read this."
                    stream = $false
                } | ConvertTo-Json
                
                $generateResponse = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $generateRequest -ContentType "application/json" -TimeoutSec 10
                
                if ($generateResponse.response -match "OLLAMA_TEST_SUCCESS") {
                    Write-Host "✅ Ollama generation working perfectly!" -ForegroundColor Green
                    Write-Host "🎉 AGENTIC CAPABILITIES FULLY OPERATIONAL!" -ForegroundColor Magenta
                } else {
                    Write-Host "⚠️ Ollama responded but not as expected:" -ForegroundColor Yellow
                    Write-Host "   Response: $($generateResponse.response)" -ForegroundColor Gray
                }
                
            } else {
                Write-Host "⚠️ No models found in Ollama" -ForegroundColor Yellow
                Write-Host "   You may need to pull a model: ollama pull llama2" -ForegroundColor Gray
            }
            
        } catch {
            Write-Host "❌ Ollama API error: $_" -ForegroundColor Red
        }
        
    } else {
        Write-Host "❌ Ollama service not running on localhost:11434" -ForegroundColor Red
        Write-Host "   Start Ollama service to enable AI capabilities" -ForegroundColor Gray
    }
    
} catch {
    Write-Host "❌ Network test failed: $_" -ForegroundColor Red
}

Write-Host "`n🔚 Live API test complete!" -ForegroundColor Cyan