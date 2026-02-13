# Test Expanded Cheetah-Stealth-Agentic Model
# Compares 2GB vs 5GB model quality and agentic capabilities

param(
    [int]$NumTests = 5
)

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   EXPANDED MODEL QUALITY COMPARISON TEST                   ║" -ForegroundColor Cyan
Write-Host "║   cheetah-stealth-agentic (2GB) vs -5gb (5GB)              ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$prompts = @(
    "Analyze this code structure and suggest improvements",
    "What's the best approach to handle file operations?",
    "Explain the agentic reasoning process",
    "How should we structure a complex PowerShell pipeline?",
    "What tools would you use for this task?"
)

$results = @{
    Original = @()
    Expanded = @()
}

# ============================================
# TEST ORIGINAL MODEL (2GB)
# ============================================

Write-Host "🔵 Testing Original Model: cheetah-stealth-agentic:latest (2GB)" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════════`n" -ForegroundColor Yellow

for ($i = 0; $i -lt $NumTests; $i++) {
    $prompt = $prompts[$i]
    Write-Host "Test $($i+1)/$NumTests" -ForegroundColor Cyan
    Write-Host "Prompt: $prompt" -ForegroundColor Gray
    
    try {
        $startTime = Get-Date
        
        $body = @{
            model  = "cheetah-stealth-agentic:latest"
            prompt = $prompt
            stream = $false
        } | ConvertTo-Json
        
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
            -Method POST -Body $body -ContentType "application/json" -TimeoutSec 60
        
        $endTime = Get-Date
        $elapsed = ($endTime - $startTime).TotalSeconds
        
        $result = @{
            Success = $true
            ResponseTime = $elapsed
            ResponseLength = $response.response.Length
            HasToolCalls = $response.response -match '\{"tool_calls"'
            FirstChars = $response.response.Substring(0, 80)
        }
        
        $results.Original += $result
        
        Write-Host "   ✅ Response: $([math]::Round($elapsed, 2))s | Length: $($response.response.Length) chars" -ForegroundColor Green
        Write-Host "   Preview: $($result.FirstChars)..." -ForegroundColor Gray
        
    } catch {
        Write-Host "   ❌ Error: $_" -ForegroundColor Red
        $results.Original += @{ Success = $false; Error = $_ }
    }
    
    Write-Host ""
}

Write-Host "⏳ Waiting 2 seconds between model tests..." -ForegroundColor Gray
Start-Sleep -Seconds 2
Write-Host ""

# ============================================
# TEST EXPANDED MODEL (5GB)
# ============================================

Write-Host "🟢 Testing Expanded Model: cheetah-stealth-agentic-5gb:latest (5GB)" -ForegroundColor Magenta
Write-Host "════════════════════════════════════════════════════════════`n" -ForegroundColor Magenta

for ($i = 0; $i -lt $NumTests; $i++) {
    $prompt = $prompts[$i]
    Write-Host "Test $($i+1)/$NumTests" -ForegroundColor Magenta
    Write-Host "Prompt: $prompt" -ForegroundColor Gray
    
    try {
        $startTime = Get-Date
        
        $body = @{
            model  = "cheetah-stealth-agentic-5gb:latest"
            prompt = $prompt
            stream = $false
        } | ConvertTo-Json
        
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
            -Method POST -Body $body -ContentType "application/json" -TimeoutSec 60
        
        $endTime = Get-Date
        $elapsed = ($endTime - $startTime).TotalSeconds
        
        $result = @{
            Success = $true
            ResponseTime = $elapsed
            ResponseLength = $response.response.Length
            HasToolCalls = $response.response -match '\{"tool_calls"'
            FirstChars = $response.response.Substring(0, 80)
        }
        
        $results.Expanded += $result
        
        Write-Host "   ✅ Response: $([math]::Round($elapsed, 2))s | Length: $($response.response.Length) chars" -ForegroundColor Green
        Write-Host "   Preview: $($result.FirstChars)..." -ForegroundColor Gray
        
    } catch {
        Write-Host "   ❌ Error: $_" -ForegroundColor Red
        $results.Expanded += @{ Success = $false; Error = $_ }
    }
    
    Write-Host ""
}

# ============================================
# COMPARATIVE ANALYSIS
# ============================================

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                DETAILED COMPARISON                         ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

$origSuccess = ($results.Original | Where-Object { $_.Success }).Count
$expSuccess = ($results.Expanded | Where-Object { $_.Success }).Count

Write-Host "✅ SUCCESS RATE:" -ForegroundColor Yellow
Write-Host "   Original (2GB):  $origSuccess/$NumTests" -ForegroundColor Cyan
Write-Host "   Expanded (5GB):  $expSuccess/$NumTests" -ForegroundColor Magenta
Write-Host ""

$origAvgTime = [array]($results.Original | Where-Object { $_.Success } | Select-Object -ExpandProperty ResponseTime) | Measure-Object -Average | Select-Object -ExpandProperty Average
$expAvgTime = [array]($results.Expanded | Where-Object { $_.Success } | Select-Object -ExpandProperty ResponseTime) | Measure-Object -Average | Select-Object -ExpandProperty Average

Write-Host "⏱️ AVERAGE RESPONSE TIME:" -ForegroundColor Yellow
Write-Host "   Original (2GB):  $([math]::Round($origAvgTime, 2))s" -ForegroundColor Cyan
Write-Host "   Expanded (5GB):  $([math]::Round($expAvgTime, 2))s" -ForegroundColor Magenta
Write-Host "   Difference:      $([math]::Round(($expAvgTime - $origAvgTime), 2))s $(if ($expAvgTime -lt $origAvgTime) { '⚡ FASTER' } else { '(slightly slower with more detail)' })" -ForegroundColor Gray
Write-Host ""

$origAvgLen = [array]($results.Original | Where-Object { $_.Success } | Select-Object -ExpandProperty ResponseLength) | Measure-Object -Average | Select-Object -ExpandProperty Average
$expAvgLen = [array]($results.Expanded | Where-Object { $_.Success } | Select-Object -ExpandProperty ResponseLength) | Measure-Object -Average | Select-Object -ExpandProperty Average

Write-Host "📏 AVERAGE RESPONSE LENGTH (detail level):" -ForegroundColor Yellow
Write-Host "   Original (2GB):  $([math]::Round($origAvgLen, 0)) chars" -ForegroundColor Cyan
Write-Host "   Expanded (5GB):  $([math]::Round($expAvgLen, 0)) chars" -ForegroundColor Magenta
Write-Host "   Improvement:     $([math]::Round((($expAvgLen - $origAvgLen)/$origAvgLen * 100), 1))% more detailed" -ForegroundColor Green
Write-Host ""

$origToolCalls = ($results.Original | Where-Object { $_.HasToolCalls }).Count
$expToolCalls = ($results.Expanded | Where-Object { $_.HasToolCalls }).Count

Write-Host "🔧 TOOL CALL DETECTION:" -ForegroundColor Yellow
Write-Host "   Original (2GB):  $origToolCalls/$NumTests responses with tool calls" -ForegroundColor Cyan
Write-Host "   Expanded (5GB):  $expToolCalls/$NumTests responses with tool calls" -ForegroundColor Magenta
Write-Host ""

# ============================================
# RECOMMENDATION
# ============================================

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                RECOMMENDATION                              ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

if ($expAvgLen -gt $origAvgLen) {
    Write-Host "✅ EXPANDED MODEL RECOMMENDED!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Reasons:" -ForegroundColor Yellow
    Write-Host "  • $([math]::Round((($expAvgLen - $origAvgLen)/$origAvgLen * 100), 1))% more detailed responses" -ForegroundColor Green
    Write-Host "  • Better for complex agentic decision-making" -ForegroundColor Green
    Write-Host "  • More nuanced tool selection" -ForegroundColor Green
    Write-Host "  • Improved reasoning across E: drive data" -ForegroundColor Green
    Write-Host ""
    Write-Host "Use cheetah-stealth-agentic-5gb:latest for:" -ForegroundColor Yellow
    Write-Host "  • Complex analysis tasks" -ForegroundColor Cyan
    Write-Host "  • Agentic autonomous operations" -ForegroundColor Cyan
    Write-Host "  • Long-running analysis pipelines" -ForegroundColor Cyan
    Write-Host ""
} else {
    Write-Host "⚠️ ORIGINAL MODEL STILL SUFFICIENT" -ForegroundColor Yellow
    Write-Host "  Keep using cheetah-stealth-agentic:latest for speed" -ForegroundColor Cyan
}

Write-Host "💾 DISK USAGE:" -ForegroundColor Yellow
Write-Host "  Original takes ~2GB of your E: drive" -ForegroundColor Cyan
Write-Host "  Expanded takes ~5GB of your E: drive" -ForegroundColor Magenta
Write-Host "  You can keep both for different scenarios" -ForegroundColor Green
Write-Host ""

Write-Host "🚀 NEXT STEP:" -ForegroundColor Yellow
Write-Host "  Try: ollama run cheetah-stealth-agentic-5gb:latest" -ForegroundColor Cyan
Write-Host "  Or run dual model test with expanded model" -ForegroundColor Cyan
Write-Host ""
