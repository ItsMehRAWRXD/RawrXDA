# RawrXD-CLI.psm1 - Command-line interface module

function Start-RawrXDCLI {
    param(
        [string]$Command,
        [string]$FilePath,
        [string]$Model,
        [string]$Prompt
    )
    
    Write-RawrXDLog "Starting RawrXD CLI mode..." -Level INFO -Component "CLI"
    
    if ($Command) {
        # Execute specific command
        switch ($Command.ToLower()) {
            'test-ollama' { 
                Test-OllamaCLI 
            }
            'list-models' { 
                List-ModelsCLI 
            }
            'chat' { 
                Start-ChatCLI -Model $Model 
            }
            'analyze-file' { 
                if ($FilePath) { 
                    Analyze-FileCLI -FilePath $FilePath -Model $Model 
                } else { 
                    Write-Host "Error: FilePath parameter required for analyze-file command" -ForegroundColor Red 
                }
            }
            'git-status' { 
                Show-GitStatusCLI -Path $FilePath 
            }
            'help' { 
                Show-HelpCLI 
            }
            default { 
                Write-Host "Unknown command: $Command" -ForegroundColor Red
                Show-HelpCLI
            }
        }
    }
    else {
        # Interactive CLI mode
        Start-InteractiveCLI
    }
}

function Test-OllamaCLI {
    Write-Host "Testing Ollama connection..." -ForegroundColor Cyan
    
    $connected = Test-OllamaConnection
    if ($connected) {
        Write-Host "✅ Ollama connection successful!" -ForegroundColor Green
        
        $models = Get-OllamaModels
        Write-Host "Found $($models.Count) available models" -ForegroundColor Green
        
        return $true
    }
    else {
        Write-Host "❌ Ollama connection failed!" -ForegroundColor Red
        Write-Host "Please ensure Ollama is running and accessible at: $($global:RawrXD.Settings.AI.OllamaEndpoint)" -ForegroundColor Yellow
        return $false
    }
}

function List-ModelsCLI {
    Write-Host "Retrieving available Ollama models..." -ForegroundColor Cyan
    
    $models = Get-OllamaModels
    if ($models.Count -gt 0) {
        Write-Host "`nAvailable Models:" -ForegroundColor Green
        Write-Host "=================" -ForegroundColor Green
        
        foreach ($model in $models) {
            $size = if ($model.size) { Format-FileSize -Size $model.size } else { "Unknown" }
            $modified = if ($model.modified) { $model.modified } else { "Unknown" }
            Write-Host "📦 $($model.name)" -ForegroundColor Yellow
            Write-Host "   Size: $size" -ForegroundColor Gray
            Write-Host "   Modified: $modified" -ForegroundColor Gray
            Write-Host ""
        }
    }
    else {
        Write-Host "❌ No models found or Ollama not accessible" -ForegroundColor Red
    }
}

function Start-ChatCLI {
    param([string]$Model = "llama3.2")
    
    Write-Host "Starting interactive chat with model: $Model" -ForegroundColor Cyan
    Write-Host "Type 'exit' or 'quit' to end the conversation" -ForegroundColor Yellow
    Write-Host "Type 'help' for available commands" -ForegroundColor Yellow
    Write-Host "=========================================" -ForegroundColor Gray
    
    $systemPrompt = "You are a helpful AI assistant integrated into RawrXD, a PowerShell-based text editor. Help the user with coding tasks, code analysis, and general questions."
    
    while ($true) {
        Write-Host "`n👤 You: " -ForegroundColor Blue -NoNewline
        $input = Read-Host
        
        if ($input -match '^(exit|quit)$') {
            Write-Host "👋 Goodbye!" -ForegroundColor Yellow
            break
        }
        
        if ($input -eq 'help') {
            Show-ChatHelpCLI
            continue
        }
        
        if ($input -eq 'clear') {
            Clear-Host
            Write-Host "Chat with model: $Model" -ForegroundColor Cyan
            Write-Host "=========================================" -ForegroundColor Gray
            continue
        }
        
        if ([string]::IsNullOrWhiteSpace($input)) {
            continue
        }
        
        Write-Host "🧠 AI: " -ForegroundColor Green -NoNewline
        Write-Host "Thinking..." -ForegroundColor Gray
        
        try {
            $response = Invoke-OllamaChat -Model $Model -Prompt $input -System $systemPrompt
            Write-Host "`r🧠 AI: " -ForegroundColor Green -NoNewline
            Write-Host $response -ForegroundColor White
        }
        catch {
            Write-Host "`r❌ Error: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
}

function Show-ChatHelpCLI {
    Write-Host "`nChat Commands:" -ForegroundColor Cyan
    Write-Host "=============" -ForegroundColor Cyan
    Write-Host "exit/quit - End the chat session" -ForegroundColor Yellow
    Write-Host "clear     - Clear the screen" -ForegroundColor Yellow
    Write-Host "help      - Show this help" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Tips:" -ForegroundColor Cyan
    Write-Host "- Ask questions about coding, debugging, or general topics" -ForegroundColor Gray
    Write-Host "- Request code analysis or improvements" -ForegroundColor Gray
    Write-Host "- Ask for help with PowerShell, Python, JavaScript, etc." -ForegroundColor Gray
}

function Analyze-FileCLI {
    param(
        [string]$FilePath,
        [string]$Model = "llama3.2"
    )
    
    if (-not (Test-Path $FilePath)) {
        Write-Host "❌ File not found: $FilePath" -ForegroundColor Red
        return
    }
    
    try {
        $content = Get-Content $FilePath -Raw
        $language = Get-FileExtensionLanguage -FilePath $FilePath
        $fileSize = (Get-Item $FilePath).Length
        
        Write-Host "Analyzing file: $FilePath" -ForegroundColor Cyan
        Write-Host "Language: $language" -ForegroundColor Gray
        Write-Host "Size: $(Format-FileSize -Size $fileSize)" -ForegroundColor Gray
        Write-Host "Model: $Model" -ForegroundColor Gray
        Write-Host "=================================" -ForegroundColor Gray
        
        $prompt = @"
Please analyze this $language file and provide insights about:
1. Code structure and organization
2. Potential improvements or optimizations
3. Any issues or bugs you notice
4. Best practices recommendations
5. Overall code quality assessment

File: $FilePath
Language: $language

Code:
$content
"@
        
        Write-Host "🧠 AI Analysis:" -ForegroundColor Green
        Write-Host "===============" -ForegroundColor Green
        
        $analysis = Invoke-OllamaChat -Model $Model -Prompt $prompt
        Write-Host $analysis -ForegroundColor White
        
        # Ask if user wants to save analysis
        Write-Host "`nSave analysis to file? (y/N): " -ForegroundColor Yellow -NoNewline
        $save = Read-Host
        
        if ($save -match '^[yY]') {
            $analysisPath = "$FilePath.analysis.txt"
            $analysisContent = @"
RawrXD File Analysis Report
Generated: $(Get-Date)
File: $FilePath
Language: $language
Model: $Model

$analysis
"@
            $analysisContent | Out-File -FilePath $analysisPath -Encoding UTF8
            Write-Host "✅ Analysis saved to: $analysisPath" -ForegroundColor Green
        }
    }
    catch {
        Write-Host "❌ Error analyzing file: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Show-GitStatusCLI {
    param([string]$Path = (Get-Location).Path)
    
    Write-Host "Git Status for: $Path" -ForegroundColor Cyan
    Write-Host "=====================" -ForegroundColor Cyan
    
    $gitStatus = Get-GitStatus -Path $Path
    
    if ($gitStatus.IsGitRepo) {
        Write-Host "✅ Git Repository" -ForegroundColor Green
        Write-Host "Branch: $($gitStatus.Branch)" -ForegroundColor Yellow
        Write-Host "Has Changes: $($gitStatus.HasChanges)" -ForegroundColor $(if ($gitStatus.HasChanges) { "Red" } else { "Green" })
        
        if ($gitStatus.HasChanges) {
            Write-Host "`nChanges:" -ForegroundColor Yellow
            foreach ($change in $gitStatus.Changes) {
                Write-Host "  $change" -ForegroundColor Gray
            }
        }
        else {
            Write-Host "Working tree is clean" -ForegroundColor Green
        }
    }
    else {
        Write-Host "❌ Not a Git repository" -ForegroundColor Red
    }
}

function Show-HelpCLI {
    $help = @"
RawrXD v$($global:RawrXD.Version) - AI-Powered Text Editor
Usage: .\RawrXD-New.ps1 [-CliMode] [-Command <command>] [additional parameters]

Available Commands:
==================

test-ollama       - Test Ollama AI service connection
list-models       - List available AI models
chat              - Start interactive chat session
                   [-Model <model_name>]
analyze-file      - Analyze a file with AI
                   -FilePath <file_path> [-Model <model_name>]
git-status        - Show Git repository status
                   [-FilePath <directory_path>]
help              - Show this help information

Examples:
=========

# Test AI connection
.\RawrXD-New.ps1 -CliMode -Command test-ollama

# List available models
.\RawrXD-New.ps1 -CliMode -Command list-models

# Start interactive chat
.\RawrXD-New.ps1 -CliMode -Command chat -Model llama3.2

# Analyze a PowerShell file
.\RawrXD-New.ps1 -CliMode -Command analyze-file -FilePath "script.ps1"

# Check Git status
.\RawrXD-New.ps1 -CliMode -Command git-status -FilePath "C:\MyProject"

# Start GUI (default)
.\RawrXD-New.ps1

Configuration:
==============
Settings file: RawrXD-Modules\..\Settings\config.json
Logs directory: RawrXD-Modules\..\Logs\

For more information, visit the RawrXD documentation.
"@
    
    Write-Host $help -ForegroundColor White
}

function Start-InteractiveCLI {
    Write-Host "RawrXD v$($global:RawrXD.Version) - Interactive CLI Mode" -ForegroundColor Cyan
    Write-Host "Type 'help' for available commands, 'exit' to quit" -ForegroundColor Yellow
    Write-Host "=================================================" -ForegroundColor Gray
    
    while ($true) {
        Write-Host "`nRawrXD> " -ForegroundColor Green -NoNewline
        $input = Read-Host
        
        if ($input -match '^(exit|quit)$') {
            Write-Host "Goodbye!" -ForegroundColor Yellow
            break
        }
        
        if ([string]::IsNullOrWhiteSpace($input)) {
            continue
        }
        
        $parts = $input -split ' ', 2
        $command = $parts[0].ToLower()
        $args = if ($parts.Length -gt 1) { $parts[1] } else { "" }
        
        try {
            switch ($command) {
                'help' { Show-HelpCLI }
                'test-ollama' { Test-OllamaCLI }
                'list-models' { List-ModelsCLI }
                'chat' { 
                    $model = if ($args) { $args } else { "llama3.2" }
                    Start-ChatCLI -Model $model 
                }
                'analyze' {
                    if ($args) {
                        Analyze-FileCLI -FilePath $args
                    } else {
                        Write-Host "Usage: analyze <file_path>" -ForegroundColor Yellow
                    }
                }
                'git' { 
                    $path = if ($args) { $args } else { (Get-Location).Path }
                    Show-GitStatusCLI -Path $path 
                }
                'clear' { Clear-Host }
                'pwd' { Write-Host (Get-Location).Path -ForegroundColor Yellow }
                'ls' { 
                    if ($args) { Get-ChildItem $args } else { Get-ChildItem }
                }
                default {
                    Write-Host "Unknown command: $command" -ForegroundColor Red
                    Write-Host "Type 'help' for available commands" -ForegroundColor Yellow
                }
            }
        }
        catch {
            Write-Host "Error executing command: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
}

# Export functions
Export-ModuleMember -Function @(
    'Start-RawrXDCLI',
    'Test-OllamaCLI',
    'List-ModelsCLI',
    'Start-ChatCLI',
    'Analyze-FileCLI',
    'Show-GitStatusCLI',
    'Show-HelpCLI',
    'Start-InteractiveCLI'
)