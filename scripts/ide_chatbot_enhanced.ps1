#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD IDE Enhanced Chatbot - Powered by Full Source Code Knowledge

.DESCRIPTION
    Intelligent chatbot that learns from your entire codebase:
    - Digests all source files (PowerShell, C++, Markdown)
    - Indexes functions, classes, and documentation
    - Answers questions based on actual implementation
    - Provides file locations and code examples
    - Falls back to manual knowledge base

.PARAMETER Question
    The question to ask the chatbot

.PARAMETER Mode
    interactive, single-question, api

.PARAMETER DigestFirst
    Run source digestion before starting chatbot

.EXAMPLE
    .\ide_chatbot_enhanced.ps1 -DigestFirst -Mode interactive
    
.EXAMPLE
    .\ide_chatbot_enhanced.ps1 -Question "How do I send a swarm to a directory?"
    
.EXAMPLE
    .\ide_chatbot_enhanced.ps1 -Mode api -Port 8080
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
    [switch]$DigestFirst,
    
    [Parameter(Mandatory=$false)]
    [switch]$UseExternalAPIFallback
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# ENHANCED CHATBOT ENGINE - WITH SOURCE CODE KNOWLEDGE
# ═══════════════════════════════════════════════════════════════════════════════

class EnhancedChatbot {
    [hashtable]$DigestedKB
    [hashtable]$ManualKB
    [System.Collections.ArrayList]$ConversationHistory
    [bool]$HasDigestedKB
    [string]$KBPath
    [bool]$UseExternalAPIFallback
    
    EnhancedChatbot([bool]$useExternalAPIFallback = $false) {
        $this.UseExternalAPIFallback = $useExternalAPIFallback
        $this.ConversationHistory = [System.Collections.ArrayList]::new()
        $projectRoot = if ($env:LAZY_INIT_IDE_ROOT) { $env:LAZY_INIT_IDE_ROOT } else { (Resolve-Path (Join-Path $PSScriptRoot "..") -ErrorAction SilentlyContinue).Path }
        $this.KBPath = if ($projectRoot) { Join-Path $projectRoot "data" "knowledge_base.json" } else { (Join-Path "D:\lazy init ide" "data" "knowledge_base.json") }
        $this.HasDigestedKB = $false
        
        $this.LoadDigestedKnowledgeBase()
        $this.InitializeManualKB()
    }
    
    [void] LoadDigestedKnowledgeBase() {
        if (Test-Path $this.KBPath) {
            try {
                Write-Host "`n  Loading comprehensive knowledge base..." -ForegroundColor Yellow
                $json = Get-Content $this.KBPath -Raw | ConvertFrom-Json -AsHashtable
                $this.DigestedKB = $json
                $this.HasDigestedKB = $true
                
                Write-Host "  ✓ Loaded: $($json.Metadata.FileCount) files, $($json.Functions.Count) functions, $($json.Classes.Count) classes" -ForegroundColor Green
                Write-Host "  ✓ Knowledge base size: $([Math]::Round((Get-Item $this.KBPath).Length / 1MB, 2)) MB" -ForegroundColor Green
                Write-Host "  ✓ Indexed $($json.Keywords.Count) keywords from source code`n" -ForegroundColor Green
            }
            catch {
                Write-Host "  ⚠ Failed to load digested KB: $_" -ForegroundColor Yellow
                $this.HasDigestedKB = $false
            }
        }
        else {
            Write-Host "`n  ℹ No digested knowledge base found." -ForegroundColor Yellow
            Write-Host "  To unlock full power, run: .\source_digester.ps1 -Operation digest`n" -ForegroundColor Cyan
            $this.HasDigestedKB = $false
        }
    }
    
    [void] InitializeManualKB() {
        # Fallback knowledge base for common questions
        $this.ManualKB = @{
            "swarm_deploy" = @{
                Keywords = @("swarm", "deploy", "directory", "send", "agents")
                Answer = @"
🤖 SWARM DEPLOYMENT TO DIRECTORY

**Quick Command:**
``````powershell
.\swarm_control.ps1 -Operation deploy -TargetDirectory "D:\target\path" -SwarmSize 5
``````

**What happens:**
1. Creates 5 AI agents
2. Sends them to D:\target\path
3. They analyze the directory structure
4. Execute assigned tasks (code analysis, file processing, etc.)
5. Report results back

**Monitor them:**
``````powershell
.\swarm_control.ps1 -Operation status
``````

**Related files:** Check digested KB for swarm_control.ps1, SwarmManager.psm1
"@
            }
            "todo_add" = @{
                Keywords = @("todo", "add", "task", "create")
                Answer = @"
✅ ADD A TODO

**Command:**
``````powershell
.\todo_manager.ps1 -Operation add -Title "Task name" -Description "Details"
``````

**Parse from text:**
``````powershell
# Automatically finds !todo markers in files
.\todo_manager.ps1 -Operation parse
``````

**View all todos:**
``````powershell
.\todo_manager.ps1 -Operation list
``````
"@
            }
            "model_create" = @{
                Keywords = @("model", "create", "train", "make", "build")
                Answer = @"
🧠 CREATE A MODEL

**Available sizes:** 7B, 13B, 30B, 50B, 120B, 800B

**Command:**
``````powershell
.\model_agent_making_station.ps1 -Operation create -Template "Small-7B"
``````

**With custom prompt:**
``````powershell
.\model_agent_making_station.ps1 -Operation create -Template "Medium-30B" -CustomPrompt "You are a coding expert"
``````

**Train existing model:**
``````powershell
.\model_agent_making_station.ps1 -Operation train -ModelPath "model.gguf" -DataPath "training_data"
``````
"@
            }
        }
    }
    
    [string] ProcessQuestion([string]$question) {
        # Log to conversation history
        $this.ConversationHistory.Add(@{
            Timestamp = Get-Date
            Question = $question
            Type = "User"
        }) | Out-Null
        
        # Handle special commands
        if ($question -match '^\s*(help|commands)\s*$') {
            return $this.GetHelpText()
        }
        
        $answer = ""
        
        # Try natural language to command translation
        if ($question -match '^\s*(send|deploy|create|make|add|find|search|open|train|quantize|monitor|stop|benchmark|prune)') {
            try {
                $translationResult = & "$PSScriptRoot\command_translator.ps1" -Request $question 2>&1
                if ($translationResult -is [string] -and $translationResult -match '"Success":\s*true') {
                    $json = $translationResult | ConvertFrom-Json
                    $answer = "🤖 **I'll translate that for you:**`n`n"
                    $answer += "📝 **Machine Command:**`n``````powershell`n$($json.Command)`n```````n`n"
                    $answer += "💡 **What it does:** $($json.Explanation)`n`n"
                    $answer += "⚡ **To execute:** Add ``-Execute`` flag or copy-paste the command"
                    
                    # Log answer and return
                    $this.ConversationHistory.Add(@{ Timestamp = Get-Date; Answer = $answer; Type = "Assistant" }) | Out-Null
                    return $answer
                }
            }
            catch {
                # Continue to other methods if translation fails
            }
        }
        
        # Try browser search for web queries
        if ($question -match '(search|look\s+up|google|find\s+info).*(online|web|internet)' -or 
            $question -match 'what\s+is|how\s+does.*work|explain') {
            try {
                $searchQuery = $question -replace '(search|look\s+up|google|find\s+info|what\s+is|how\s+does|explain|online|web|internet)', '' -replace '\s+', ' '
                $searchQuery = $searchQuery.Trim()
                
                if ($searchQuery.Length -gt 3) {
                    $searchResult = & "$PSScriptRoot\browser_helper.ps1" -Operation search -Query $searchQuery -MaxResults 3 2>&1 | Out-String
                    
                    if ($searchResult -match '"Success":\s*true') {
                        $json = $searchResult | ConvertFrom-Json
                        $answer = "🌐 **Web Search Results for:** $searchQuery`n`n"
                        
                        foreach ($result in $json.Results) {
                            $answer += "**$($result.Position). $($result.Title)**`n"
                            $answer += "   $($result.URL)`n`n"
                        }
                        
                        $answer += "💡 **Tip:** Use ``.\browser_helper.ps1 -Operation fetch -Query <url>`` to read a page"
                        
                        # Log and return
                        $this.ConversationHistory.Add(@{ Timestamp = Get-Date; Answer = $answer; Type = "Assistant" }) | Out-Null
                        return $answer
                    }
                }
            }
            catch {
                # Continue to other methods
            }
        }
        
        # First try digested knowledge base (comprehensive)
        if ($this.HasDigestedKB) {
            $digestedResults = $this.SearchDigestedKB($question)
            
            if ($digestedResults.Score -gt 8) {
                $answer = $this.FormatDigestedResults($question, $digestedResults)
            }
        }
        
        # Fall back to manual KB
        if ($answer -eq "") {
            $manualResults = $this.SearchManualKB($question)
            if ($manualResults.Count -gt 0) {
                $answer = $manualResults[0].Answer
            }
        }
        
        # Fallback: External API (OpenAI/Anthropic) when enabled
        if ($answer -eq "" -and $this.UseExternalAPIFallback) {
            try {
                $provider = if ($env:ANTHROPIC_API_KEY) { "anthropic" } else { "openai" }
                $extResult = & "$PSScriptRoot\external_api_bridge.ps1" -Provider $provider -Prompt $question 2>&1
                if ($extResult -and $extResult -notmatch "error|required") {
                    $answer = "🌐 **External API ($provider):**`n`n$extResult"
                }
            } catch {
                # Continue to GetNoMatchResponse
            }
        }
        
        # No results found
        if ($answer -eq "") {
            $answer = $this.GetNoMatchResponse($question)
        }
        
        # Log answer
        $this.ConversationHistory.Add(@{
            Timestamp = Get-Date
            Answer = $answer
            Type = "Assistant"
        }) | Out-Null
        
        return $answer
    }
    
    [hashtable] SearchDigestedKB([string]$query) {
        $results = @{
            Files = @()
            Functions = @()
            Classes = @()
            Topics = @()
            CodeExamples = @()
            Score = 0
        }
        
        $queryLower = $query.ToLower()
        $keywords = $queryLower -split '\s+' | Where-Object { $_.Length -gt 2 }
        
        # Search files
        foreach ($filePath in $this.DigestedKB.Files.Keys) {
            $fileInfo = $this.DigestedKB.Files[$filePath]
            $score = 0
            
            foreach ($keyword in $keywords) {
                if ($fileInfo.Name.ToLower() -match $keyword) { $score += 10 }
                if ($fileInfo.Synopsis.ToLower() -match $keyword) { $score += 5 }
                if ($fileInfo.Keywords -contains $keyword) { $score += 3 }
                
                # Boost for exact function name matches
                foreach ($funcName in $fileInfo.Functions) {
                    if ($funcName.ToLower() -match $keyword) { $score += 7 }
                }
            }
            
            if ($score -gt 0) {
                $results.Files += @{
                    Path = $fileInfo.RelativePath
                    Name = $fileInfo.Name
                    Synopsis = $fileInfo.Synopsis
                    Functions = $fileInfo.Functions
                    Classes = $fileInfo.Classes
                    Lines = $fileInfo.Lines
                    Score = $score
                }
            }
        }
        
        # Search functions directly
        foreach ($funcName in $this.DigestedKB.Functions.Keys) {
            $funcInfo = $this.DigestedKB.Functions[$funcName]
            $score = 0
            
            foreach ($keyword in $keywords) {
                if ($funcName.ToLower() -match $keyword) { $score += 15 }
                if ($funcInfo.Synopsis.ToLower() -match $keyword) { $score += 5 }
            }
            
            if ($score -gt 0) {
                $results.Functions += @{
                    Name = $funcName
                    Synopsis = $funcInfo.Synopsis
                    Parameters = $funcInfo.Parameters
                    Score = $score
                }
            }
        }
        
        # Search classes
        foreach ($className in $this.DigestedKB.Classes.Keys) {
            $classInfo = $this.DigestedKB.Classes[$className]
            $score = 0
            
            foreach ($keyword in $keywords) {
                if ($className.ToLower() -match $keyword) { $score += 12 }
            }
            
            if ($score -gt 0) {
                $results.Classes += @{
                    Name = $className
                    Properties = $classInfo.Properties
                    Methods = $classInfo.Methods
                    Score = $score
                }
            }
        }
        
        # Sort by relevance
        $results.Files = $results.Files | Sort-Object -Property Score -Descending
        $results.Functions = $results.Functions | Sort-Object -Property Score -Descending
        $results.Classes = $results.Classes | Sort-Object -Property Score -Descending
        
        # Calculate total score
        $results.Score = ($results.Files | Measure-Object -Property Score -Sum).Sum + 
                         ($results.Functions | Measure-Object -Property Score -Sum).Sum +
                         ($results.Classes | Measure-Object -Property Score -Sum).Sum
        
        return $results
    }
    
    [array] SearchManualKB([string]$question) {
        $results = @()
        
        foreach ($key in $this.ManualKB.Keys) {
            $entry = $this.ManualKB[$key]
            $score = 0
            
            foreach ($keyword in $entry.Keywords) {
                if ($question.ToLower() -match $keyword) {
                    $score++
                }
            }
            
            if ($score -gt 0) {
                $results += @{
                    Answer = $entry.Answer
                    Score = $score
                }
            }
        }
        
        return $results | Sort-Object -Property Score -Descending
    }
    
    [string] FormatDigestedResults([string]$question, [hashtable]$results) {
        $response = "📚 **Based on your codebase:**`n`n"
        
        # Show most relevant files
        if ($results.Files.Count -gt 0) {
            $response += "📁 **Relevant Files:**`n"
            foreach ($file in ($results.Files | Select-Object -First 3)) {
                $response += "  📄 ``$($file.Name)`` (Score: $($file.Score))`n"
                
                if ($file.Synopsis) {
                    $response += "     💡 $($file.Synopsis)`n"
                }
                
                if ($file.Functions.Count -gt 0) {
                    $funcList = $file.Functions | Select-Object -First 5
                    $response += "     ⚡ Functions: $($funcList -join ', ')$(if ($file.Functions.Count -gt 5) { '...' } else { '' })`n"
                }
                
                $response += "     📍 Path: $($file.Path)`n"
                $response += "     📊 Lines: $($file.Lines)`n`n"
            }
        }
        
        # Show most relevant functions
        if ($results.Functions.Count -gt 0) {
            $response += "`n⚡ **Relevant Functions:**`n"
            foreach ($func in ($results.Functions | Select-Object -First 5)) {
                $response += "  🔧 ``$($func.Name)()`` (Score: $($func.Score))`n"
                
                if ($func.Synopsis) {
                    $response += "     💬 $($func.Synopsis)`n"
                }
                
                if ($func.Parameters -and $func.Parameters.Count -gt 0) {
                    $paramList = $func.Parameters | ForEach-Object { "[$($_.Type)] `$$($_.Name)" }
                    $response += "     📋 Parameters: $($paramList -join ', ')`n"
                }
                $response += "`n"
            }
        }
        
        # Show relevant classes
        if ($results.Classes.Count -gt 0) {
            $response += "`n🏗️ **Relevant Classes:**`n"
            foreach ($class in ($results.Classes | Select-Object -First 3)) {
                $response += "  📦 ``$($class.Name)`` (Score: $($class.Score))`n"
                
                if ($class.Properties -and $class.Properties.Count -gt 0) {
                    $response += "     🔹 Properties: $($class.Properties.Count)`n"
                }
                
                if ($class.Methods -and $class.Methods.Count -gt 0) {
                    $methodNames = $class.Methods | ForEach-Object { $_.Name } | Select-Object -First 5
                    $response += "     🔸 Methods: $($methodNames -join ', ')$(if ($class.Methods.Count -gt 5) { '...' } else { '' })`n"
                }
                $response += "`n"
            }
        }
        
        # Add usage examples from manual KB if available
        $manualResults = $this.SearchManualKB($question)
        if ($manualResults.Count -gt 0) {
            $response += "`n💡 **Quick Usage Example:**`n"
            $response += $manualResults[0].Answer
        }
        
        $response += "`n`n🔍 **Pro Tip:** For deeper search, use: ``.\source_digester.ps1 -Operation search -Query `"$question`"``"
        
        return $response
    }
    
    [string] GetHelpText() {
        $baseHelp = @"

📖 **RawrXD IDE Enhanced Assistant - Help**

**I know about your entire codebase!**
"@
        
        if ($this.HasDigestedKB) {
            $baseHelp += "`n✅ Loaded: $($this.DigestedKB.Metadata.FileCount) files, $($this.DigestedKB.Functions.Count) functions, $($this.DigestedKB.Classes.Count) classes"
        }
        else {
            $baseHelp += "`n⚠️  No digested KB. Run: ``.\source_digester.ps1 -Operation digest``"
        }
        
        $baseHelp += @"


**Ask me anything like:**
• "How do I send a swarm to a directory?"
• "Show me functions for model training"
• "What classes handle quantization?"
• "Where is the todo manager?"
• "How does virtual quantization work?"

**Special commands:**
• ``help`` - Show this help
• ``stats`` - Show knowledge base statistics
• ``refresh`` - Reload knowledge base
• ``exit`` - Quit chatbot

**Advanced search:**
• ``.\source_digester.ps1 -Operation search -Query "your query"``
"@
        
        return $baseHelp
    }
    
    [string] GetNoMatchResponse([string]$question) {
        $response = "`n❓ **I couldn't find a direct match for that.**`n`n"
        
        if ($this.UseExternalAPIFallback) {
            $response += "💡 **Tip:** Set OPENAI_API_KEY or ANTHROPIC_API_KEY and use -UseExternalAPIFallback for frontier model fallback.`n`n"
        }
        if ($this.HasDigestedKB) {
            $response += "Try rephrasing with keywords like:`n"
            $response += "  • swarm, deploy, agents`n"
            $response += "  • model, train, create`n"
            $response += "  • todo, task, manage`n"
            $response += "  • quantize, prune, optimize`n"
        }
        else {
            $response += "💡 **Unlock full power by digesting your codebase:**`n"
            $response += "``````powershell`n"
            $response += ".\source_digester.ps1 -Operation digest`n"
            $response += "``````"
        }
        
        return $response
    }
    
    [string] GetGreeting() {
        $greeting = @"

╔═══════════════════════════════════════════════════════════════════╗
║         🤖 RawrXD IDE Enhanced Assistant 🤖                       ║
╚═══════════════════════════════════════════════════════════════════╝

"@
        
        if ($this.HasDigestedKB) {
            $greeting += "✨ **POWERED BY YOUR FULL CODEBASE** ✨`n`n"
            $greeting += "📊 Knowledge Base:`n"
            $greeting += "   • Files: $($this.DigestedKB.Metadata.FileCount)`n"
            $greeting += "   • Functions: $($this.DigestedKB.Functions.Count)`n"
            $greeting += "   • Classes: $($this.DigestedKB.Classes.Count)`n"
            $greeting += "   • Keywords: $($this.DigestedKB.Keywords.Count)`n"
        }
        else {
            $greeting += "⚠️  **Running in limited mode**`n"
            $greeting += "To unlock full codebase knowledge, run:`n"
            $greeting += "``.\source_digester.ps1 -Operation digest```n"
        }
        
        $greeting += @"

Ask me anything about:
🤖 Swarms  ✅ Todos  🧠 Models  📊 Benchmarks  ⚡ Advanced Ops

Type 'help' for commands or ask your question!
"@
        
        return $greeting
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# INTERACTIVE MODE
# ═══════════════════════════════════════════════════════════════════════════════

function Start-InteractiveChatbot {
    param([bool]$UseExternalAPIFallback = $false)
    $chatbot = [EnhancedChatbot]::new($UseExternalAPIFallback)
    
    Write-Host $chatbot.GetGreeting() -ForegroundColor Cyan
    
    while ($true) {
        Write-Host "`n" -NoNewline
        Write-Host "You> " -NoNewline -ForegroundColor Green
        $userInput = Read-Host
        
        if ($userInput -match '^\s*(exit|quit|bye)\s*$') {
            Write-Host "`n👋 Goodbye! Happy coding!`n" -ForegroundColor Yellow
            break
        }
        
        if ($userInput.Trim() -eq "") {
            continue
        }
        
        # Special commands
        if ($userInput -match '^\s*stats\s*$') {
            if ($chatbot.HasDigestedKB) {
                Write-Host "`n📊 KNOWLEDGE BASE STATISTICS:" -ForegroundColor Magenta
                Write-Host "   Files:      $($chatbot.DigestedKB.Metadata.FileCount)" -ForegroundColor Gray
                Write-Host "   Functions:  $($chatbot.DigestedKB.Functions.Count)" -ForegroundColor Gray
                Write-Host "   Classes:    $($chatbot.DigestedKB.Classes.Count)" -ForegroundColor Gray
                Write-Host "   Keywords:   $($chatbot.DigestedKB.Keywords.Count)" -ForegroundColor Gray
                Write-Host "   Digest Date: $($chatbot.DigestedKB.Metadata.DigestDate)" -ForegroundColor Gray
            }
            else {
                Write-Host "`n⚠️  No knowledge base loaded" -ForegroundColor Yellow
            }
            continue
        }
        
        if ($userInput -match '^\s*refresh\s*$') {
            Write-Host "`n🔄 Reloading knowledge base..." -ForegroundColor Yellow
            $chatbot.LoadDigestedKnowledgeBase()
            continue
        }
        
        # Process question
        Write-Host "`nAssistant> " -ForegroundColor Cyan
        $answer = $chatbot.ProcessQuestion($userInput)
        Write-Host $answer -ForegroundColor White
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# SINGLE QUESTION MODE
# ═══════════════════════════════════════════════════════════════════════════════

function Start-SingleQuestionMode([string]$question, [bool]$UseExternalAPIFallback = $false) {
    $chatbot = [EnhancedChatbot]::new($UseExternalAPIFallback)
    
    Write-Host "`n🤖 Question: $question`n" -ForegroundColor Cyan
    $answer = $chatbot.ProcessQuestion($question)
    Write-Host $answer -ForegroundColor White
    Write-Host ""
}

# ═══════════════════════════════════════════════════════════════════════════════
# API MODE
# ═══════════════════════════════════════════════════════════════════════════════

function Start-APIMode([int]$port, [bool]$UseExternalAPIFallback = $false) {
    $chatbot = [EnhancedChatbot]::new($UseExternalAPIFallback)
    
    Write-Host "`n╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║         Enhanced Chatbot API Server                              ║" -ForegroundColor Magenta
    Write-Host "╚═══════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta
    
    Write-Host "  🌐 Starting HTTP server on http://localhost:$port" -ForegroundColor Yellow
    Write-Host "  📡 API Endpoint: http://localhost:$port/ask" -ForegroundColor Yellow
    Write-Host "  🛑 Press Ctrl+C to stop`n" -ForegroundColor Gray
    
    $listener = [System.Net.HttpListener]::new()
    $listener.Prefixes.Add("http://localhost:$port/")
    $listener.Start()
    
    Write-Host "  ✅ Server running!`n" -ForegroundColor Green
    
    try {
        while ($listener.IsListening) {
            $context = $listener.GetContext()
            $request = $context.Request
            $response = $context.Response
            
            # Enable CORS
            $response.AddHeader("Access-Control-Allow-Origin", "*")
            $response.AddHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            $response.AddHeader("Access-Control-Allow-Headers", "Content-Type")
            
            if ($request.HttpMethod -eq "OPTIONS") {
                $response.StatusCode = 200
                $response.Close()
                continue
            }
            
            if ($request.Url.AbsolutePath -eq "/ask" -and $request.HttpMethod -eq "POST") {
                $reader = [System.IO.StreamReader]::new($request.InputStream)
                $body = $reader.ReadToEnd()
                $json = $body | ConvertFrom-Json
                
                Write-Host "  📩 Question: $($json.question)" -ForegroundColor Cyan
                
                $answer = $chatbot.ProcessQuestion($json.question)
                
                $responseJson = @{
                    answer = $answer
                    timestamp = Get-Date -Format "o"
                } | ConvertTo-Json
                
                $buffer = [System.Text.Encoding]::UTF8.GetBytes($responseJson)
                $response.ContentType = "application/json"
                $response.ContentLength64 = $buffer.Length
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
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

# Run source digester first if requested
if ($DigestFirst) {
    Write-Host "`n🔄 Running source digester first..." -ForegroundColor Yellow
    & "$PSScriptRoot\source_digester.ps1" -Operation digest
    Write-Host ""
}

# Execute based on mode
switch ($Mode) {
    "interactive" {
        Start-InteractiveChatbot -UseExternalAPIFallback $UseExternalAPIFallback
    }
    "single-question" {
        if ($Question) {
            Start-SingleQuestionMode -question $Question -UseExternalAPIFallback $UseExternalAPIFallback
        }
        else {
            Write-Error "Question parameter required for single-question mode"
            exit 1
        }
    }
    "api" {
        Start-APIMode -port $Port -UseExternalAPIFallback $UseExternalAPIFallback
    }
}
