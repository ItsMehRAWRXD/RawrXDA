# 🌐 Cloud Models Deep Dive & Live Testing
# Explains how cloud models work and tests their connectivity

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Cloud Models Explained & Live Test" -ForegroundColor Cyan
Write-Host "  How do cloud models work in Ollama?" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Get your cloud models
$cloudModels = @(
  "glm-4.6:cloud",
  "minimax-m2:cloud",
  "gpt-oss:20b-cloud",
  "gpt-oss:120b-cloud",
  "qwen3-coder:480b-cloud",
  "qwen3-vl:235b-cloud"
)

Write-Host "📚 How Cloud Models Work in Ollama:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

Write-Host "`n🔄 Architecture Overview:" -ForegroundColor Cyan
Write-Host "  1. 🖥️ Your App (RawrXD) ➜ 📡 Ollama Local Server ➜ 🌐 Cloud API ➜ 🤖 Remote Model"
Write-Host "  2. Ollama acts as a proxy/gateway to cloud services"
Write-Host "  3. Your requests get forwarded to external APIs"
Write-Host "  4. Responses come back through the same chain"

Write-Host "`n🔑 Authentication Flow:" -ForegroundColor Cyan
Write-Host "  • Cloud models require API keys/tokens"
Write-Host "  • Ollama stores these credentials securely"
Write-Host "  • Each request includes auth headers automatically"

Write-Host "`n💰 Billing Model:" -ForegroundColor Cyan
Write-Host "  • Pay-per-use (tokens consumed)"
Write-Host "  • Rate limits based on subscription tier"
Write-Host "  • No local storage needed (models stay in cloud)"

Write-Host "`n⚡ Performance Characteristics:" -ForegroundColor Cyan
Write-Host "  • Latency: Higher (network roundtrip)"
Write-Host "  • Throughput: Depends on your internet & API limits"
Write-Host "  • Reliability: Depends on cloud service uptime"

Write-Host "`n🧪 Testing Your Cloud Models:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

foreach ($model in $cloudModels) {
  Write-Host "`n🔍 Testing: $model" -ForegroundColor White
  Write-Host "   Model Type: " -NoNewline -ForegroundColor Gray

  # Determine model type and requirements
  switch -Regex ($model) {
    "glm-4.6:cloud" {
      Write-Host "GLM-4.6 (Zhipu AI)" -ForegroundColor Red
      Write-Host "   📋 Requirements: Zhipu API key, subscription plan" -ForegroundColor Gray
      Write-Host "   🌍 Endpoint: https://open.bigmodel.cn/api/paas/v4/" -ForegroundColor Gray
      Write-Host "   💳 Billing: Token-based (input + output tokens)" -ForegroundColor Gray
    }
    "minimax-m2:cloud" {
      Write-Host "MiniMax M2 (MiniMax AI)" -ForegroundColor Red
      Write-Host "   📋 Requirements: MiniMax API key, active account" -ForegroundColor Gray
      Write-Host "   🌍 Endpoint: https://api.minimax.chat/v1/" -ForegroundColor Gray
      Write-Host "   💳 Billing: Per-request pricing model" -ForegroundColor Gray
    }
    "gpt-oss:.*cloud" {
      Write-Host "Custom GPT OSS Cloud" -ForegroundColor Cyan
      Write-Host "   📋 Requirements: Your custom API configuration" -ForegroundColor Gray
      Write-Host "   🌍 Endpoint: Your configured endpoint" -ForegroundColor Gray
      Write-Host "   💳 Billing: Depends on your hosting setup" -ForegroundColor Gray
    }
    "qwen.*:.*cloud" {
      Write-Host "Qwen Cloud (Alibaba)" -ForegroundColor Red
      Write-Host "   📋 Requirements: Alibaba Cloud API key" -ForegroundColor Gray
      Write-Host "   🌍 Endpoint: https://dashscope.aliyuncs.com/api/" -ForegroundColor Gray
      Write-Host "   💳 Billing: Token consumption model" -ForegroundColor Gray
    }
  }

  # Test basic connectivity
  Write-Host "   🔌 Testing connectivity..." -NoNewline -ForegroundColor Yellow

  try {
    # Try a very simple generation with short timeout
    $testPrompt = "Hi"
    $result = ollama generate $model $testPrompt --timeout 10 2>$null

    if ($LASTEXITCODE -eq 0 -and $result) {
      Write-Host " ✅ CONNECTED" -ForegroundColor Green
      Write-Host "   📤 Test Response: $($result.Substring(0, [Math]::Min(50, $result.Length)))..." -ForegroundColor Green
    }
    else {
      Write-Host " ❌ CONNECTION FAILED" -ForegroundColor Red

      # Try to get more specific error info
      $errorResult = ollama generate $model $testPrompt 2>&1
      if ($errorResult -match "429") {
        Write-Host "   ⚠️ Rate limited (429 Too Many Requests)" -ForegroundColor Yellow
      }
      elseif ($errorResult -match "401|403") {
        Write-Host "   🔑 Authentication failed - check API key" -ForegroundColor Red
      }
      elseif ($errorResult -match "404") {
        Write-Host "   ❓ Model not found or not accessible" -ForegroundColor Red
      }
      else {
        Write-Host "   ❌ Network or service error" -ForegroundColor Red
      }
    }
  }
  catch {
    Write-Host " ❌ ERROR: $_" -ForegroundColor Red
  }

  Write-Host ""
}

Write-Host "`n🔧 How to Configure Cloud Models:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

Write-Host "`n1. 🔑 Get API Keys:" -ForegroundColor Cyan
Write-Host "   • GLM-4.6: Register at https://zhipuai.cn/"
Write-Host "   • MiniMax: Sign up at https://api.minimax.chat/"
Write-Host "   • Qwen Cloud: Alibaba Cloud Dashboard"

Write-Host "`n2. 🛠️ Configure Ollama:" -ForegroundColor Cyan
Write-Host "   • Set environment variables:"
Write-Host "     $env:ZHIPU_API_KEY = 'your-zhipu-key'"
Write-Host "     $env:MINIMAX_API_KEY = 'your-minimax-key'"
Write-Host "     $env:DASHSCOPE_API_KEY = 'your-alibaba-key'"

Write-Host "`n3. 📥 Install Models:" -ForegroundColor Cyan
Write-Host "   ollama pull glm-4.6:cloud"
Write-Host "   ollama pull minimax-m2:cloud"

Write-Host "`n💡 Cloud vs Local Comparison:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

Write-Host "`n🏠 Local Models (like your bigdaddyg series):" -ForegroundColor Green
Write-Host "   ✅ No ongoing costs after download"
Write-Host "   ✅ No rate limits"
Write-Host "   ✅ Works offline"
Write-Host "   ✅ Full privacy (data stays local)"
Write-Host "   ❌ Uses your hardware resources"
Write-Host "   ❌ Limited to downloaded model size/capability"

Write-Host "`n🌐 Cloud Models:" -ForegroundColor Red
Write-Host "   ✅ Access to larger, more powerful models"
Write-Host "   ✅ No local hardware requirements"
Write-Host "   ✅ Always up-to-date model versions"
Write-Host "   ❌ Ongoing costs per usage"
Write-Host "   ❌ Rate limits"
Write-Host "   ❌ Requires internet connection"
Write-Host "   ❌ Data sent to external services"

Write-Host "`n🎯 Best Practices for Your Setup:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

Write-Host "`n💡 Recommended Strategy:" -ForegroundColor Cyan
Write-Host "   1. Use your custom models (bigdaddyg, cheetah-stealth) for primary tasks"
Write-Host "   2. Use official local models (llama3, qwen3) for general work"
Write-Host "   3. Reserve cloud models for specialized tasks requiring extra power"
Write-Host "   4. Monitor cloud usage to control costs"

Write-Host "`n🔄 Fallback Strategy:" -ForegroundColor Cyan
Write-Host "   RawrXD can be configured to:"
Write-Host "   • Try local model first"
Write-Host "   • Fall back to cloud model if local fails"
Write-Host "   • Handle rate limits gracefully"

Write-Host "`n📊 Cost Optimization:" -ForegroundColor Cyan
Write-Host "   • Most of your models (15/17) are FREE!"
Write-Host "   • Your custom models are your best value"
Write-Host "   • Consider unsubscribing from unused cloud services"

Write-Host "`n🧪 Next Steps:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow
Write-Host "   • Configure API keys for cloud models you want to use"
Write-Host "   • Test your custom models with: .\Headless-Agentic-Test-CustomModels.ps1"
Write-Host "   • Set up fallback logic in RawrXD configuration"
Write-Host "   • Monitor usage and costs for cloud models"

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Cloud models explained! Your local models are probably sufficient for most tasks." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
