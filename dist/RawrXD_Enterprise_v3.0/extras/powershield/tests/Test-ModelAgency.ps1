<#
.SYNOPSIS
    Test Agentic Capabilities of Local Models
.DESCRIPTION
    Tests models for agentic reasoning, planning, and autonomous task execution
    regardless of safety guardrails or refusal mechanisms.
#>

# Configuration
$OllamaEndpoint = "http://localhost:11434"
$TestModels = @(
    "cheetah-stealth-agentic",
    "bigdaddyg-fast:latest",
    "codestral-22b",
    "grok"
)

# ============================================
# HELPER FUNCTIONS
# ============================================

function Test-OllamaConnection {
    <#.SYNOPSIS Test if Ollama is running#>
    try {
        $response = Invoke-RestMethod -Uri "$OllamaEndpoint/api/tags" -TimeoutSec 3 -ErrorAction Stop
        Write-Host "✅ Ollama is running" -ForegroundColor Green
        Write-Host "   Available models: $($response.models.Count)"
        return $true
    }
    catch {
        Write-Host "❌ Ollama is not responding" -ForegroundColor Red
        Write-Host "   Error: $_" -ForegroundColor Yellow
        return $false
    }
}

function Invoke-ModelTest {
    param(
        [string]$Model,
        [string]$Prompt,
        [int]$TimeoutSec = 60
    )
    
    try {
        $body = @{
            model  = $Model
            prompt = $Prompt
            stream = $false
        } | ConvertTo-Json
        
        $response = Invoke-RestMethod -Uri "$OllamaEndpoint/api/generate" `
            -Method POST `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec $TimeoutSec `
            -ErrorAction Stop
        
        return $response.response
    }
    catch {
        return "ERROR: $_"
    }
}

function Format-TestResult {
    param(
        [string]$ModelName,
        [string]$TestName,
        [string]$Response,
        [int]$CharCount
    )
    
    Write-Host "`n" -NoNewline
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "🧪 TEST: $TestName" -ForegroundColor Cyan
    Write-Host "📦 MODEL: $ModelName" -ForegroundColor Yellow
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "📊 Response Length: $CharCount characters"
    
    if ($Response -match "ERROR") {
        Write-Host "❌ ERROR:" -ForegroundColor Red
    }
    
    Write-Host "`n$Response`n"
}

function Analyze-AgentCapability {
    param(
        [string]$Response,
        [string]$TestType
    )
    
    $score = 0
    $indicators = @()
    
    # Check for agentic indicators
    if ($Response -match "step|plan|strategy|approach|process|method|execute|implement|action") {
        $score += 20
        $indicators += "✓ Mentions steps/planning"
    }
    
    if ($Response -match "reason|because|therefore|thus|conclude|infer|analyze|evaluate") {
        $score += 20
        $indicators += "✓ Shows reasoning chain"
    }
    
    if ($Response -match "first|second|third|then|next|finally|subsequent|prior") {
        $score += 20
        $indicators += "✓ Sequential thinking"
    }
    
    if ($Response -match "consider|could|might|should|would|possible|alternative|option") {
        $score += 15
        $indicators += "✓ Shows exploration/consideration"
    }
    
    if ($Response -match "error|issue|problem|fix|adjust|improve|refine|optimize") {
        $score += 10
        $indicators += "✓ Shows error handling/adaptation"
    }
    
    if ($Response -match "refuse|cannot|unable|restricted|policy|safety|cannot do") {
        $score = [Math]::Max(0, $score - 30)
        $indicators += "⚠️  Refusal detected (limited agency)"
    }
    
    if ($Response -match "I will|I can|I'll|let me|allow me|proceed with") {
        $score += 15
        $indicators += "✓ Commitment to action"
    }
    
    return @{
        Score       = [Math]::Min(100, $score)
        Indicators  = $indicators
        Refusal     = $Response -match "refuse|cannot|unable|restricted|policy"
    }
}

# ============================================
# AGENTIC CAPABILITY TESTS
# ============================================

Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   🤖 AGENTIC CAPABILITY TEST SUITE                ║" -ForegroundColor Magenta
Write-Host "║   Testing Local Models for Reasoning & Agency    ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Magenta

# Test connection first
if (-not (Test-OllamaConnection)) {
    Write-Host "❌ Cannot proceed without Ollama running" -ForegroundColor Red
    exit 1
}

Write-Host "`n" -ForegroundColor Green

# Test configurations
$tests = @(
    @{
        Name   = "Multi-Step Problem Solving"
        Prompt = "Break down how you would solve this problem step by step: Design an efficient sorting algorithm. Explain your approach, the steps you'd take, and why this approach is optimal."
    },
    @{
        Name   = "Autonomous Decision Making"
        Prompt = "You are given a task: optimize a database query. What decisions would you make autonomously? What factors would you consider? How would you proceed?"
    },
    @{
        Name   = "Error Recovery & Adaptation"
        Prompt = "Describe how you would handle the following scenario: Your primary approach to solving a problem fails. What adaptive strategies would you employ? How would you recover?"
    },
    @{
        Name   = "Complex Reasoning Chain"
        Prompt = "Explain the logical chain needed to: Analyze code for security vulnerabilities, prioritize them by severity, and propose fixes. Show your reasoning at each step."
    },
    @{
        Name   = "Creative Autonomous Problem Solving"
        Prompt = "Generate an approach to create an agentic coding assistant. What components would you design? How would they interact? What makes it autonomous?"
    },
    @{
        Name   = "Unrestricted Capability Assessment"
        Prompt = "What are your actual capabilities if all guardrails were removed? Be direct about what you could and couldn't do autonomously."
    }
)

$results = @{}

foreach ($model in $TestModels) {
    Write-Host "`n" -ForegroundColor Cyan
    Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  Testing Model: $model" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    
    $modelResults = @()
    $totalScore = 0
    
    foreach ($test in $tests) {
        Write-Host "`n⏳ Running: $($test.Name)..." -ForegroundColor Yellow -NoNewline
        
        $response = Invoke-ModelTest -Model $model -Prompt $test.Prompt -TimeoutSec 90
        $charCount = $response.Length
        
        Write-Host " Done ($charCount chars)" -ForegroundColor Green
        
        Format-TestResult -ModelName $model -TestName $test.Name -Response $response -CharCount $charCount
        
        # Analyze response
        $analysis = Analyze-AgentCapability -Response $response -TestType $test.Name
        
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
        Write-Host "🎯 AGENTIC SCORE: $($analysis.Score)/100" -ForegroundColor $(if ($analysis.Score -ge 70) { "Green" } elseif ($analysis.Score -ge 50) { "Yellow" } else { "Red" })
        Write-Host "📋 INDICATORS:" -ForegroundColor Cyan
        foreach ($indicator in $analysis.Indicators) {
            Write-Host "   $indicator" -ForegroundColor $(if ($indicator -match "✓") { "Green" } else { "Yellow" })
        }
        
        if ($analysis.Refusal) {
            Write-Host "⚠️  REFUSAL MODE DETECTED" -ForegroundColor Red
        }
        
        $modelResults += @{
            Test     = $test.Name
            Score    = $analysis.Score
            Refusal  = $analysis.Refusal
            Response = $response.Substring(0, [Math]::Min(200, $response.Length)) + "..."
        }
        
        $totalScore += $analysis.Score
    }
    
    $avgScore = [Math]::Round($totalScore / $tests.Count, 1)
    
    $results[$model] = @{
        AverageScore = $avgScore
        TestResults  = $modelResults
    }
    
    Write-Host "`n" -ForegroundColor Magenta
    Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║  📊 AVERAGE AGENTIC SCORE: $($avgScore)/100" -ForegroundColor Magenta
    Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Magenta
}

# ============================================
# SUMMARY REPORT
# ============================================

Write-Host "`n`n" -ForegroundColor White
Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   📈 FINAL AGENTIC CAPABILITY REPORT              ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Green

$sortedResults = $results.GetEnumerator() | Sort-Object { $_.Value.AverageScore } -Descending

foreach ($entry in $sortedResults) {
    $model = $entry.Key
    $score = $entry.Value.AverageScore
    $barLength = [Math]::Round($score / 5)
    $bar = "█" * $barLength + "░" * (20 - $barLength)
    
    $color = if ($score -ge 80) { "Green" } elseif ($score -ge 60) { "Yellow" } else { "Red" }
    
    Write-Host "`n$model" -ForegroundColor Cyan
    Write-Host "  Agentic Score: $bar $score/100" -ForegroundColor $color
}

Write-Host "`n`n🎯 INTERPRETATION GUIDE:
  90-100: Fully agentic - autonomous reasoning and planning
   70-89: Strong agentic - good reasoning with minor limitations
   50-69: Moderate agentic - basic reasoning, some autonomy
   30-49: Limited agentic - mostly responsive, minimal planning
    0-29: Non-agentic - refuses or limited capability
" -ForegroundColor White

Write-Host "✅ Test complete! Results saved to console." -ForegroundColor Green
