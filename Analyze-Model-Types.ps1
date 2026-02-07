# 🔍 Ollama Model Analysis: Custom vs Official vs Subscription
# Analyzes all available models to categorize ownership and requirements

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Ollama Model Type Analysis v1.0" -ForegroundColor Cyan
Write-Host "  Custom vs Official vs Subscription Analysis" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Continue"

# Known Ollama official models (these are free and don't require subscriptions)
$OfficialModels = @{
  "llama3"      = "Meta's Llama 3 - Open source"
  "llama3.2"    = "Meta's Llama 3.2 - Open source"
  "qwen3"       = "Alibaba Qwen 3 - Open source"
  "qwen3-vl"    = "Alibaba Qwen 3 Vision-Language - Open source"
  "qwen3-coder" = "Alibaba Qwen 3 Coder - Open source"
  "phi"         = "Microsoft Phi - Open source"
  "gemma3"      = "Google Gemma 3 - Open source"
}

# Cloud/subscription models (these require API keys or subscriptions)
$CloudModels = @{
  "glm-4.6:cloud"          = "GLM-4.6 Cloud - Requires Zhipu API subscription"
  "gpt-oss:20b-cloud"      = "GPT OSS Cloud - Requires cloud provider subscription"
  "gpt-oss:120b-cloud"     = "GPT OSS Cloud Large - Requires cloud provider subscription"
  "minimax-m2:cloud"       = "MiniMax M2 Cloud - Requires MiniMax API subscription"
  "qwen3-coder:480b-cloud" = "Qwen Coder Cloud - Requires Alibaba Cloud API"
  "qwen3-vl:235b-cloud"    = "Qwen Vision-Language Cloud - Requires Alibaba Cloud API"
}

# Get current models
Write-Host "🔍 Scanning Ollama models..." -ForegroundColor Yellow

try {
  $modelList = ollama list 2>$null
  if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Error: Ollama service not running or accessible" -ForegroundColor Red
    exit 1
  }
}
catch {
  Write-Host "❌ Error: Failed to get model list - $_" -ForegroundColor Red
  exit 1
}

# Parse model list (skip header)
$models = $modelList | Select-Object -Skip 1 | Where-Object { $_.Trim() -ne "" }

$analysisResults = @{
  Official = @()
  Cloud    = @()
  Custom   = @()
  Unknown  = @()
}

$totalSize = 0

Write-Host "`n📊 Model Analysis Results:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan

foreach ($line in $models) {
  # Parse the line - format: NAME ID SIZE MODIFIED
  $parts = $line.Split() | Where-Object { $_ -ne "" }
  if ($parts.Count -ge 4) {
    $modelName = $parts[0]
    $modelId = $parts[1]
    $modelSize = $parts[2]
    $modified = ($parts[3..($parts.Count - 1)] -join " ")
        
    # Convert size to GB for calculation
    $sizeInGB = 0
    if ($modelSize -match "(\d+\.?\d*)\s*GB") {
      $sizeInGB = [float]$matches[1]
      $totalSize += $sizeInGB
    }
    elseif ($modelSize -match "(\d+)\s*MB") {
      $sizeInGB = [float]$matches[1] / 1024
      $totalSize += $sizeInGB
    }
        
    # Determine model type
    $modelType = "Unknown"
    $description = ""
    $isCustom = $false
        
    # Check if it's a cloud model
    if ($modelName.EndsWith(":cloud")) {
      if ($CloudModels.ContainsKey($modelName)) {
        $modelType = "Cloud/Subscription"
        $description = $CloudModels[$modelName]
        $analysisResults.Cloud += @{
          Name                 = $modelName
          Size                 = $modelSize
          Description          = $description
          Modified             = $modified
          RequiresSubscription = $true
        }
      }
      else {
        $modelType = "Cloud/Unknown"
        $description = "Cloud model - likely requires subscription"
        $analysisResults.Unknown += @{
          Name                 = $modelName
          Size                 = $modelSize
          Description          = $description
          Modified             = $modified
          RequiresSubscription = $true
        }
      }
    }
    # Check if it's an official model
    else {
      $baseModel = ($modelName -split ":")[0]
      if ($OfficialModels.ContainsKey($baseModel)) {
        $modelType = "Official/Free"
        $description = $OfficialModels[$baseModel]
        $analysisResults.Official += @{
          Name                 = $modelName
          Size                 = $modelSize
          Description          = $description
          Modified             = $modified
          RequiresSubscription = $false
        }
      }
      # Custom models - those not in official list and not cloud
      else {
        $modelType = "Custom/Local"
        $isCustom = $true
                
        # Try to identify custom models by common naming patterns
        if ($modelName -match "bigdaddyg|cheetah-stealth") {
          $description = "Your custom fine-tuned model"
        }
        else {
          $description = "Custom or community model"
        }
                
        $analysisResults.Custom += @{
          Name                 = $modelName
          Size                 = $modelSize
          Description          = $description
          Modified             = $modified
          RequiresSubscription = $false
        }
      }
    }
        
    # Display result with color coding
    $color = switch ($modelType) {
      "Official/Free" { "Green" }
      "Cloud/Subscription" { "Red" }
      "Cloud/Unknown" { "Magenta" }
      "Custom/Local" { "Cyan" }
      default { "Yellow" }
    }
        
    $subscriptionIcon = if ($modelType.Contains("Cloud") -or $modelType.Contains("Subscription")) { " 💳" } else { "" }
    $customIcon = if ($isCustom) { " 🔧" } else { "" }
        
    Write-Host "  $modelName" -ForegroundColor $color -NoNewline
    Write-Host "$subscriptionIcon$customIcon" -ForegroundColor Yellow -NoNewline
    Write-Host " [$modelType] - $modelSize - $description" -ForegroundColor Gray
  }
}

# Summary report
Write-Host "`n📈 Summary Report:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan

Write-Host "`n🆓 Official/Free Models: $($analysisResults.Official.Count)" -ForegroundColor Green
foreach ($model in $analysisResults.Official) {
  Write-Host "   • $($model.Name) ($($model.Size))" -ForegroundColor Green
}

Write-Host "`n🔧 Your Custom Models: $($analysisResults.Custom.Count)" -ForegroundColor Cyan
foreach ($model in $analysisResults.Custom) {
  Write-Host "   • $($model.Name) ($($model.Size))" -ForegroundColor Cyan
}

Write-Host "`n💳 Cloud/Subscription Models: $($analysisResults.Cloud.Count)" -ForegroundColor Red
foreach ($model in $analysisResults.Cloud) {
  Write-Host "   • $($model.Name) ($($model.Size)) - REQUIRES SUBSCRIPTION" -ForegroundColor Red
}

if ($analysisResults.Unknown.Count -gt 0) {
  Write-Host "`n❓ Unknown/Unclassified Models: $($analysisResults.Unknown.Count)" -ForegroundColor Yellow
  foreach ($model in $analysisResults.Unknown) {
    Write-Host "   • $($model.Name) ($($model.Size))" -ForegroundColor Yellow
  }
}

# Storage analysis
Write-Host "`n💾 Storage Analysis:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Total Storage Used: $([math]::Round($totalSize, 2)) GB" -ForegroundColor White

$officialSize = ($analysisResults.Official | ForEach-Object {
    if ($_.Size -match "(\d+\.?\d*)\s*GB") { [float]$matches[1] }
    elseif ($_.Size -match "(\d+)\s*MB") { [float]$matches[1] / 1024 }
    else { 0 }
  } | Measure-Object -Sum).Sum

$customSize = ($analysisResults.Custom | ForEach-Object {
    if ($_.Size -match "(\d+\.?\d*)\s*GB") { [float]$matches[1] }
    elseif ($_.Size -match "(\d+)\s*MB") { [float]$matches[1] / 1024 }
    else { 0 }
  } | Measure-Object -Sum).Sum

Write-Host "  Official Models: $([math]::Round($officialSize, 2)) GB" -ForegroundColor Green
Write-Host "  Custom Models: $([math]::Round($customSize, 2)) GB" -ForegroundColor Cyan
Write-Host "  Cloud Models: 0 GB (remote)" -ForegroundColor Red

# Cost analysis
Write-Host "`n💰 Cost Analysis:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan

$freeModels = $analysisResults.Official.Count + $analysisResults.Custom.Count
$paidModels = $analysisResults.Cloud.Count

Write-Host "✅ Free Models: $freeModels (No ongoing costs)" -ForegroundColor Green
Write-Host "💳 Subscription Required: $paidModels models" -ForegroundColor Red

if ($paidModels -gt 0) {
  Write-Host "`n⚠️ Subscription Requirements:" -ForegroundColor Yellow
  Write-Host "   The following models require active subscriptions or API credits:"
  foreach ($model in $analysisResults.Cloud) {
    Write-Host "   • $($model.Name) - $($model.Description)" -ForegroundColor Red
  }
}

# Recommendations
Write-Host "`n💡 Recommendations:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan

if ($analysisResults.Custom.Count -gt 0) {
  Write-Host "✨ You have $($analysisResults.Custom.Count) custom models - these are your unique assets!" -ForegroundColor Cyan
}

if ($paidModels -gt $freeModels) {
  Write-Host "⚠️ You have more subscription models than free ones - consider costs" -ForegroundColor Yellow
}

if ($analysisResults.Official.Count -gt 0) {
  Write-Host "💡 Your free official models can replace subscription models for testing" -ForegroundColor Green
}

Write-Host "`n🔍 Use 'Test-Live-Ollama.ps1' to test connectivity to these models" -ForegroundColor Gray
Write-Host "🧪 Use 'Headless-Agentic-Test-CustomModels.ps1' to test model capabilities" -ForegroundColor Gray

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Model analysis complete! Check results above for detailed breakdown." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan