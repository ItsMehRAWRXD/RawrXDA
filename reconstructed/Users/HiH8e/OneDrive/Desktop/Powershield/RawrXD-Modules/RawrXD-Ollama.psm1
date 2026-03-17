# RawrXD-Ollama.psm1 - Extended Ollama integration module

function Initialize-OllamaIntegration {
    Write-RawrXDLog "Initializing Ollama integration..." -Level INFO -Component "Ollama"
    
    # Test connection and get initial status
    $connected = Test-OllamaConnection
    $global:RawrXD.OllamaAvailable = $connected
    
    if ($connected) {
        # Get available models
        $models = Get-OllamaModels
        $global:RawrXD.AvailableModels = $models
        
        # Set up periodic health checks
        Setup-OllamaHealthCheck
        
        Write-RawrXDLog "Ollama integration initialized successfully with $($models.Count) models" -Level SUCCESS -Component "Ollama"
    }
    else {
        Write-RawrXDLog "Ollama not available - some features will be disabled" -Level WARNING -Component "Ollama"
    }
}

function Setup-OllamaHealthCheck {
    # Create a timer to periodically check Ollama health
    $global:RawrXD.OllamaHealthTimer = New-Object System.Windows.Forms.Timer
    $global:RawrXD.OllamaHealthTimer.Interval = 30000  # 30 seconds
    $global:RawrXD.OllamaHealthTimer.add_Tick({
        try {
            $wasAvailable = $global:RawrXD.OllamaAvailable
            $isAvailable = Test-OllamaConnection
            $global:RawrXD.OllamaAvailable = $isAvailable
            
            # Update UI if status changed
            if ($wasAvailable -ne $isAvailable) {
                Update-OllamaStatus -Available $isAvailable
                
                if ($isAvailable) {
                    Write-RawrXDLog "Ollama connection restored" -Level SUCCESS -Component "Ollama"
                }
                else {
                    Write-RawrXDLog "Ollama connection lost" -Level WARNING -Component "Ollama"
                }
            }
        }
        catch {
            Write-RawrXDLog "Error in Ollama health check: $($_.Exception.Message)" -Level ERROR -Component "Ollama"
        }
    })
    $global:RawrXD.OllamaHealthTimer.Start()
}

function Update-OllamaStatus {
    param([bool]$Available)
    
    # Update status bar
    if ($global:RawrXD.Components.AiStatusLabel) {
        $global:RawrXD.Components.AiStatusLabel.Text = if ($Available) { "AI: Connected" } else { "AI: Disconnected" }
    }
    
    # Update chat panel status
    if ($global:RawrXD.Components.ChatButtons.Status) {
        $statusLabel = $global:RawrXD.Components.ChatButtons.Status
        $statusLabel.Text = if ($Available) { "✅ Connected" } else { "❌ Disconnected" }
        $statusLabel.ForeColor = if ($Available) { [System.Drawing.Color]::Green } else { [System.Drawing.Color]::Red }
    }
}

function Get-ModelCapabilities {
    param([string]$ModelName)
    
    # Define known model capabilities (this could be extended with API calls in the future)
    $capabilities = @{
        "llama3.2" = @{
            SupportsCode = $true
            SupportsReasoning = $true
            MaxTokens = 4096
            Languages = @("PowerShell", "Python", "JavaScript", "C#", "General")
            Specialties = @("Code Analysis", "Debugging", "Documentation")
        }
        "llama2" = @{
            SupportsCode = $true
            SupportsReasoning = $true
            MaxTokens = 2048
            Languages = @("PowerShell", "Python", "JavaScript", "General")
            Specialties = @("General Purpose", "Code Assistance")
        }
        "codellama" = @{
            SupportsCode = $true
            SupportsReasoning = $true
            MaxTokens = 4096
            Languages = @("Python", "JavaScript", "C++", "Java", "C#", "PowerShell")
            Specialties = @("Code Generation", "Code Completion", "Code Explanation", "Debugging")
        }
        "mistral" = @{
            SupportsCode = $true
            SupportsReasoning = $true
            MaxTokens = 8192
            Languages = @("PowerShell", "Python", "JavaScript", "C#", "General")
            Specialties = @("Analysis", "Reasoning", "Code Review")
        }
    }
    
    return $capabilities[$ModelName] ?? @{
        SupportsCode = $true
        SupportsReasoning = $true
        MaxTokens = 2048
        Languages = @("General")
        Specialties = @("General Purpose")
    }
}

function Get-OptimalModel {
    param(
        [string]$Task,
        [string]$Language = "General"
    )
    
    if (-not $global:RawrXD.AvailableModels) {
        return $global:RawrXD.Settings.AI.DefaultModel
    }
    
    # Simple model selection logic based on task and language
    $modelScores = @{}
    
    foreach ($model in $global:RawrXD.AvailableModels) {
        $capabilities = Get-ModelCapabilities -ModelName $model.name
        $score = 0
        
        # Score based on task
        switch ($Task.ToLower()) {
            "code" { 
                if ($capabilities.SupportsCode) { $score += 10 }
                if ($model.name -like "*code*") { $score += 5 }
            }
            "analysis" { 
                if ($capabilities.SupportsReasoning) { $score += 10 }
                if ($model.name -like "*mistral*") { $score += 3 }
            }
            "chat" { 
                $score += 5  # Base score for general chat
                if ($model.name -like "*llama*") { $score += 3 }
            }
        }
        
        # Score based on language support
        if ($capabilities.Languages -contains $Language) {
            $score += 5
        }
        
        # Prefer newer/larger models (simple heuristic)
        if ($model.name -match "3\.2|3\.1") { $score += 3 }
        elseif ($model.name -match "2\.1|2\.0") { $score += 1 }
        
        $modelScores[$model.name] = $score
    }
    
    # Return the highest scoring model
    $bestModel = $modelScores.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1
    return if ($bestModel) { $bestModel.Key } else { $global:RawrXD.Settings.AI.DefaultModel }
}

function Invoke-CodeAnalysis {
    param(
        [string]$Code,
        [string]$Language,
        [string]$AnalysisType = "general"
    )
    
    $model = Get-OptimalModel -Task "analysis" -Language $Language
    
    $systemPrompt = @"
You are an expert code analyst integrated into RawrXD text editor. Analyze the provided $Language code and provide insights based on the requested analysis type.

Analysis Type: $AnalysisType

Focus on:
- Code structure and organization
- Potential bugs or issues
- Performance considerations
- Best practices and improvements
- Security considerations (if applicable)
- Maintainability and readability

Provide actionable feedback that helps improve the code quality.
"@
    
    $userPrompt = @"
Please analyze this $Language code:

```$Language
$Code
```

Analysis type requested: $AnalysisType
"@
    
    try {
        $analysis = Invoke-OllamaChat -Model $model -Prompt $userPrompt -System $systemPrompt
        Write-RawrXDLog "Code analysis completed using model: $model" -Level SUCCESS -Component "Ollama"
        return $analysis
    }
    catch {
        Write-RawrXDLog "Code analysis failed: $($_.Exception.Message)" -Level ERROR -Component "Ollama"
        return "Sorry, I couldn't analyze the code due to an error: $($_.Exception.Message)"
    }
}

function Invoke-CodeCompletion {
    param(
        [string]$CodeContext,
        [string]$Language,
        [int]$MaxSuggestions = 3
    )
    
    $model = Get-OptimalModel -Task "code" -Language $Language
    
    $systemPrompt = @"
You are a code completion assistant integrated into RawrXD text editor. Provide helpful code completions for the given context.

Rules:
1. Provide only the completion part, not the entire context
2. Focus on the most likely and useful completions
3. Ensure completions follow $Language best practices
4. Provide multiple options when appropriate (max $MaxSuggestions)
5. Include brief explanations for complex completions
"@
    
    $userPrompt = @"
Complete this $Language code:

$CodeContext
"@
    
    try {
        $completion = Invoke-OllamaChat -Model $model -Prompt $userPrompt -System $systemPrompt
        Write-RawrXDLog "Code completion generated using model: $model" -Level SUCCESS -Component "Ollama"
        return $completion
    }
    catch {
        Write-RawrXDLog "Code completion failed: $($_.Exception.Message)" -Level ERROR -Component "Ollama"
        return $null
    }
}

function Get-AIBasedSuggestions {
    param(
        [string]$CurrentFile,
        [string]$CurrentText,
        [string]$CursorPosition
    )
    
    if (-not $global:RawrXD.OllamaAvailable) {
        return @()
    }
    
    $language = Get-FileExtensionLanguage -FilePath $CurrentFile
    $model = Get-OptimalModel -Task "code" -Language $language
    
    # Extract context around cursor for intelligent suggestions
    $lines = $CurrentText -split "`n"
    $suggestions = @()
    
    try {
        # Quick code completion based on current context
        $contextWindow = Get-ContextWindow -Text $CurrentText -CursorPosition $CursorPosition
        
        if ($contextWindow) {
            $completion = Invoke-CodeCompletion -CodeContext $contextWindow -Language $language
            if ($completion) {
                $suggestions += @{
                    Type = "Completion"
                    Text = $completion
                    Priority = 10
                }
            }
        }
        
        # Add common snippets for the language
        $snippets = Get-LanguageSnippets -Language $language
        $suggestions += $snippets
        
        return $suggestions
    }
    catch {
        Write-RawrXDLog "Error generating AI suggestions: $($_.Exception.Message)" -Level ERROR -Component "Ollama"
        return @()
    }
}

function Get-ContextWindow {
    param(
        [string]$Text,
        [int]$CursorPosition,
        [int]$WindowSize = 200
    )
    
    # Extract text around cursor position for context
    $startPos = [Math]::Max(0, $CursorPosition - $WindowSize)
    $endPos = [Math]::Min($Text.Length, $CursorPosition + $WindowSize)
    
    return $Text.Substring($startPos, $endPos - $startPos)
}

function Get-LanguageSnippets {
    param([string]$Language)
    
    $snippets = switch ($Language) {
        "PowerShell" {
            @(
                @{ Type = "Snippet"; Text = "if ($condition) {`n    `n}"; Priority = 8; Description = "If statement" },
                @{ Type = "Snippet"; Text = "foreach ($item in $collection) {`n    `n}"; Priority = 8; Description = "ForEach loop" },
                @{ Type = "Snippet"; Text = "try {`n    `n} catch {`n    `n}"; Priority = 7; Description = "Try-Catch block" },
                @{ Type = "Snippet"; Text = "function FunctionName {`n    param()`n    `n}"; Priority = 9; Description = "Function template" }
            )
        }
        "Python" {
            @(
                @{ Type = "Snippet"; Text = "if condition:`n    pass"; Priority = 8; Description = "If statement" },
                @{ Type = "Snippet"; Text = "for item in iterable:`n    pass"; Priority = 8; Description = "For loop" },
                @{ Type = "Snippet"; Text = "def function_name():`n    pass"; Priority = 9; Description = "Function definition" },
                @{ Type = "Snippet"; Text = "try:`n    pass`nexcept Exception as e:`n    pass"; Priority = 7; Description = "Try-Except block" }
            )
        }
        "JavaScript" {
            @(
                @{ Type = "Snippet"; Text = "if (condition) {`n    `n}"; Priority = 8; Description = "If statement" },
                @{ Type = "Snippet"; Text = "for (let i = 0; i < array.length; i++) {`n    `n}"; Priority = 8; Description = "For loop" },
                @{ Type = "Snippet"; Text = "function functionName() {`n    `n}"; Priority = 9; Description = "Function declaration" },
                @{ Type = "Snippet"; Text = "try {`n    `n} catch (error) {`n    `n}"; Priority = 7; Description = "Try-Catch block" }
            )
        }
        default {
            @()
        }
    }
    
    return $snippets
}

function Stop-OllamaIntegration {
    if ($global:RawrXD.OllamaHealthTimer) {
        $global:RawrXD.OllamaHealthTimer.Stop()
        $global:RawrXD.OllamaHealthTimer.Dispose()
        $global:RawrXD.OllamaHealthTimer = $null
    }
    Write-RawrXDLog "Ollama integration stopped" -Level INFO -Component "Ollama"
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-OllamaIntegration',
    'Get-ModelCapabilities',
    'Get-OptimalModel',
    'Invoke-CodeAnalysis',
    'Invoke-CodeCompletion',
    'Get-AIBasedSuggestions',
    'Stop-OllamaIntegration'
)