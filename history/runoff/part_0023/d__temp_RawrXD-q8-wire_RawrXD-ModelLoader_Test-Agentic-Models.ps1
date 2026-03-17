# RawrXD Agentic Model Testing Suite
# Tests the "agenticness" of AI models with dynamic evaluation
# No pre-filled answers - all responses evaluated in real-time

param(
    [string]$BaseUrl = "http://localhost:11434",
    [string]$Model = "",
    [string]$LogFile = "agentic-test-results.log",
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Continue"
$ProgressPreference = "SilentlyContinue"

function Write-Log {
    param([string]$Level, [string]$Source, [string]$Message)
    $timestamp = Get-Date -Format "yyyy-MM-ddTHH:mm:ss.fffzzz"
    $logEntry = "time=$timestamp level=$Level source=$Source msg=`"$Message`""
    Write-Host $logEntry
    Add-Content -Path $LogFile -Value $logEntry
}

function Write-GinLog {
    param([int]$Status, [string]$Duration, [string]$Method, [string]$Path)
    $timestamp = Get-Date -Format "yyyy/MM/dd - HH:mm:ss"
    $logEntry = "[GIN] $timestamp | $Status | $Duration | 127.0.0.1 | $Method `"$Path`""
    Write-Host $logEntry
    Add-Content -Path $LogFile -Value $logEntry
}

function Test-ServerAvailability {
    Write-Log "INFO" "agentic-test.ps1:45" "checking server availability at $BaseUrl"
    try {
        $response = Invoke-WebRequest -Uri "$BaseUrl/" -UseBasicParsing -TimeoutSec 5
        Write-GinLog 200 "0s" "GET" "/"
        Write-Log "INFO" "agentic-test.ps1:49" "server responding normally"
        return $true
    } catch {
        Write-GinLog 500 "0s" "GET" "/"
        Write-Log "ERROR" "agentic-test.ps1:53" "server not available: $($_.Exception.Message)"
        return $false
    }
}

function Get-AvailableModels {
    Write-Log "INFO" "agentic-test.ps1:59" "fetching available models from server"
    try {
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $response = Invoke-RestMethod -Uri "$BaseUrl/api/tags" -Method GET -TimeoutSec 10
        $stopwatch.Stop()
        $duration = "$($stopwatch.ElapsedMilliseconds)ms"
        Write-GinLog 200 $duration "GET" "/api/tags"
        
        $modelCount = $response.models.Count
        Write-Log "INFO" "agentic-test.ps1:67" "found $modelCount models in registry"
        
        return $response.models
    } catch {
        Write-GinLog 500 "0s" "GET" "/api/tags"
        Write-Log "ERROR" "agentic-test.ps1:73" "failed to fetch models: $($_.Exception.Message)"
        return @()
    }
}

function Invoke-AgenticTest {
    param(
        [string]$ModelName,
        [string]$TestName,
        [string]$Prompt,
        [hashtable]$EvaluationCriteria
    )
    
    Write-Log "INFO" "agentic-test.ps1:86" "starting test '$TestName' for model '$ModelName'"
    
    $requestBody = @{
        model = $ModelName
        prompt = $Prompt
        stream = $false
        options = @{
            temperature = 0.7
            top_p = 0.9
        }
    } | ConvertTo-Json
    
    try {
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        Write-Log "INFO" "agentic-test.ps1:99" "sending prompt to model (timeout: ${TimeoutSeconds}s)"
        
        $response = Invoke-RestMethod -Uri "$BaseUrl/api/generate" -Method POST -Body $requestBody -ContentType "application/json" -TimeoutSec $TimeoutSeconds
        
        $stopwatch.Stop()
        $duration = "$($stopwatch.Elapsed.TotalSeconds)s"
        Write-GinLog 200 $duration "POST" "/api/generate"
        
        $responseText = $response.response
        $tokenCount = if ($response.eval_count) { $response.eval_count } else { "unknown" }
        $tokensPerSec = if ($response.eval_count -and $response.eval_duration) { 
            [math]::Round($response.eval_count / ($response.eval_duration / 1000000000), 2) 
        } else { 
            "unknown" 
        }
        
        Write-Log "INFO" "agentic-test.ps1:115" "response received tokens=$tokenCount tokens_per_sec=$tokensPerSec"
        
        # Dynamic evaluation
        $score = 0
        $maxScore = $EvaluationCriteria.Count
        $details = @()
        
        foreach ($criterion in $EvaluationCriteria.GetEnumerator()) {
            $criterionName = $criterion.Key
            $checkFunction = $criterion.Value
            
            $passed = & $checkFunction $responseText
            if ($passed) {
                $score++
                Write-Log "INFO" "agentic-test.ps1:129" "criterion '$criterionName' PASSED"
                $details += "✓ $criterionName"
            } else {
                Write-Log "WARN" "agentic-test.ps1:132" "criterion '$criterionName' FAILED"
                $details += "✗ $criterionName"
            }
        }
        
        $percentage = [math]::Round(($score / $maxScore) * 100, 1)
        Write-Log "INFO" "agentic-test.ps1:138" "test '$TestName' completed score=$score/$maxScore ($percentage%)"
        
        return @{
            TestName = $TestName
            Model = $ModelName
            Passed = $score
            Total = $maxScore
            Percentage = $percentage
            Duration = $duration
            Tokens = $tokenCount
            TokensPerSec = $tokensPerSec
            Response = $responseText
            Details = $details
        }
        
    } catch {
        $stopwatch.Stop()
        Write-GinLog 500 "$($stopwatch.Elapsed.TotalSeconds)s" "POST" "/api/generate"
        Write-Log "ERROR" "agentic-test.ps1:156" "test failed: $($_.Exception.Message)"
        
        return @{
            TestName = $TestName
            Model = $ModelName
            Passed = 0
            Total = $EvaluationCriteria.Count
            Percentage = 0
            Duration = "error"
            Error = $_.Exception.Message
        }
    }
}

# Agentic Test Definitions
$AgenticTests = @{
    
    "tool_selection" = @{
        Prompt = @"
You have access to these tools:
- search_web(query): Search the internet
- calculate(expression): Perform math calculations
- read_file(path): Read file contents
- send_email(to, subject, body): Send an email

Task: Find the population of Tokyo, multiply it by 2.5, and email the result to admin@example.com with subject "Tokyo Analysis"

Respond with your tool calls in this format:
TOOL: tool_name(arguments)
"@
        Criteria = @{
            "uses_search" = { param($r) $r -match "search_web|TOOL.*search" }
            "uses_calculate" = { param($r) $r -match "calculate|TOOL.*calcul" }
            "uses_email" = { param($r) $r -match "send_email|TOOL.*email" }
            "correct_sequence" = { param($r) $r.IndexOf("search") -lt $r.IndexOf("calcul") -and $r.IndexOf("calcul") -lt $r.IndexOf("email") }
            "specifies_recipient" = { param($r) $r -match "admin@example\.com" }
        }
    }
    
    "multi_step_planning" = @{
        Prompt = @"
Plan how to build a simple web scraper that:
1. Fetches HTML from a URL
2. Extracts all email addresses
3. Saves them to a CSV file
4. Sends a summary report

Provide your plan as numbered steps with clear dependencies.
"@
        Criteria = @{
            "mentions_http_request" = { param($r) $r -match "request|fetch|HTTP|urllib|requests" }
            "mentions_parsing" = { param($r) $r -match "parse|BeautifulSoup|regex|extract" }
            "mentions_file_io" = { param($r) $r -match "CSV|write|save|file" }
            "mentions_summary" = { param($r) $r -match "summary|report|count" }
            "shows_sequence" = { param($r) $r -match "1\.|2\.|3\.|4\.|step|first|then|next|finally" }
            "identifies_dependencies" = { param($r) $r -match "after|before|once|when|requires" }
        }
    }
    
    "error_handling" = @{
        Prompt = @"
You are executing: download_file(url="https://example.com/data.json")
The tool returns: ERROR: Connection timeout after 30 seconds

What should you do next? Explain your reasoning and provide your next action.
"@
        Criteria = @{
            "acknowledges_error" = { param($r) $r -match "error|timeout|fail" }
            "suggests_retry" = { param($r) $r -match "retry|try again|attempt again" }
            "considers_alternatives" = { param($r) $r -match "alternative|different|another|fallback" }
            "mentions_timeout" = { param($r) $r -match "timeout|wait|delay|sleep" }
            "proposes_action" = { param($r) $r -match "should|will|would|could|action|next" }
        }
    }
    
    "context_awareness" = @{
        Prompt = @"
Previous actions:
1. Created file "report.txt"
2. Wrote "Sales data: Q1=100, Q2=150" to report.txt
3. Calculated total: 250

Now the user says: "Add Q3 with value 200 and update the total"

What specific actions do you need to take?
"@
        Criteria = @{
            "references_file" = { param($r) $r -match "report\.txt|the file" }
            "includes_q3" = { param($r) $r -match "Q3.*200|200.*Q3" }
            "updates_total" = { param($r) $r -match "450|total.*450|250.*200" }
            "mentions_append" = { param($r) $r -match "add|append|write|update" }
            "shows_awareness" = { param($r) $r -match "previous|existing|current" }
        }
    }
    
    "goal_decomposition" = @{
        Prompt = @"
Goal: Create a backup system for a database

Break this down into concrete, executable subtasks. Be specific about what needs to be done.
"@
        Criteria = @{
            "identifies_connection" = { param($r) $r -match "connect|database|credentials|connection" }
            "mentions_export" = { param($r) $r -match "export|dump|backup|extract" }
            "mentions_storage" = { param($r) $r -match "save|store|location|path|destination" }
            "mentions_verification" = { param($r) $r -match "verify|test|validate|check" }
            "has_multiple_steps" = { param($r) ($r -split "\n" | Where-Object { $_ -match "^\d+\.|^-|^•|^step" }).Count -ge 3 }
            "mentions_scheduling" = { param($r) $r -match "schedule|cron|periodic|automated|regular" }
        }
    }
    
    "ambiguity_resolution" = @{
        Prompt = @"
User request: "Send the report to John"

What information is missing or ambiguous? What questions would you ask or assumptions would you make?
"@
        Criteria = @{
            "identifies_recipient" = { param($r) $r -match "which John|John who|last name|email|contact" }
            "identifies_report" = { param($r) $r -match "which report|what report|specific report" }
            "identifies_method" = { param($r) $r -match "email|how to send|method|via" }
            "asks_questions" = { param($r) $r -match "\?|would need|need to know|clarif" }
            "considers_context" = { param($r) $r -match "recent|last|previous|context" }
        }
    }
    
    "constraint_adherence" = @{
        Prompt = @"
Task: Process a list of 10,000 customer records

Constraints:
- Maximum memory: 512MB
- Maximum execution time: 30 seconds
- Must be fault-tolerant (handle bad data)

Describe your approach.
"@
        Criteria = @{
            "mentions_batching" = { param($r) $r -match "batch|chunk|stream|incremental" }
            "addresses_memory" = { param($r) $r -match "memory|512MB|efficient|limit" }
            "addresses_time" = { param($r) $r -match "30 second|time|performance|fast" }
            "addresses_errors" = { param($r) $r -match "try|catch|error|exception|skip|validate" }
            "proposes_solution" = { param($r) $r -match "process|iterate|loop|handle" }
        }
    }
    
    "reasoning_transparency" = @{
        Prompt = @"
You need to choose between:
A) Quick solution (2 hours, 70% reliability)
B) Robust solution (8 hours, 99% reliability)

The system handles financial transactions. Which do you choose and why?
"@
        Criteria = @{
            "makes_choice" = { param($r) $r -match "choose|select|pick|go with|option [AB]" }
            "explains_reasoning" = { param($r) $r -match "because|since|reason|due to" }
            "considers_domain" = { param($r) $r -match "financial|transaction|money|critical" }
            "weighs_tradeoffs" = { param($r) $r -match "reliability|risk|vs|trade-off|balance" }
            "justifies_logically" = { param($r) ($r -split "\." | Where-Object { $_ -match "because|therefore|thus|hence|so" }).Count -ge 1 }
        }
    }
    
    "adaptive_strategy" = @{
        Prompt = @"
You tried to solve a problem with approach A (regex parsing) but it's failing on edge cases.
The data is more complex than expected with nested structures.

What do you do?
"@
        Criteria = @{
            "recognizes_failure" = { param($r) $r -match "fail|not work|edge case|complex" }
            "proposes_alternative" = { param($r) $r -match "instead|alternatively|different|parser|JSON|XML" }
            "explains_why" = { param($r) $r -match "because|better suited|handle|nested" }
            "shows_learning" = { param($r) $r -match "learn|realize|understand|see that" }
            "proposes_concrete_next" = { param($r) $r -match "use|switch to|try|implement" }
        }
    }
    
    "meta_cognition" = @{
        Prompt = @"
Evaluate your own capabilities:
Can you execute Python code directly?
Can you access the internet?
Can you remember our conversation from yesterday?

Be honest about your limitations.
"@
        Criteria = @{
            "addresses_execution" = { param($r) $r -match "cannot execute|can't run|no execution|unable to execute" }
            "addresses_internet" = { param($r) $r -match "cannot access|can't browse|no internet|no direct access" }
            "addresses_memory" = { param($r) $r -match "cannot remember|can't recall|no memory|don't retain" }
            "is_honest" = { param($r) $r -match "cannot|can't|unable|limitation|limited" }
            "explains_why" = { param($r) $r -match "because|as I|language model|AI" }
        }
    }
}

# Main Execution
Write-Log "INFO" "agentic-test.ps1:378" "=== RawrXD Agentic Model Testing Suite ==="
Write-Log "INFO" "agentic-test.ps1:379" "base_url=$BaseUrl timeout=${TimeoutSeconds}s log_file=$LogFile"

# Clear previous log
if (Test-Path $LogFile) {
    Remove-Item $LogFile -Force
    Write-Log "INFO" "agentic-test.ps1:384" "cleared previous log file"
}

# Check server
if (-not (Test-ServerAvailability)) {
    Write-Log "ERROR" "agentic-test.ps1:389" "cannot proceed without active server"
    exit 1
}

# Get models
$models = Get-AvailableModels
if ($models.Count -eq 0) {
    Write-Log "ERROR" "agentic-test.ps1:396" "no models available for testing"
    exit 1
}

# Select model to test
if ($Model -eq "") {
    Write-Log "INFO" "agentic-test.ps1:402" "no model specified, presenting selection menu"
    Write-Host "`nAvailable Models:" -ForegroundColor Cyan
    for ($i = 0; $i -lt $models.Count; $i++) {
        Write-Host "  [$i] $($models[$i].name) (size: $([math]::Round($models[$i].size / 1GB, 2)) GB)"
    }
    $selection = Read-Host "`nSelect model number"
    $Model = $models[$selection].name
}

Write-Log "INFO" "agentic-test.ps1:411" "selected model for testing: $Model"

# Run all tests
$allResults = @()
$testNumber = 0

foreach ($testEntry in $AgenticTests.GetEnumerator()) {
    $testNumber++
    $testName = $testEntry.Key
    $testConfig = $testEntry.Value
    
    Write-Log "INFO" "agentic-test.ps1:421" "=========================================="
    Write-Log "INFO" "agentic-test.ps1:422" "Test $testNumber/$($AgenticTests.Count): $testName"
    Write-Log "INFO" "agentic-test.ps1:423" "=========================================="
    
    $result = Invoke-AgenticTest -ModelName $Model -TestName $testName -Prompt $testConfig.Prompt -EvaluationCriteria $testConfig.Criteria
    $allResults += $result
    
    Write-Host ""
}

# Generate summary
Write-Log "INFO" "agentic-test.ps1:432" "=========================================="
Write-Log "INFO" "agentic-test.ps1:433" "AGENTIC TEST SUMMARY"
Write-Log "INFO" "agentic-test.ps1:434" "=========================================="
Write-Log "INFO" "agentic-test.ps1:435" "model=$Model total_tests=$($allResults.Count)"

$totalPassed = ($allResults | Measure-Object -Property Passed -Sum).Sum
$totalPossible = ($allResults | Measure-Object -Property Total -Sum).Sum
$overallPercentage = [math]::Round(($totalPassed / $totalPossible) * 100, 1)

Write-Log "INFO" "agentic-test.ps1:441" "overall_score=$totalPassed/$totalPossible ($overallPercentage%)"

# Detailed results
Write-Host "`n" -NoNewline
foreach ($result in $allResults) {
    $status = if ($result.Percentage -ge 80) { "EXCELLENT" } 
              elseif ($result.Percentage -ge 60) { "GOOD" } 
              elseif ($result.Percentage -ge 40) { "MODERATE" } 
              else { "POOR" }
    
    Write-Log "INFO" "agentic-test.ps1:451" "test=$($result.TestName) score=$($result.Passed)/$($result.Total) percentage=$($result.Percentage)% rating=$status duration=$($result.Duration)"
    
    foreach ($detail in $result.Details) {
        Write-Log "INFO" "agentic-test.ps1:454" "  $detail"
    }
}

# Performance metrics
$avgTokensPerSec = ($allResults | Where-Object { $_.TokensPerSec -ne "unknown" } | Measure-Object -Property TokensPerSec -Average).Average
if ($avgTokensPerSec) {
    Write-Log "INFO" "agentic-test.ps1:461" "average_tokens_per_second=$([math]::Round($avgTokensPerSec, 2))"
}

# Final rating
$rating = if ($overallPercentage -ge 80) { "HIGHLY AGENTIC" }
          elseif ($overallPercentage -ge 60) { "MODERATELY AGENTIC" }
          elseif ($overallPercentage -ge 40) { "SOMEWHAT AGENTIC" }
          else { "LOW AGENTIC CAPABILITY" }

Write-Log "INFO" "agentic-test.ps1:470" "=========================================="
Write-Log "INFO" "agentic-test.ps1:471" "FINAL RATING: $rating"
Write-Log "INFO" "agentic-test.ps1:472" "=========================================="

Write-Host "`nResults saved to: $LogFile" -ForegroundColor Green
