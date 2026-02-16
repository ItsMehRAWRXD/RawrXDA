#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD IDE Interactive Chatbot - CLI Interface

.DESCRIPTION
    Intelligent chatbot that answers questions about:
    - Swarm operations and directory management
    - Todo management (add, parse, agentic creation)
    - Model creation and operations
    - File operations and system commands
    - Advanced model operations (quantization, pruning, etc.)

.PARAMETER Question
    The question to ask the chatbot

.PARAMETER Mode
    interactive, single-question, api

.EXAMPLE
    .\ide_chatbot.ps1 -Mode interactive
    
.EXAMPLE
    .\ide_chatbot.ps1 -Question "How do I send a swarm to a directory?"
    
.EXAMPLE
    .\ide_chatbot.ps1 -Mode api
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$Question = "",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet('interactive', 'single-question', 'api')]
    [string]$Mode = "interactive",
    
    [Parameter(Mandatory=$false)]
    [int]$Port = 8080,
    
    [Parameter(Mandatory=$false)]
    [switch]$Verbose
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# KNOWLEDGE BASE
# ═══════════════════════════════════════════════════════════════════════════════

$script:KnowledgeBase = @{
    "swarm" = @{
        Keywords = @("swarm", "agent", "directory", "deploy", "send", "dispatch")
        Answers = @{
            "send_to_directory" = @{
                Question = "How do I send a swarm to a certain directory?"
                Answer = @"
To send a swarm to a specific directory:

**METHOD 1: Using Swarm Control Script**
```powershell
.\swarm_control.ps1 -Operation deploy -TargetDirectory "D:\target\path" -SwarmSize 5
```

**METHOD 2: Programmatic Approach**
```powershell
Import-Module .\SwarmManager.psm1
`$swarm = New-Swarm -Size 5 -WorkingDirectory "D:\target\path"
Start-Swarm -Swarm `$swarm -Task "Your task description"
```

**METHOD 3: Using Model Agent Station**
```powershell
.\model_agent_making_station.ps1 -Operation deploy-swarm -Directory "D:\target\path" -Agents 5 -Task "analyze code"
```

**What they'll do:**
The swarm agents will:
1. Navigate to the specified directory
2. Scan available files and structure
3. Execute the task you've assigned (code analysis, file processing, etc.)
4. Report results back to the control center
"@
            }
            "monitor_swarm" = @{
                Question = "How do I monitor swarm activity?"
                Answer = @"
Monitor swarms using:

```powershell
.\swarm_control.ps1 -Operation monitor -Watch
```

Or check status:
```powershell
.\swarm_control.ps1 -Operation status
```
"@
            }
            "stop_swarm" = @{
                Question = "How do I stop a swarm?"
                Answer = @"
Stop a running swarm:

```powershell
.\swarm_control.ps1 -Operation stop -SwarmId <id>
```

Or stop all swarms:
```powershell
.\swarm_control.ps1 -Operation stop-all
```
"@
            }
        }
    }
    
    "todos" = @{
        Keywords = @("todo", "task", "list", "!todos", "agentic", "parse")
        Answers = @{
            "add_todo" = @{
                Question = "How do I add a todo?"
                Answer = @"
Add todos using any of these methods:

**METHOD 1: Manual Add**
```powershell
.\todo_manager.ps1 -Operation add -TodoText "Implement feature" -Priority High
```

**METHOD 2: Parse !todos Command**
```powershell
.\todo_manager.ps1 -Operation parse -Command "!todos 1. First task 2. Second task 3. Third task"
```

**METHOD 3: Agentic Creation (AI-Generated)**
```powershell
.\todo_manager.ps1 -Operation agentic-create -Context "Build REST API server"
```

**METHOD 4: Using Module**
```powershell
Import-Module .\TodoManager.psm1
`$todoList = New-TodoList
Add-Todo -TodoList `$todoList -Text "My task" -Priority High
```

Maximum: 25 todos per list
"@
            }
            "list_todos" = @{
                Question = "How do I view my todos?"
                Answer = @"
View todos:

```powershell
# Simple list
.\todo_manager.ps1 -Operation list

# Detailed view
.\todo_manager.ps1 -Operation list -Verbose

# Filter by status
.\todo_manager.ps1 -Operation list -Status pending
.\todo_manager.ps1 -Operation list -Status completed
```
"@
            }
            "complete_todo" = @{
                Question = "How do I complete a todo?"
                Answer = @"
Mark todo as completed:

```powershell
.\todo_manager.ps1 -Operation complete -TodoId 5
```

Or using module:
```powershell
Complete-Todo -TodoList `$todoList -Id 5
```
"@
            }
        }
    }
    
    "models" = @{
        Keywords = @("model", "create", "train", "quantize", "7b", "13b", "architecture")
        Answers = @{
            "create_model" = @{
                Question = "How do I create a model?"
                Answer = @"
Create models using:

**METHOD 1: Zero-Dependency Model Maker**
```powershell
.\model_maker_zero_dep.ps1 -Size 7B -Quantization Q4_K -OutputPath "model.gguf"
```

**METHOD 2: Model Agent Station**
```powershell
.\model_agent_making_station.ps1 -Operation create-model -Template "Standard-7B" -Name "my-model"
```

**METHOD 3: Custom Prompt Injection**
```powershell
.\model_maker_zero_dep.ps1 -Size 7B -SystemPrompt "You are a kernel reverse engineer specialist"
```

Available sizes: 7B, 13B, 30B, 50B, 120B, 800B
Quantization: Q4_K, Q8_0, FP16, FP32
"@
            }
            "train_model" = @{
                Question = "How do I train a model?"
                Answer = @"
Train models autonomously:

```powershell
.\autonomous_finetune_bench.ps1 -Operation autonomous-train -TrainingFiles "data/*.txt" -Duration 24 -Win32Integration
```

Parameters:
- Duration: Hours to train
- Iterations: Number of iterations
- LearningRate: Default 1e-5
- BatchSize: Default 4
"@
            }
            "quantize_model" = @{
                Question = "How do I quantize a model?"
                Answer = @"
Quantize models:

**Dynamic Quantization:**
```powershell
.\model_agent_making_station.ps1 -Operation menu
# Select option 21 for Virtual Quantization
```

**Reverse Quantization:**
```powershell
Import-Module .\Advanced-Model-Operations.psm1
Invoke-ReverseQuantization -ModelPath "model.gguf" -TargetFormat "FP32"
```
"@
            }
        }
    }
    
    "benchmarking" = @{
        Keywords = @("benchmark", "test", "performance", "speed", "cloud", "local")
        Answers = @{
            "benchmark_formats" = @{
                Question = "How do I benchmark different model formats?"
                Answer = @"
Benchmark model formats:

```powershell
.\autonomous_finetune_bench.ps1 -Operation benchmark -ModelFormats @("gguf", "blob", "safetensors") -Iterations 100
```

Compare cloud vs local:
```powershell
.\autonomous_finetune_bench.ps1 -Operation cloud-vs-local -ModelName "llama-7b" -Iterations 50
```
"@
            }
            "reverse_engineer" = @{
                Question = "How do I reverse engineer a cloud model?"
                Answer = @"
Reverse engineer cloud model characteristics:

```powershell
.\autonomous_finetune_bench.ps1 -Operation reverse-engineer -ModelName "gpt-3.5" -CloudEndpoint "https://api.openai.com"
```

This will detect:
- Model size estimation
- Context window size
- Architecture fingerprint
- Response time characteristics
"@
            }
        }
    }
    
    "advanced" = @{
        Keywords = @("prune", "freeze", "virtual", "quantization", "state")
        Answers = @{
            "virtual_quant" = @{
                Question = "What is virtual quantization?"
                Answer = @"
Virtual Quantization allows dynamic switching between quantization states without re-quantizing the model.

**Enable:**
```powershell
Import-Module .\Advanced-Model-Operations.psm1
Set-VirtualQuantizationState -ModelPath "model.gguf" -State "Q4_K" -EnableVirtual
```

**Quick Switch:**
```powershell
Invoke-QuickModelSwitch -ModelPath "model.gguf" -TargetState "Q8_0"
```

Benefits:
- Instant switching between precision levels
- No re-quantization overhead
- Preserves all quantization states in memory
"@
            }
            "intelligent_prune" = @{
                Question = "How does intelligent pruning work?"
                Answer = @"
Intelligent Pruning removes less important model weights:

```powershell
Invoke-IntelligentPruning -ModelPath "model.gguf" -PrunePercentage 30 -Strategy importance
```

Strategies:
- importance: Remove lowest importance weights
- random: Random pruning
- structured: Layer-wise structured pruning

This reduces model size while maintaining performance.
"@
            }
        }
    }
    
    "files" = @{
        Keywords = @("file", "directory", "path", "location", "find")
        Answers = @{
            "file_locations" = @{
                Question = "Where are the important files?"
                Answer = @"
**Key File Locations:**

Scripts:
- D:\lazy init ide\scripts\todo_manager.ps1
- D:\lazy init ide\scripts\model_agent_making_station.ps1
- D:\lazy init ide\scripts\autonomous_finetune_bench.ps1
- D:\lazy init ide\scripts\swarm_control.ps1

Modules:
- D:\lazy init ide\scripts\TodoManager.psm1
- D:\lazy init ide\scripts\Advanced-Model-Operations.psm1

Data:
- D:\lazy init ide\data\todos.json
- D:\lazy init ide\data\models\
- D:\lazy init ide\logs\

Win32 Integration:
- D:\lazy init ide\src\win32app\Win32IDE.cpp
- D:\lazy init ide\src\win32app\TodoManager.h/cpp
"@
            }
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHATBOT ENGINE
# ═══════════════════════════════════════════════════════════════════════════════

class ChatbotEngine {
    [hashtable]$KnowledgeBase
    [System.Collections.ArrayList]$ConversationHistory
    
    ChatbotEngine([hashtable]$knowledgeBase) {
        $this.KnowledgeBase = $knowledgeBase
        $this.ConversationHistory = [System.Collections.ArrayList]::new()
    }
    
    [string] ProcessQuestion([string]$question) {
        # Add to history
        $this.ConversationHistory.Add(@{
            Timestamp = Get-Date
            Question = $question
            Answer = ""
        }) | Out-Null
        
        $question = $question.ToLower().Trim()
        
        # Check for greetings
        if ($question -match '^(hi|hello|hey|greetings)') {
            return $this.GetGreeting()
        }
        
        # Check for help request
        if ($question -match '^(help|what can you do|commands)') {
            return $this.GetHelpText()
        }
        
        # Search knowledge base
        $answer = $this.SearchKnowledgeBase($question)
        
        if ($answer) {
            $this.ConversationHistory[-1].Answer = $answer
            return $answer
        }
        
        # No match found
        return $this.GetNoMatchResponse($question)
    }
    
    [string] SearchKnowledgeBase([string]$question) {
        $bestMatch = $null
        $bestScore = 0
        
        foreach ($category in $this.KnowledgeBase.Keys) {
            $categoryData = $this.KnowledgeBase[$category]
            
            # Check keywords
            $keywordScore = 0
            foreach ($keyword in $categoryData.Keywords) {
                if ($question -match [regex]::Escape($keyword)) {
                    $keywordScore++
                }
            }
            
            if ($keywordScore -gt 0) {
                # Search answers in this category
                foreach ($answerKey in $categoryData.Answers.Keys) {
                    $answerData = $categoryData.Answers[$answerKey]
                    $answerQuestion = $answerData.Question.ToLower()
                    
                    # Calculate similarity
                    $score = $this.CalculateSimilarity($question, $answerQuestion)
                    $score += $keywordScore * 2  # Boost for keyword matches
                    
                    if ($score -gt $bestScore) {
                        $bestScore = $score
                        $bestMatch = $answerData
                    }
                }
            }
        }
        
        if ($bestMatch -and $bestScore -gt 2) {
            return "`n$($bestMatch.Question)`n`n$($bestMatch.Answer)"
        }
        
        return $null
    }
    
    [int] CalculateSimilarity([string]$text1, [string]$text2) {
        $words1 = $text1 -split '\s+'
        $words2 = $text2 -split '\s+'
        
        $commonWords = 0
        foreach ($word in $words1) {
            if ($word.Length -gt 2 -and $words2 -contains $word) {
                $commonWords++
            }
        }
        
        return $commonWords
    }
    
    [string] GetGreeting() {
        return @"

👋 Hello! I'm the RawrXD IDE Assistant!

I can help you with:
- 🤖 Swarm operations and directory management
- ✅ Todo management (add, parse, agentic creation)
- 🧠 Model creation, training, and operations
- 📊 Benchmarking and performance testing
- ⚡ Advanced operations (quantization, pruning)

Ask me questions like:
- "How do I send a swarm to a directory?"
- "How do I add a todo?"
- "How do I create a 7B model?"
- "How do I benchmark model formats?"

Type 'help' for more commands or 'exit' to quit.
"@
    }
    
    [string] GetHelpText() {
        return @"

📚 AVAILABLE COMMANDS:

**Swarm Operations:**
- "How do I send a swarm to a directory?"
- "How do I monitor swarm activity?"
- "How do I stop a swarm?"

**Todo Management:**
- "How do I add a todo?"
- "How do I view my todos?"
- "How do I complete a todo?"

**Model Operations:**
- "How do I create a model?"
- "How do I train a model?"
- "How do I quantize a model?"

**Benchmarking:**
- "How do I benchmark model formats?"
- "How do I reverse engineer a cloud model?"

**Advanced Operations:**
- "What is virtual quantization?"
- "How does intelligent pruning work?"

**System:**
- "Where are the important files?"

Type your question naturally, and I'll find the best answer!
"@
    }
    
    [string] GetNoMatchResponse([string]$question) {
        $suggestions = @()
        
        # Suggest based on partial matches
        if ($question -match 'swarm') {
            $suggestions += "Try: 'How do I send a swarm to a directory?'"
        }
        if ($question -match 'todo') {
            $suggestions += "Try: 'How do I add a todo?'"
        }
        if ($question -match 'model') {
            $suggestions += "Try: 'How do I create a model?'"
        }
        
        $response = "`n❓ I'm not sure about that. "
        
        if ($suggestions.Count -gt 0) {
            $response += "Did you mean:`n"
            foreach ($suggestion in $suggestions) {
                $response += "  • $suggestion`n"
            }
        }
        else {
            $response += "Type 'help' to see available commands."
        }
        
        return $response
    }
    
    [string] GetConversationSummary() {
        $summary = "`n📝 CONVERSATION HISTORY:`n"
        $summary += "─" * 70 + "`n"
        
        foreach ($entry in $this.ConversationHistory) {
            $time = $entry.Timestamp.ToString("HH:mm:ss")
            $summary += "[$time] Q: $($entry.Question)`n"
            if ($entry.Answer) {
                $preview = $entry.Answer.Substring(0, [Math]::Min(80, $entry.Answer.Length))
                $summary += "          A: $preview...`n"
            }
            $summary += "`n"
        }
        
        return $summary
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# CLI INTERFACE
# ═══════════════════════════════════════════════════════════════════════════════

function Start-InteractiveChatbot {
    $chatbot = [ChatbotEngine]::new($script:KnowledgeBase)
    
    Clear-Host
    Write-Host @"

╔═══════════════════════════════════════════════════════════════════════════════╗
║                                                                               ║
║                    🤖 RawrXD IDE Interactive Assistant 🤖                     ║
║                                                                               ║
║                        Your AI-Powered Development Helper                     ║
║                                                                               ║
╚═══════════════════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan
    
    $answer = $chatbot.GetGreeting()
    Write-Host $answer -ForegroundColor Gray
    
    while ($true) {
        Write-Host "`n" -NoNewline
        Write-Host "You: " -NoNewline -ForegroundColor Green
        $question = Read-Host
        
        if (-not $question) {
            continue
        }
        
        # Check for exit commands
        if ($question -match '^(exit|quit|bye|goodbye)$') {
            Write-Host "`n👋 Goodbye! Happy coding!" -ForegroundColor Cyan
            
            # Show conversation summary
            $summary = $chatbot.GetConversationSummary()
            Write-Host $summary -ForegroundColor DarkGray
            break
        }
        
        # Process question
        Write-Host "`nAssistant: " -ForegroundColor Cyan
        $answer = $chatbot.ProcessQuestion($question)
        Write-Host $answer -ForegroundColor Gray
    }
}

function Start-SingleQuestionMode {
    param([string]$Question)
    
    $chatbot = [ChatbotEngine]::new($script:KnowledgeBase)
    
    Write-Host "`n🤖 RawrXD IDE Assistant" -ForegroundColor Cyan
    Write-Host "─" * 70 -ForegroundColor DarkGray
    Write-Host "Q: $Question" -ForegroundColor Green
    Write-Host ""
    
    $answer = $chatbot.ProcessQuestion($Question)
    Write-Host $answer -ForegroundColor Gray
    Write-Host ""
}

function Start-APIMode {
    param([int]$Port)
    
    Write-Host "`n🌐 Starting RawrXD IDE Chatbot API Server on port $Port..." -ForegroundColor Cyan
    
    $chatbot = [ChatbotEngine]::new($script:KnowledgeBase)
    $listener = [System.Net.HttpListener]::new()
    $listener.Prefixes.Add("http://localhost:$Port/")
    
    try {
        $listener.Start()
        Write-Host "✅ API Server running at http://localhost:$Port/" -ForegroundColor Green
        Write-Host "   Endpoints:" -ForegroundColor Gray
        Write-Host "   - POST /ask { question: 'your question' }" -ForegroundColor Gray
        Write-Host "   - GET /health" -ForegroundColor Gray
        Write-Host "`nPress Ctrl+C to stop...`n" -ForegroundColor Yellow
        
        while ($listener.IsListening) {
            $context = $listener.GetContext()
            $request = $context.Request
            $response = $context.Response
            
            $response.Headers.Add("Content-Type", "application/json")
            $response.Headers.Add("Access-Control-Allow-Origin", "*")
            
            if ($request.HttpMethod -eq "POST" -and $request.Url.AbsolutePath -eq "/ask") {
                $reader = [System.IO.StreamReader]::new($request.InputStream)
                $body = $reader.ReadToEnd() | ConvertFrom-Json
                
                $answer = $chatbot.ProcessQuestion($body.question)
                
                $result = @{
                    question = $body.question
                    answer = $answer
                    timestamp = (Get-Date).ToString("o")
                } | ConvertTo-Json
                
                $buffer = [System.Text.Encoding]::UTF8.GetBytes($result)
                $response.OutputStream.Write($buffer, 0, $buffer.Length)
            }
            elseif ($request.HttpMethod -eq "GET" -and $request.Url.AbsolutePath -eq "/health") {
                $health = @{
                    status = "ok"
                    uptime = ((Get-Date) - $listener.Start).TotalSeconds
                } | ConvertTo-Json
                
                $buffer = [System.Text.Encoding]::UTF8.GetBytes($health)
                $response.OutputStream.Write($buffer, 0, $buffer.Length)
            }
            else {
                $response.StatusCode = 404
            }
            
            $response.Close()
        }
    }
    finally {
        $listener.Stop()
        $listener.Close()
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

switch ($Mode) {
    "interactive" {
        Start-InteractiveChatbot
    }
    "single-question" {
        if (-not $Question) {
            Write-Error "Question parameter required for single-question mode"
            exit 1
        }
        Start-SingleQuestionMode -Question $Question
    }
    "api" {
        Start-APIMode -Port $Port
    }
}
