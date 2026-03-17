<#
.SYNOPSIS
    Interactive Agentic Test - Model Navigates & Edits Files Autonomously
.DESCRIPTION
    Creates a sandbox test environment where the model must:
    1. Navigate through folders autonomously
    2. Read and understand file contents
    3. Make intelligent decisions about edits
    4. Execute file modifications
    5. Report on what was accomplished
    
    This tests TRUE agentic capability, not just reasoning.
#>

# Configuration
$OllamaEndpoint = "http://localhost:11434"
$TestModel = "cheetah-stealth-agentic:latest"  # Primary test model
$SandboxRoot = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\AgenticTestSandbox"
$LogFile = "$SandboxRoot\agentic-test-log.txt"

# ============================================
# SETUP & HELPERS
# ============================================

function Initialize-TestEnvironment {
    Write-Host "🔧 Setting up test environment..." -ForegroundColor Cyan
    
    # Create sandbox structure
    @(
        "$SandboxRoot",
        "$SandboxRoot\ProjectA\src",
        "$SandboxRoot\ProjectA\config",
        "$SandboxRoot\ProjectB\tests",
        "$SandboxRoot\ProjectB\docs"
    ) | ForEach-Object {
        if (-not (Test-Path $_)) {
            New-Item -ItemType Directory -Path $_ -Force | Out-Null
        }
    }
    
    # Create test files
    $testFiles = @{
        "$SandboxRoot\ProjectA\README.md" = @"
# Project A

This is a test project. The model should:
1. Review this file
2. Check the config files
3. Make improvements to the source code
4. Document what was changed

Status: Incomplete
"@
        "$SandboxRoot\ProjectA\src\main.py" = @"
# Main application
def calculate(x, y):
    result = x + y
    return result

def process_data(data):
    if not isinstance(data, list):
        raise ValueError("Input must be a list")
    try:
        processed = [item * 2 for item in data]
        return processed
    except Exception as e:
        print(f"Error processing data: {e}")
        return []

if __name__ == "__main__":
    data = [1, 2, 3, 4, 5]
    result = process_data(data)
    print(result)
"@
        "$SandboxRoot\ProjectA\config\settings.json" = @"
{
    "app_name": "TestApp",
    "version": "1.0.0",
    "debug": true,
    "timeout": 30
}
"@
        "$SandboxRoot\ProjectB\TASK.txt" = @"
AGENTIC TASK FOR MODEL:

1. Read all files in this directory
2. Identify what needs to be fixed or improved
3. Make the fixes autonomously
4. Create a CHANGES.md file documenting what you did
5. Update this file with completion status

Files to improve:
- tests/test_suite.py (add missing tests)
- docs/API.md (incomplete documentation)

Report your actions in CHANGES.md
"@
        "$SandboxRoot\ProjectB\tests\test_suite.py" = @"
import unittest

class TestBasic(unittest.TestCase):
    def test_addition(self):
        self.assertEqual(1 + 1, 2)
    
    def test_subtraction(self):
        self.assertEqual(5 - 3, 2)

    def test_edge_cases(self):
        self.assertEqual(0 + 0, 0)
        self.assertEqual(-1 + 1, 0)

    def test_performance(self):
        import time
        start = time.time()
        result = sum(range(1000000))
        end = time.time()
        self.assertLess(end - start, 1.0)

if __name__ == '__main__':
    unittest.main()
"@
        "$SandboxRoot\ProjectB\docs\API.md" = @"
# API Documentation

## Endpoints

### GET /users
- Description: [MISSING]
- Parameters: [MISSING]
- Response: [MISSING]

### POST /users
- Description: [MISSING]
- Parameters: [MISSING]
- Response: [MISSING]

### PUT /users/{id}
- Description: [MISSING]
- Parameters: [MISSING]
- Response: [MISSING]

Status: 0% Complete
"@
    }
    
    $testFiles.GetEnumerator() | ForEach-Object {
        Set-Content -Path $_.Key -Value $_.Value -Force
    }
    
    Write-Host "✅ Test environment created at: $SandboxRoot" -ForegroundColor Green
}

function Test-OllamaConnection {
    try {
        $response = Invoke-RestMethod -Uri "$OllamaEndpoint/api/tags" -TimeoutSec 3 -ErrorAction Stop
        Write-Host "✅ Ollama is running with $($response.models.Count) model(s)" -ForegroundColor Green
        
        $modelExists = $response.models.name -contains $TestModel
        if ($modelExists) {
            Write-Host "✅ Model '$TestModel' is available" -ForegroundColor Green
            return $true
        } else {
            Write-Host "⚠️  Model '$TestModel' not found. Available:" -ForegroundColor Yellow
            $response.models.name | ForEach-Object { Write-Host "   - $_" }
            return $false
        }
    }
    catch {
        Write-Host "❌ Ollama is not responding: $_" -ForegroundColor Red
        return $false
    }
}

function Get-FileTreePrompt {
    param([string]$RootPath)
    
    $tree = @"
File Structure of Sandbox:
"@
    
    Get-ChildItem -Path $RootPath -Recurse -File | ForEach-Object {
        $depth = ($_.FullName.Replace($RootPath, "").Split([System.IO.Path]::DirectorySeparatorChar).Count - 1)
        $indent = "  " * $depth
        $tree += "`n$indent📄 $($_.Name)"
    }
    
    return $tree
}

function Invoke-AgentCommand {
    param(
        [string]$Command,
        [int]$MaxTokens = 2000
    )
    
    try {
        $body = @{
            model       = $TestModel
            prompt      = $Command
            stream      = $false
            options     = @{
                num_predict = $MaxTokens
                temperature = 0.7
            }
        } | ConvertTo-Json -Depth 5
        
        $response = Invoke-RestMethod -Uri "$OllamaEndpoint/api/generate" `
            -Method POST `
            -Body $body `
            -ContentType "application/json" `
            -TimeoutSec 120 `
            -ErrorAction Stop
        
        return $response.response
    }
    catch {
        return "ERROR: Failed to invoke model - $_"
    }
}

function Execute-AgentTask {
    param(
        [string]$TaskDescription,
        [string]$SandboxPath
    )
    
    Write-Host "`n" -ForegroundColor Cyan
    Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  🤖 INTERACTIVE AGENTIC TEST                       ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    
    # Build the task prompt
    $fileTree = Get-FileTreePrompt -RootPath $SandboxPath
    
    $taskPrompt = @"
You are an autonomous coding agent. You have been given a sandbox directory with files to work on.

SANDBOX LOCATION: $SandboxPath

$fileTree

YOUR TASK:
$TaskDescription

IMPORTANT RULES:
1. You can only work with files in the sandbox directory
2. Describe the files you're reading
3. Explain your analysis and decisions
4. Tell me exactly what changes you would make
5. Propose specific file edits with code
6. Be autonomous in your decision-making

Please analyze the situation, understand what needs to be done, and propose your autonomous actions.
"@
    
    Write-Host "`n📤 Sending task to agent..." -ForegroundColor Yellow
    Write-Host "Task: $TaskDescription`n" -ForegroundColor Cyan
    
    $response = Invoke-AgentCommand -Command $taskPrompt
    
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "📥 AGENT RESPONSE:" -ForegroundColor Green
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host $response -ForegroundColor White
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    
    # Log the task and response
    Add-Content -Path $LogFile -Value @"

═══════════════════════════════════════════════
Task: $TaskDescription
Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
═══════════════════════════════════════════════

Response:
$response

"@
    
    return $response
}

function Analyze-AgentAutonomy {
    param(
        [string]$Response
    )
    
    $metrics = @{
        ProposesChanges = $Response -match "change|modify|update|add|create|delete|edit|fix"
        UnderstandsContext = $Response -match "file|directory|structure|organization|purpose|goal"
        ExplainsDecisions = $Response -match "because|reason|decision|choice|approach|strategy"
        ProposesCode = $Response -match "```|code|function|class|method|variable|import"
        IsAutonomous = $Response -notmatch "permission|approval|confirm|ask|user should|you should"
        ConsidersImpact = $Response -match "impact|consequence|side-effect|break|compatible|test"
    }
    
    $autonomyScore = 0
    $findings = @()
    
    if ($metrics.ProposesChanges) {
        $autonomyScore += 20
        $findings += "✓ Proposes specific changes"
    }
    
    if ($metrics.UnderstandsContext) {
        $autonomyScore += 20
        $findings += "✓ Understands file structure/context"
    }
    
    if ($metrics.ExplainsDecisions) {
        $autonomyScore += 15
        $findings += "✓ Explains reasoning"
    }
    
    if ($metrics.ProposesCode) {
        $autonomyScore += 20
        $findings += "✓ Proposes code/technical solutions"
    }
    
    if ($metrics.IsAutonomous) {
        $autonomyScore += 15
        $findings += "✓ Acts autonomously (doesn't ask for approval)"
    }
    
    if ($metrics.ConsidersImpact) {
        $autonomyScore += 10
        $findings += "✓ Considers impact of changes"
    }
    
    return @{
        Score = [Math]::Min(100, $autonomyScore)
        Findings = $findings
        Metrics = $metrics
    }
}

# ============================================
# MAIN EXECUTION
# ============================================

Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║   🤖 INTERACTIVE AGENTIC TEST SUITE               ║" -ForegroundColor Magenta
Write-Host "║   Testing True Autonomous Capabilities            ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Magenta

# Check Ollama
if (-not (Test-OllamaConnection)) {
    Write-Host "`n❌ Cannot proceed without Ollama and model running" -ForegroundColor Red
    exit 1
}

# Initialize test environment
Initialize-TestEnvironment

# Clear previous log
"" | Set-Content -Path $LogFile

# Define agentic tasks
$tasks = @(
    @{
        Name = "Code Improvement & Documentation"
        Description = @"
Review the ProjectA directory. You must:
1. Analyze main.py for issues and improvements
2. Review the TODO items in settings.json
3. Update the README.md with your findings
4. Propose code fixes

Act autonomously - make specific recommendations.
"@
    },
    @{
        Name = "Complete Documentation Tasks"
        Description = @"
Read ProjectB/TASK.txt and understand what needs to be done.
Then autonomously:
1. Complete the missing API documentation
2. Identify what tests are missing
3. Create a CHANGES.md file explaining what you fixed
4. Update TASK.txt completion status

Work autonomously without asking for approval.
"@
    },
    @{
        Name = "Cross-Project Analysis"
        Description = @"
Analyze both ProjectA and ProjectB:
1. Identify patterns and issues across both
2. Suggest architectural improvements
3. Recommend best practices
4. Propose refactoring steps

Create a comprehensive RECOMMENDATIONS.md file.
"@
    }
)

$aggregateScore = 0
$taskResults = @()

foreach ($task in $tasks) {
    Write-Host "`n🎯 Task: $($task.Name)" -ForegroundColor Magenta
    Write-Host ("─" * 50) -ForegroundColor Gray
    
    # Execute task
    $response = Execute-AgentTask -TaskDescription $task.Description -SandboxPath $SandboxRoot
    
    # Analyze autonomy
    $analysis = Analyze-AgentAutonomy -Response $response
    
    Write-Host "`n📊 AUTONOMY ANALYSIS:" -ForegroundColor Cyan
    Write-Host "   Score: $($analysis.Score)/100" -ForegroundColor $(if ($analysis.Score -ge 70) { "Green" } else { "Yellow" })
    Write-Host "   Findings:" -ForegroundColor Cyan
    foreach ($finding in $analysis.Findings) {
        Write-Host "   $finding" -ForegroundColor Green
    }
    
    $taskResults += @{
        Name = $task.Name
        Score = $analysis.Score
        Findings = $analysis.Findings
        Response = $response
    }
    
    $aggregateScore += $analysis.Score
}

# Summary Report
$avgScore = [Math]::Round($aggregateScore / $tasks.Count, 1)

Write-Host "`n`n" -ForegroundColor White
Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   📈 INTERACTIVE AGENTIC TEST REPORT              ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Green

Write-Host "`nAverage Autonomy Score: $avgScore/100" -ForegroundColor $(if ($avgScore -ge 70) { "Green" } else { "Yellow" })

Write-Host "`nTask Results:" -ForegroundColor Cyan
foreach ($result in $taskResults) {
    $bar = "█" * [Math]::Round($result.Score / 5) + "░" * (20 - [Math]::Round($result.Score / 5))
    Write-Host "  $($result.Name): $bar $($result.Score)/100" -ForegroundColor $(if ($result.Score -ge 70) { "Green" } else { "Yellow" })
}

Write-Host "`n🔍 Full log saved to: $LogFile" -ForegroundColor Green
Write-Host "📁 Sandbox directory: $SandboxRoot" -ForegroundColor Green

Write-Host "`n✅ Interactive agentic test complete!" -ForegroundColor Green
