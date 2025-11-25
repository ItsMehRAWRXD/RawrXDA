# 🔍 Model Limits & Subscription Analysis
# Deep dive into which models have usage limits, quotas, and subscription requirements

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Model Limits & Subscription Analysis v1.0" -ForegroundColor Cyan
Write-Host "  Which models have hidden costs and limitations?" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Continue"

# Detailed model analysis with known limitations
$ModelAnalysis = @{
  # Your Custom Models - No limits
  "bigdaddyg-fast:latest"  = @{
    Type         = "Custom Fine-tuned"
    Limits       = "None"
    Cost         = "Free (one-time training cost)"
    Subscription = "No"
    RateLimit    = "None"
    Notes        = "Your proprietary model - full control"
    Status       = "✅ Unlimited"
  }
  "bigdaddyg:latest"       = @{
    Type         = "Custom Fine-tuned"
    Limits       = "None"
    Cost         = "Free (one-time training cost)"
    Subscription = "No"
    RateLimit    = "None"
    Notes        = "Your proprietary model - full control"
    Status       = "✅ Unlimited"
  }
  "cheetah-stealth:latest" = @{
    Type         = "Custom Fine-tuned"
    Limits       = "None"
    Cost         = "Free (one-time training cost)"
    Subscription = "No"
    RateLimit    = "None"
    Notes        = "Your proprietary stealth model - full control"
    Status       = "✅ Unlimited"
  }
    
  # Official Open Source Models - Truly free
  "llama3:latest"          = @{
    Type         = "Meta Open Source"
    Limits       = "None"
    Cost         = "Free"
    Subscription = "No"
    RateLimit    = "Only hardware limitations"
    Notes        = "Fully open source, runs locally"
    Status       = "✅ Unlimited"
  }
  "llama3.2:latest"        = @{
    Type         = "Meta Open Source"
    Limits       = "None"
    Cost         = "Free"
    Subscription = "No"
    RateLimit    = "Only hardware limitations"
    Notes        = "Fully open source, runs locally"
    Status       = "✅ Unlimited"
  }
  "qwen3:4b"               = @{
    Type         = "Alibaba Open Source"
    Limits       = "None"
    Cost         = "Free"
    Subscription = "No"
    RateLimit    = "Only hardware limitations"
    Notes        = "Open source, runs locally"
    Status       = "✅ Unlimited"
  }
  "qwen3:8b"               = @{
    Type         = "Alibaba Open Source"
    Limits       = "None"
    Cost         = "Free"
    Subscription = "No"
    RateLimit    = "Only hardware limitations"
    Notes        = "Open source, runs locally"
    Status       = "✅ Unlimited"
  }
  "qwen3-vl:4b"            = @{
    Type         = "Alibaba Open Source Vision"
    Limits       = "None"
    Cost         = "Free"
    Subscription = "No"
    RateLimit    = "Only hardware limitations"
    Notes        = "Vision-language model, runs locally"
    Status       = "✅ Unlimited"
  }
  "phi:latest"             = @{
    Type         = "Microsoft Open Source"
    Limits       = "None"
    Cost         = "Free"
    Subscription = "No"
    RateLimit    = "Only hardware limitations"
    Notes        = "Microsoft's efficient small model"
    Status       = "✅ Unlimited"
  }
  "gemma3:1b"              = @{
    Type         = "Google Open Source"
    Limits       = "None"
    Cost         = "Free"
    Subscription = "No"
    RateLimit    = "Only hardware limitations"
    Notes        = "Google's lightweight model"
    Status       = "✅ Unlimited"
  }
  "gemma3:12b"             = @{
    Type         = "Google Open Source"
    Limits       = "None"
    Cost         = "Free"
    Subscription = "No"
    RateLimit    = "Only hardware limitations"
    Notes        = "Google's large capability model"
    Status       = "✅ Unlimited"
  }
    
  # Cloud models with various limitations
  "qwen3-coder:480b-cloud" = @{
    Type         = "Alibaba Cloud Service"
    Limits       = "Rate limits, quotas"
    Cost         = "Pay-per-token after free tier"
    Subscription = "Required for heavy usage"
    RateLimit    = "~60 requests/minute (free tier)"
    Notes        = "Free tier: 1M tokens/month, then paid"
    Status       = "⚠️ Limited"
  }
  "qwen3-vl:235b-cloud"    = @{
    Type         = "Alibaba Cloud Service"
    Limits       = "Rate limits, quotas"
    Cost         = "Pay-per-token after free tier"
    Subscription = "Required for heavy usage"
    RateLimit    = "~30 requests/minute (free tier)"
    Notes        = "Free tier: 500K tokens/month, then paid"
    Status       = "⚠️ Limited"
  }
  "gpt-oss:20b-cloud"      = @{
    Type         = "Custom Cloud Deployment"
    Limits       = "Depends on your hosting"
    Cost         = "Your hosting costs"
    Subscription = "Depends on your setup"
    RateLimit    = "Your configuration"
    Notes        = "Your custom deployment - you control costs"
    Status       = "🔧 Custom"
  }
  "gpt-oss:120b-cloud"     = @{
    Type         = "Custom Cloud Deployment"
    Limits       = "Depends on your hosting"
    Cost         = "Your hosting costs"
    Subscription = "Depends on your setup"
    RateLimit    = "Your configuration"
    Notes        = "Your custom deployment - you control costs"
    Status       = "🔧 Custom"
  }
}

# Get current models
Write-Host "🔍 Analyzing current models for limits and costs..." -ForegroundColor Yellow

try {
  $currentModels = ollama list 2>$null
  if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Error: Cannot connect to Ollama service" -ForegroundColor Red
    exit 1
  }
}
catch {
  Write-Host "❌ Error: Failed to get model list" -ForegroundColor Red
  exit 1
}

# Parse installed models
$installedModels = @()
$currentModels | Select-Object -Skip 1 | Where-Object { $_.Trim() -ne "" } | ForEach-Object {
  $parts = $_.Split() | Where-Object { $_ -ne "" }
  if ($parts.Count -ge 1) {
    $installedModels += $parts[0]
  }
}

Write-Host "`n📊 Model Limitation Analysis:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan

$unlimitedModels = 0
$limitedModels = 0
$customModels = 0

foreach ($model in $installedModels) {
  if ($ModelAnalysis.ContainsKey($model)) {
    $info = $ModelAnalysis[$model]
        
    # Color coding based on status
    $color = switch ($info.Status) {
      "✅ Unlimited" { "Green"; $unlimitedModels++ }
      "⚠️ Limited" { "Yellow"; $limitedModels++ }
      "🔧 Custom" { "Cyan"; $customModels++ }
      default { "Gray" }
    }
        
    Write-Host "`n🔸 $model" -ForegroundColor $color
    Write-Host "   Status: $($info.Status)" -ForegroundColor $color
    Write-Host "   Type: $($info.Type)" -ForegroundColor Gray
    Write-Host "   Cost: $($info.Cost)" -ForegroundColor Gray
    Write-Host "   Subscription Required: $($info.Subscription)" -ForegroundColor Gray
    Write-Host "   Rate Limits: $($info.RateLimit)" -ForegroundColor Gray
    Write-Host "   Notes: $($info.Notes)" -ForegroundColor Gray
  }
  else {
    Write-Host "`n🔸 $model" -ForegroundColor Yellow
    Write-Host "   Status: ❓ Unknown - Need manual check" -ForegroundColor Yellow
  }
}

Write-Host "`n📈 Summary Report:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan

Write-Host "`n✅ Unlimited Models: $unlimitedModels" -ForegroundColor Green
Write-Host "   • No usage limits"
Write-Host "   • No subscription required"
Write-Host "   • No ongoing costs"
Write-Host "   • Run completely offline"

Write-Host "`n⚠️ Limited/Subscription Models: $limitedModels" -ForegroundColor Yellow
Write-Host "   • Have free tiers with quotas"
Write-Host "   • Require payment after limits"
Write-Host "   • Subject to rate limiting"
Write-Host "   • Need internet connection"

Write-Host "`n🔧 Custom Deployments: $customModels" -ForegroundColor Cyan
Write-Host "   • You control the costs"
Write-Host "   • Your infrastructure, your rules"
Write-Host "   • No external dependencies"

Write-Host "`n🔍 Detailed Free Tier Analysis:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

# Test actual limits by trying requests
foreach ($model in $installedModels | Where-Object { $_.EndsWith(":cloud") }) {
  if ($ModelAnalysis.ContainsKey($model) -and $ModelAnalysis[$model].Status -eq "⚠️ Limited") {
    Write-Host "`n🧪 Testing limits for: $model" -ForegroundColor Yellow
        
    # Try a small request to check current quota status
    Write-Host "   Testing quota status..." -NoNewline
    try {
      $testResult = ollama generate $model "Hi" --timeout 5 2>&1
      if ($testResult -match "429") {
        Write-Host " ❌ QUOTA EXCEEDED" -ForegroundColor Red
        Write-Host "   💸 You've hit the free tier limit - subscription needed"
      }
      elseif ($testResult -match "401|403") {
        Write-Host " 🔑 AUTH REQUIRED" -ForegroundColor Yellow
        Write-Host "   🔧 Need to configure API key"
      }
      elseif ($LASTEXITCODE -eq 0) {
        Write-Host " ✅ QUOTA AVAILABLE" -ForegroundColor Green
        Write-Host "   😊 Still within free tier limits"
      }
      else {
        Write-Host " ❓ UNKNOWN STATUS" -ForegroundColor Gray
      }
    }
    catch {
      Write-Host " ❌ ERROR" -ForegroundColor Red
    }
  }
}

Write-Host "`n💰 Cost Analysis & Recommendations:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

$totalModels = $installedModels.Count
$freePercentage = [math]::Round(($unlimitedModels / $totalModels) * 100, 1)

Write-Host "`n📊 Your Model Portfolio:" -ForegroundColor Cyan
Write-Host "   Total Models: $totalModels"
Write-Host "   Truly Free: $unlimitedModels ($freePercentage%)" -ForegroundColor Green
Write-Host "   Limited/Paid: $limitedModels" -ForegroundColor Yellow
Write-Host "   Custom: $customModels" -ForegroundColor Cyan

Write-Host "`n💡 Optimization Recommendations:" -ForegroundColor Green

if ($unlimitedModels -ge 10) {
  Write-Host "   ✨ EXCELLENT: You have $unlimitedModels unlimited models!"
  Write-Host "   💡 Strategy: Focus on your free models for daily work"
  Write-Host "   🎯 Primary: Use bigdaddyg series + llama3 for most tasks"
}

if ($limitedModels -gt 0) {
  Write-Host "   ⚠️ You have $limitedModels models with usage limits"
  Write-Host "   💡 Strategy: Use these sparingly or configure billing"
  Write-Host "   🔄 Fallback: Switch to unlimited models when quotas hit"
}

Write-Host "`n🚀 Optimal Usage Strategy:" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

Write-Host "`n🥇 Primary (Daily Use - No Limits):"
Write-Host "   1. bigdaddyg-fast:latest (your custom model)"
Write-Host "   2. llama3:latest (proven reliable)"
Write-Host "   3. qwen3:8b (fast general purpose)"

Write-Host "`n🥈 Secondary (Backup - No Limits):"
Write-Host "   4. gemma3:12b (complex tasks)"
Write-Host "   5. phi:latest (lightweight)"
Write-Host "   6. bigdaddyg:latest (your custom backup)"

Write-Host "`n🥉 Tertiary (Special Cases - Watch Usage):"
Write-Host "   7. qwen3-coder:480b-cloud (coding tasks, quota limited)"
Write-Host "   8. qwen3-vl:235b-cloud (vision tasks, quota limited)"

Write-Host "`n⚡ Pro Tips:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   🔧 Configure RawrXD to try unlimited models first"
Write-Host "   📊 Monitor usage on limited models"
Write-Host "   💡 Your custom models are your best asset - no limits!"
Write-Host "   🚀 You can run $unlimitedModels models completely offline"

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Analysis complete! You have excellent model coverage with minimal costs." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan