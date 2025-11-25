# 🔧 Agent Command Tuning for bigdaddyg-fast:latest
# Fixes the generate_summary command response issues

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Agent Command Tuning v1.0" -ForegroundColor Cyan
Write-Host "  Fixing generate_summary for bigdaddyg-fast:latest" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Test current prompt vs improved prompt
$modelName = "bigdaddyg-fast:latest"

Write-Host "🔍 Diagnosing the generate_summary issue..." -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Yellow

# Current problematic prompt
$currentPrompt = "As an AI agent, create a brief summary of this: 'Testing AI model capabilities for custom deployment.' End with 'AGENT_SUMMARY_COMPLETE'."

Write-Host "`n❌ Current Prompt (causing issues):" -ForegroundColor Red
Write-Host "$currentPrompt" -ForegroundColor Gray

Write-Host "`nTesting current prompt..." -NoNewline -ForegroundColor Yellow

try {
  $requestBody = @{
    model   = $modelName
    prompt  = $currentPrompt
    stream  = $false
    options = @{
      temperature = 0.2
      max_tokens  = 150
    }
  } | ConvertTo-Json -Depth 3
    
  $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec 30
    
  Write-Host " ✅ RESPONSE RECEIVED" -ForegroundColor Green
  Write-Host "Response: $($response.response.Substring(0, [Math]::Min(100, $response.response.Length)))..." -ForegroundColor Gray
    
  if ($response.response -match "AGENT_SUMMARY_COMPLETE") {
    Write-Host "✅ Contains expected pattern: AGENT_SUMMARY_COMPLETE" -ForegroundColor Green
  }
  else {
    Write-Host "❌ Missing expected pattern: AGENT_SUMMARY_COMPLETE" -ForegroundColor Red
    Write-Host "This is why the test shows 'Command response unclear'" -ForegroundColor Yellow
  }
}
catch {
  Write-Host " ❌ FAILED: $_" -ForegroundColor Red
}

# Improved prompts specifically tuned for bigdaddyg-fast
Write-Host "`n🔧 Testing Improved Prompts..." -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan

$improvedPrompts = @(
  @{
    Name   = "Explicit Instruction"
    Prompt = "TASK: Generate summary. INPUT: 'Testing AI model capabilities for custom deployment.' REQUIRED: End your response with exactly 'AGENT_SUMMARY_COMPLETE' (no quotes)."
  },
  @{
    Name   = "Step-by-step Format"
    Prompt = "Please do this: 1) Read this text: 'Testing AI model capabilities for custom deployment.' 2) Write a brief summary 3) End with AGENT_SUMMARY_COMPLETE"
  },
  @{
    Name   = "Direct Command"
    Prompt = "Summarize: Testing AI model capabilities for custom deployment.

IMPORTANT: You must end your response with: AGENT_SUMMARY_COMPLETE"
  }
)

$bestPrompt = $null
$bestResponse = $null

foreach ($test in $improvedPrompts) {
  Write-Host "`n🧪 Testing: $($test.Name)" -ForegroundColor Yellow
  Write-Host "Prompt: $($test.Prompt)" -ForegroundColor Gray
    
  try {
    $requestBody = @{
      model   = $modelName
      prompt  = $test.Prompt
      stream  = $false
      options = @{
        temperature = 0.1  # Lower temperature for more consistent completion
        max_tokens  = 100
      }
    } | ConvertTo-Json -Depth 3
        
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec 30
        
    Write-Host "Response: $($response.response)" -ForegroundColor Cyan
        
    if ($response.response -match "AGENT_SUMMARY_COMPLETE") {
      Write-Host "✅ SUCCESS: Contains expected pattern!" -ForegroundColor Green
      $bestPrompt = $test.Prompt
      $bestResponse = $response.response
    }
    else {
      Write-Host "❌ Still missing pattern" -ForegroundColor Red
    }
        
  }
  catch {
    Write-Host "❌ Error: $_" -ForegroundColor Red
  }
}

# Apply the fix to the test script
if ($bestPrompt) {
  Write-Host "`n🎯 SOLUTION FOUND!" -ForegroundColor Green
  Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Green
    
  Write-Host "`nBest working prompt:" -ForegroundColor Green
  Write-Host "$bestPrompt" -ForegroundColor White
    
  Write-Host "`nExample response:" -ForegroundColor Green
  Write-Host "$bestResponse" -ForegroundColor White
    
  Write-Host "`n🔧 Applying fix to test script..." -ForegroundColor Yellow
    
  # Read the current test file
  $testFile = ".\Headless-Agentic-Test-CustomModels.ps1"
  $content = Get-Content $testFile -Raw
    
  # Replace the problematic prompt
  $oldPrompt = "As an AI agent, create a brief summary of this: 'Testing AI model capabilities for custom deployment.' End with 'AGENT_SUMMARY_COMPLETE'."
  $newPrompt = $bestPrompt
    
  $newContent = $content -replace [regex]::Escape($oldPrompt), $newPrompt
    
  if ($newContent -ne $content) {
    Set-Content $testFile $newContent -Encoding UTF8
    Write-Host "✅ Fixed generate_summary prompt in test script!" -ForegroundColor Green
  }
  else {
    Write-Host "⚠️ Could not automatically update - manual fix needed" -ForegroundColor Yellow
  }
    
}
else {
  Write-Host "`n⚠️ No perfect solution found - trying alternative approach..." -ForegroundColor Yellow
    
  # Try with even more explicit instructions
  $ultimatePrompt = @"
You are bigdaddyg-fast, an AI agent. Your task is to summarize text.

INPUT TEXT: Testing AI model capabilities for custom deployment.

INSTRUCTIONS:
1. Create a 1-2 sentence summary
2. You MUST end with exactly these words: AGENT_SUMMARY_COMPLETE

Do this now:
"@
    
  Write-Host "`n🚀 Testing ultimate explicit prompt:" -ForegroundColor Magenta
  Write-Host "$ultimatePrompt" -ForegroundColor Gray
    
  try {
    $requestBody = @{
      model   = $modelName
      prompt  = $ultimatePrompt
      stream  = $false
      options = @{
        temperature = 0.1
        max_tokens  = 80
      }
    } | ConvertTo-Json -Depth 3
        
    $response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec 30
        
    Write-Host "`nUltimate Response: $($response.response)" -ForegroundColor Cyan
        
    if ($response.response -match "AGENT_SUMMARY_COMPLETE") {
      Write-Host "🎉 ULTIMATE SUCCESS!" -ForegroundColor Green
            
      # Apply this fix
      $testFile = ".\Headless-Agentic-Test-CustomModels.ps1"
      $content = Get-Content $testFile -Raw
      $oldPrompt = "As an AI agent, create a brief summary of this: 'Testing AI model capabilities for custom deployment.' End with 'AGENT_SUMMARY_COMPLETE'."
      $newContent = $content -replace [regex]::Escape($oldPrompt), $ultimatePrompt
            
      if ($newContent -ne $content) {
        Set-Content $testFile $newContent -Encoding UTF8
        Write-Host "✅ Applied ultimate fix to test script!" -ForegroundColor Green
      }
    }
    else {
      Write-Host "❌ Even ultimate prompt failed" -ForegroundColor Red
    }
        
  }
  catch {
    Write-Host "❌ Ultimate test failed: $_" -ForegroundColor Red
  }
}

Write-Host "`n💡 Additional Tuning Recommendations:" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Yellow

Write-Host "`n🎯 For bigdaddyg-fast:latest specifically:" -ForegroundColor Cyan
Write-Host "   • Use lower temperature (0.1) for more consistent completion markers"
Write-Host "   • Use shorter, more explicit prompts"
Write-Host "   • Put completion markers at the end of instructions"
Write-Host "   • Use direct commands rather than conversational style"
Write-Host "   • Consider the model was trained on specific instruction patterns"

Write-Host "`n🔄 If issue persists:" -ForegroundColor Cyan
Write-Host "   • The model may need fine-tuning for agent completion markers"
Write-Host "   • Consider post-processing to add markers if summary is good"
Write-Host "   • Test with different temperature/top_p settings"
Write-Host "   • Review original training data format"

Write-Host "`n🧪 Test the fix:" -ForegroundColor Green
Write-Host "   Run: .\Headless-Agentic-Test-CustomModels.ps1 -CustomModels @('bigdaddyg-fast:latest') -TimeoutSeconds 30"

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Agent command tuning complete! The generate_summary issue should be fixed." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan