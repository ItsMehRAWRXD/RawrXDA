# ============================================
# RawrXD ChatBot Extension Module
# ============================================
# Category: AI/Chat
# Purpose: Enterprise-level chatbot with multi-language support, terminal integration, and HTTP server capabilities
# Author: RawrXD
# Version: 1.0.0
# ============================================

# Extension Metadata
$global:RawrXDChatBotExtension = @{
    Id = "rawrxd-chatbot"
    Name = "RawrXD ChatBot"
    Description = "Production-ready chatbot with multi-language support, terminal command execution, and HTTP server capabilities"
    Author = "RawrXD Official"
    Version = "1.0.0"
    Language = 0  # LANG_CUSTOM
    Capabilities = @("ChatBot", "TerminalIntegration", "AIIntegration", "HTTPServer", "MultiLanguage")
    EditorType = $null
    Dependencies = @()
    Enabled = $true
}

# ============================================
# REQUIRED ASSEMBLIES
# ============================================
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Net.Http
Add-Type -AssemblyName System.Web

# Try to load System.Web.Extensions for JSON handling
try {
    $webExtensionsPath = "C:\Windows\Microsoft.NET\Framework\v4.0.30319\System.Web.Extensions.dll"
    if (Test-Path $webExtensionsPath) {
        Add-Type -Path $webExtensionsPath
    }
}
catch {
    Write-DevConsole "⚠️ System.Web.Extensions not available, using native JSON handling" "WARNING"
}

# ============================================
# CHATBOT STATE MANAGEMENT CLASSES
# ============================================

class ChatBotState {
    [string]$CurrentIntent
    [array]$Entities
    [hashtable]$Context
    [string]$Language
    [datetime]$LastActivity

    ChatBotState([string]$currentIntent, [array]$entities, [hashtable]$context, [string]$language) {
        $this.CurrentIntent = $currentIntent
        $this.Entities = $entities
        $this.Context = $context
        $this.Language = $language
        $this.LastActivity = Get-Date
    }
}

class ChatBotIntent {
    [string]$Name
    [string[]]$Patterns
    [string]$Response
    [scriptblock]$Handler
    [string]$NextIntent
    [bool]$RequiresEntities

    ChatBotIntent([string]$name, [string[]]$patterns, [string]$response, [scriptblock]$handler, [string]$nextIntent, [bool]$requiresEntities) {
        $this.Name = $name
        $this.Patterns = $patterns
        $this.Response = $response
        $this.Handler = $handler
        $this.NextIntent = $nextIntent
        $this.RequiresEntities = $requiresEntities
    }
}

class ChatBotEntity {
    [string]$Name
    [string]$Type
    [string]$Value
    [string]$Pattern

    ChatBotEntity([string]$name, [string]$type, [string]$value, [string]$pattern) {
        $this.Name = $name
        $this.Type = $type
        $this.Value = $value
        $this.Pattern = $pattern
    }
}

# ============================================
# CHATBOT DATA STRUCTURES
# ============================================

$script:chatBotState = $null
$script:chatBotIntents = @{}
$script:chatBotEntities = @{}
$script:chatBotConversationHistory = @()
$script:chatBotLanguage = "en"
$script:chatBotHttpListener = $null
$script:chatBotHttpServerRunning = $false
$script:chatBotHttpPort = 8080

# Language support mappings
$script:supportedLanguages = @{
    "en" = @{
        Name = "English"
        Greeting = "Hello! How can I assist you today?"
        Goodbye = "Goodbye! Have a great day!"
        Unknown = "Sorry, I didn't understand that."
        Processing = "Processing your request..."
    }
    "es" = @{
        Name = "Spanish"
        Greeting = "¡Hola! ¿Cómo puedo ayudarte hoy?"
        Goodbye = "¡Adiós! ¡Que tengas un gran día!"
        Unknown = "Lo siento, no entendí eso."
        Processing = "Procesando tu solicitud..."
    }
    "fr" = @{
        Name = "French"
        Greeting = "Bonjour! Comment puis-je vous aider aujourd'hui?"
        Goodbye = "Au revoir! Passez une excellente journée!"
        Unknown = "Désolé, je n'ai pas compris cela."
        Processing = "Traitement de votre demande..."
    }
    "de" = @{
        Name = "German"
        Greeting = "Hallo! Wie kann ich Ihnen heute helfen?"
        Goodbye = "Auf Wiedersehen! Haben Sie einen schönen Tag!"
        Unknown = "Entschuldigung, das habe ich nicht verstanden."
        Processing = "Ihre Anfrage wird bearbeitet..."
    }
}

# Default intents
$script:defaultIntents = @(
    @{
        Name = "greeting"
        Patterns = @("^hello.*", "^hi.*", "^hey.*", "^greetings.*")
        Response = "greeting"
        RequiresEntities = $false
    },
    @{
        Name = "goodbye"
        Patterns = @("^goodbye.*", "^bye.*", "^see you.*", "^farewell.*")
        Response = "goodbye"
        RequiresEntities = $false
    },
    @{
        Name = "terminal_command"
        Patterns = @("^/term\s+(.+)$", "^/terminal\s+(.+)$", "^/exec\s+(.+)$", "^(?:run|execute)\s+(?:command\s+)?(.+)")
        Response = "terminal_execution"
        RequiresEntities = $true
    },
    @{
        Name = "cli_command"
        Patterns = @("^/cli\s+(.+)$", "^cli\s+(.+)$", "^rawrxd\s+(.+)$")
        Response = "cli_execution"
        RequiresEntities = $true
    },
    @{
        Name = "help"
        Patterns = @("^help.*", "^what can you do.*", "^commands.*", "^assist.*")
        Response = "help"
        RequiresEntities = $false
    }
)

# RawrXD CLI commands mapping
$script:rawrxdCliCommands = @{
    "test-ollama" = @{ Description = "Test Ollama connection"; Params = @() }
    "list-models" = @{ Description = "List available Ollama models"; Params = @() }
    "chat" = @{ Description = "Start interactive chat"; Params = @("Model") }
    "analyze-file" = @{ Description = "Analyze file with AI"; Params = @("FilePath", "Model") }
    "git-status" = @{ Description = "Show git status"; Params = @() }
    "create-agent" = @{ Description = "Create agent task"; Params = @("AgentName", "Prompt") }
    "list-agents" = @{ Description = "List agent tasks"; Params = @() }
    "marketplace-sync" = @{ Description = "Sync marketplace"; Params = @() }
    "marketplace-search" = @{ Description = "Search marketplace"; Params = @("Prompt") }
    "marketplace-install" = @{ Description = "Install extension"; Params = @("Prompt") }
    "list-extensions" = @{ Description = "List extensions"; Params = @() }
    "vscode-popular" = @{ Description = "Get popular VSCode extensions"; Params = @() }
    "vscode-search" = @{ Description = "Search VSCode marketplace"; Params = @("Prompt") }
    "vscode-install" = @{ Description = "Install VSCode extension"; Params = @("Prompt") }
    "vscode-categories" = @{ Description = "Browse VSCode categories"; Params = @() }
    "copilot-status" = @{ Description = "Check Copilot status"; Params = @("Prompt") }
    "diagnose" = @{ Description = "Run diagnostics"; Params = @() }
    "test-editor-settings" = @{ Description = "Test editor settings"; Params = @() }
    "test-file-operations" = @{ Description = "Test file operations"; Params = @() }
    "test-settings-persistence" = @{ Description = "Test settings persistence"; Params = @() }
    "test-visibility" = @{ Description = "Test text visibility"; Params = @() }
    "check-editor-visibility" = @{ Description = "Check editor visibility"; Params = @() }
    "test-all-features" = @{ Description = "Test all features"; Params = @() }
    "get-settings" = @{ Description = "Get settings"; Params = @("SettingName") }
    "set-setting" = @{ Description = "Set setting"; Params = @("SettingName", "SettingValue") }
    "test-gui" = @{ Description = "Test GUI features"; Params = @() }
    "test-gui-interactive" = @{ Description = "Interactive GUI test"; Params = @() }
    "test-dropdowns" = @{ Description = "Test dropdowns"; Params = @() }
    "video-search" = @{ Description = "Search YouTube videos"; Params = @("Prompt") }
    "video-download" = @{ Description = "Download video"; Params = @("URL", "OutputPath") }
    "video-play" = @{ Description = "Play video"; Params = @("URL") }
    "video-help" = @{ Description = "Video engine help"; Params = @() }
    "browser-navigate" = @{ Description = "Navigate browser"; Params = @("URL") }
    "browser-screenshot" = @{ Description = "Take screenshot"; Params = @("OutputPath") }
    "browser-click" = @{ Description = "Click browser element"; Params = @("Selector") }
    "help" = @{ Description = "Show help"; Params = @() }
}

# ============================================
# INITIALIZATION
# ============================================

function Initialize-RawrXDChatBotExtension {
    <#
    .SYNOPSIS
        Initializes the ChatBot extension module
    #>
    
    try {
        Write-DevConsole "🤖 Initializing RawrXD ChatBot Extension..." "INFO"
        
        # Initialize state machine
        $script:chatBotState = [ChatBotState]::new("greeting", @(), @{}, $script:chatBotLanguage)
        
        # Register default intents
        foreach ($intentDef in $script:defaultIntents) {
            $handler = {
                param($input, $entities, $state)
                return $intentDef.Response
            }
            
            $intent = [ChatBotIntent]::new(
                $intentDef.Name,
                $intentDef.Patterns,
                $intentDef.Response,
                $handler,
                $null,
                $intentDef.RequiresEntities
            )
            
            $script:chatBotIntents[$intentDef.Name] = $intent
        }
        
        # Register entity extractors
        Register-ChatBotEntity -Name "Command" -Type "Text" -Pattern "(.+)"
        Register-ChatBotEntity -Name "Name" -Type "Text" -Pattern "name is (\w+)"
        Register-ChatBotEntity -Name "Age" -Type "Number" -Pattern "(\d+)"
        
        Write-DevConsole "✅ ChatBot Extension initialized successfully" "SUCCESS"
        Write-DevConsole "   - Language: $($script:supportedLanguages[$script:chatBotLanguage].Name)" "INFO"
        Write-DevConsole "   - Intents registered: $($script:chatBotIntents.Count)" "INFO"
        Write-DevConsole "   - Entities registered: $($script:chatBotEntities.Count)" "INFO"
        
        return $true
    }
    catch {
        Write-DevConsole "❌ Failed to initialize ChatBot Extension: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

# ============================================
# INTENT AND ENTITY MANAGEMENT
# ============================================

function Register-ChatBotIntent {
    <#
    .SYNOPSIS
        Registers a new intent for the chatbot
    .PARAMETER Name
        Name of the intent
    .PARAMETER Patterns
        Array of regex patterns to match this intent
    .PARAMETER Response
        Default response text or response key
    .PARAMETER Handler
        Scriptblock to handle the intent
    .PARAMETER NextIntent
        Next intent to transition to
    .PARAMETER RequiresEntities
        Whether this intent requires entity extraction
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        
        [Parameter(Mandatory = $true)]
        [string[]]$Patterns,
        
        [Parameter(Mandatory = $false)]
        [string]$Response = "",
        
        [Parameter(Mandatory = $false)]
        [scriptblock]$Handler,
        
        [Parameter(Mandatory = $false)]
        [string]$NextIntent = $null,
        
        [Parameter(Mandatory = $false)]
        [bool]$RequiresEntities = $false
    )
    
    try {
        if (-not $Handler) {
            $Handler = {
                param($input, $entities, $state)
                return $Response
            }
        }
        
        $intent = [ChatBotIntent]::new($Name, $Patterns, $Response, $Handler, $NextIntent, $RequiresEntities)
        $script:chatBotIntents[$Name] = $intent
        
        Write-DevConsole "✅ Registered intent: $Name" "SUCCESS"
        return $true
    }
    catch {
        Write-DevConsole "❌ Failed to register intent $Name : $($_.Exception.Message)" "ERROR"
        return $false
    }
}

function Register-ChatBotEntity {
    <#
    .SYNOPSIS
        Registers a new entity extractor
    .PARAMETER Name
        Name of the entity
    .PARAMETER Type
        Type of entity (Text, Number, Date, etc.)
    .PARAMETER Pattern
        Regex pattern to extract the entity
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        
        [Parameter(Mandatory = $true)]
        [string]$Type,
        
        [Parameter(Mandatory = $true)]
        [string]$Pattern
    )
    
    try {
        $entity = [ChatBotEntity]::new($Name, $Type, "", $Pattern)
        $script:chatBotEntities[$Name] = $entity
        
        Write-DevConsole "✅ Registered entity extractor: $Name" "SUCCESS"
        return $true
    }
    catch {
        Write-DevConsole "❌ Failed to register entity $Name : $($_.Exception.Message)" "ERROR"
        return $false
    }
}

# ============================================
# INTENT DETECTION AND ENTITY EXTRACTION
# ============================================

function Detect-ChatBotIntent {
    <#
    .SYNOPSIS
        Detects the intent from user input
    .PARAMETER Input
        User input text
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Input
    )
    
    $normalizedInput = $Input.Trim().ToLower()
    
    foreach ($intentName in $script:chatBotIntents.Keys) {
        $intent = $script:chatBotIntents[$intentName]
        
        foreach ($pattern in $intent.Patterns) {
            if ($normalizedInput -match $pattern) {
                return @{
                    Intent = $intent
                    Match = $Matches
                }
            }
        }
    }
    
    return $null
}

function Extract-ChatBotEntities {
    <#
    .SYNOPSIS
        Extracts entities from user input
    .PARAMETER Input
        User input text
    .PARAMETER Intent
        Detected intent
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Input,
        
        [Parameter(Mandatory = $false)]
        [ChatBotIntent]$Intent = $null
    )
    
    $extractedEntities = @()
    
    foreach ($entityName in $script:chatBotEntities.Keys) {
        $entity = $script:chatBotEntities[$entityName]
        
        if ($Input -match $entity.Pattern) {
            $value = $Matches[1]
            $extractedEntity = [ChatBotEntity]::new($entityName, $entity.Type, $value, $entity.Pattern)
            $extractedEntities += $extractedEntity
        }
    }
    
    return $extractedEntities
}

# ============================================
# RESPONSE GENERATION
# ============================================

function Generate-ChatBotResponse {
    <#
    .SYNOPSIS
        Generates a response based on intent and entities
    .PARAMETER Intent
        Detected intent
    .PARAMETER Entities
        Extracted entities
    .PARAMETER Input
        Original user input
    #>
    param(
        [Parameter(Mandatory = $true)]
        [ChatBotIntent]$Intent,
        
        [Parameter(Mandatory = $false)]
        [array]$Entities = @(),
        
        [Parameter(Mandatory = $false)]
        [string]$Input = ""
    )
    
    try {
        # Get language-specific responses
        $lang = $script:chatBotLanguage
        $langData = $script:supportedLanguages[$lang]
        
        # Handle specific intents
        switch ($Intent.Name.ToLower()) {
            "greeting" {
                return $langData.Greeting
            }
            "goodbye" {
                return $langData.Goodbye
            }
            "terminal_command" {
                # Extract command from entities
                $commandEntity = $Entities | Where-Object { $_.Name -eq "Command" }
                if ($commandEntity) {
                    $command = $commandEntity.Value
                    return Invoke-ChatBotTerminalCommand -Command $command
                }
                return "Please specify a command to execute."
            }
            "cli_command" {
                # Extract CLI command from entities
                $commandEntity = $Entities | Where-Object { $_.Name -eq "Command" }
                if ($commandEntity) {
                    $cliCommand = $commandEntity.Value.Trim()
                    return Invoke-ChatBotCliCommand -Command $cliCommand -Input $Input
                }
                return "Please specify a RawrXD CLI command. Use '/cli help' for available commands."
            }
            "help" {
                return Get-ChatBotHelpMessage
            }
            default {
                # Try to use the intent's handler
                if ($Intent.Handler) {
                    $result = & $Intent.Handler -Input $Input -Entities $Entities -State $script:chatBotState
                    if ($result) {
                        return $result
                    }
                }
                
                # Fallback to language-specific unknown response
                return $langData.Unknown
            }
        }
    }
    catch {
        Write-DevConsole "❌ Error generating response: $($_.Exception.Message)" "ERROR"
        return $script:supportedLanguages[$script:chatBotLanguage].Unknown
    }
}

function Get-ChatBotHelpMessage {
    <#
    .SYNOPSIS
        Returns help message with available commands
    #>
    
    $cliCommandsList = ($script:rawrxdCliCommands.Keys | Select-Object -First 10) -join ", "
    
    $helpText = @"
🤖 RawrXD ChatBot Help

Available Commands:
  • /term <command>     - Execute terminal command
  • /terminal <command> - Execute terminal command (alias)
  • /exec <command>    - Execute command (alias)
  • /cli <command>     - Execute RawrXD CLI command
  • /help              - Show this help message
  • /language <code>   - Change language (en, es, fr, de)
  • /start-server      - Start HTTP server
  • /stop-server       - Stop HTTP server
  • /status            - Show chatbot status

RawrXD CLI Commands (via /cli):
  • test-ollama, list-models, chat, analyze-file
  • git-status, create-agent, list-agents
  • marketplace-sync, marketplace-search, marketplace-install
  • vscode-popular, vscode-search, vscode-install
  • video-search, video-download, video-play
  • browser-navigate, browser-screenshot, browser-click
  • diagnose, get-settings, set-setting
  • ... and more! Use "/cli help" for full list

Natural Language:
  • "run ls"             - Execute terminal command
  • "execute dir"        - Execute terminal command
  • "hello"              - Greeting
  • "goodbye"            - End conversation

Current Language: $($script:supportedLanguages[$script:chatBotLanguage].Name)
"@
    
    return $helpText
}

# ============================================
# TERMINAL COMMAND INTEGRATION
# ============================================

function Invoke-ChatBotTerminalCommand {
    <#
    .SYNOPSIS
        Executes a terminal command via RawrXD's terminal integration
    .PARAMETER Command
        Command to execute
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )
    
    try {
        Write-DevConsole "🔧 Executing terminal command: $Command" "INFO"
        
        # Check if Invoke-TerminalCommand function exists (from RawrXD.Terminal module)
        if (Get-Command -Name "Invoke-TerminalCommand" -ErrorAction SilentlyContinue) {
            $result = Invoke-TerminalCommand -Command $Command
            return "✅ Command executed: $Command`nResult: $result"
        }
        else {
            # Fallback to direct PowerShell execution
            $output = Invoke-Expression $Command 2>&1 | Out-String
            return "✅ Command executed: $Command`nOutput: $output"
        }
    }
    catch {
        Write-DevConsole "❌ Error executing command: $($_.Exception.Message)" "ERROR"
        return "❌ Error executing command: $($_.Exception.Message)"
    }
}

# ============================================
# RAWRXD CLI COMMAND INTEGRATION
# ============================================

function Invoke-ChatBotCliCommand {
    <#
    .SYNOPSIS
        Executes a RawrXD CLI command
    .PARAMETER Command
        CLI command to execute
    .PARAMETER Input
        Original user input for parameter extraction
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        
        [Parameter(Mandatory = $false)]
        [string]$Input = ""
    )
    
    try {
        Write-DevConsole "🔧 Executing RawrXD CLI command: $Command" "INFO"
        
        # Parse command and parameters
        $commandParts = $Command -split '\s+', 2
        $cliCommandName = $commandParts[0].ToLower()
        $commandArgs = if ($commandParts.Count -gt 1) { $commandParts[1] } else { "" }
        
        # Handle help command
        if ($cliCommandName -eq "help") {
            return Get-ChatBotCliHelpMessage
        }
        
        # Check if command exists
        if (-not $script:rawrxdCliCommands.ContainsKey($cliCommandName)) {
            $availableCommands = ($script:rawrxdCliCommands.Keys | Select-Object -First 20) -join ", "
            return "❌ Unknown CLI command: $cliCommandName`n`nAvailable commands: $availableCommands`n`nUse '/cli help' for full list."
        }
        
        $commandInfo = $script:rawrxdCliCommands[$cliCommandName]
        
        # Build PowerShell command to execute RawrXD CLI
        $rawrxdScript = if ($PSScriptRoot) {
            Join-Path $PSScriptRoot "..\RawrXD.ps1"
        } else {
            ".\RawrXD.ps1"
        }
        
        # Check if RawrXD.ps1 exists
        if (-not (Test-Path $rawrxdScript)) {
            # Try alternative paths
            $rawrxdScript = Join-Path $env:USERPROFILE "Desktop\Powershield\RawrXD.ps1"
            if (-not (Test-Path $rawrxdScript)) {
                return "❌ RawrXD.ps1 not found. Cannot execute CLI commands."
            }
        }
        
        # Build command arguments
        $psArgs = @("-CliMode", "-Command", $cliCommandName)
        
        # Extract parameters from command args or input
        if ($commandInfo.Params) {
            foreach ($param in $commandInfo.Params) {
                # Try to extract parameter value from command args
                $paramPattern = "-$param\s+([^\s]+)"
                if ($commandArgs -match $paramPattern) {
                    $paramValue = $Matches[1]
                    $psArgs += "-$param", $paramValue
                }
                # Try natural language extraction
                elseif ($Input -match "$param\s+(?:is\s+)?([^\s]+)") {
                    $paramValue = $Matches[1]
                    $psArgs += "-$param", $paramValue
                }
            }
        }
        
        # Execute the command
        Write-DevConsole "Executing: $rawrxdScript $($psArgs -join ' ')" "INFO"
        
        # Capture output
        $output = & $rawrxdScript @psArgs 2>&1 | Out-String
        
        if ($LASTEXITCODE -eq 0) {
            return "✅ CLI command executed: $cliCommandName`n`n$output"
        }
        else {
            return "⚠️ CLI command completed with exit code $LASTEXITCODE: $cliCommandName`n`n$output"
        }
    }
    catch {
        Write-DevConsole "❌ Error executing CLI command: $($_.Exception.Message)" "ERROR"
        return "❌ Error executing CLI command: $($_.Exception.Message)"
    }
}

function Get-ChatBotCliHelpMessage {
    <#
    .SYNOPSIS
        Returns help message for RawrXD CLI commands
    #>
    
    $helpText = "🤖 RawrXD CLI Commands Available via ChatBot:`n`n"
    
    # Group commands by category
    $categories = @{
        "AI & Ollama" = @("test-ollama", "list-models", "chat", "analyze-file")
        "Agents" = @("create-agent", "list-agents", "git-status")
        "Marketplace" = @("marketplace-sync", "marketplace-search", "marketplace-install", "list-extensions")
        "VSCode" = @("vscode-popular", "vscode-search", "vscode-install", "vscode-categories", "copilot-status")
        "Video" = @("video-search", "video-download", "video-play", "video-help")
        "Browser" = @("browser-navigate", "browser-screenshot", "browser-click")
        "Testing" = @("diagnose", "test-editor-settings", "test-file-operations", "test-all-features")
        "Settings" = @("get-settings", "set-setting")
    }
    
    foreach ($category in $categories.Keys) {
        $helpText += "📁 $category`n"
        foreach ($cmd in $categories[$category]) {
            if ($script:rawrxdCliCommands.ContainsKey($cmd)) {
                $desc = $script:rawrxdCliCommands[$cmd].Description
                $helpText += "  • $cmd - $desc`n"
            }
        }
        $helpText += "`n"
    }
    
    $helpText += "Usage: /cli <command> [parameters]`n"
    $helpText += "Example: /cli video-search -Prompt 'python tutorial'`n"
    $helpText += "Example: /cli test-ollama`n"
    
    return $helpText
}

# ============================================
# MAIN CHATBOT PROCESSING
# ============================================

function Process-ChatBotMessage {
    <#
    .SYNOPSIS
        Processes a user message and returns a response
    .PARAMETER Input
        User input message
    .PARAMETER UseAI
        Whether to use AI (Ollama) for responses when intent is unknown
    .PARAMETER AIModel
        AI model to use (default: llama2)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Input,
        
        [Parameter(Mandatory = $false)]
        [bool]$UseAI = $true,
        
        [Parameter(Mandatory = $false)]
        [string]$AIModel = "llama2"
    )
    
    try {
        # Add to conversation history
        $script:chatBotConversationHistory += @{
            Role = "user"
            Content = $Input
            Timestamp = Get-Date
        }
        
        # Check for special commands
        if ($Input -match "^/language\s+(\w+)$") {
            $langCode = $Matches[1].ToLower()
            if ($script:supportedLanguages.ContainsKey($langCode)) {
                $script:chatBotLanguage = $langCode
                $script:chatBotState.Language = $langCode
                $response = "Language changed to $($script:supportedLanguages[$langCode].Name)"
                $script:chatBotConversationHistory += @{
                    Role = "assistant"
                    Content = $response
                    Timestamp = Get-Date
                }
                return $response
            }
            else {
                return "Unsupported language. Available: $($script:supportedLanguages.Keys -join ', ')"
            }
        }
        
        if ($Input -match "^/start-server$") {
            return Start-ChatBotHttpServer
        }
        
        if ($Input -match "^/stop-server$") {
            return Stop-ChatBotHttpServer
        }
        
        if ($Input -match "^/status$") {
            return Get-ChatBotStatus
        }
        
        # Handle CLI help
        if ($Input -match "^/cli\s+help$" -or $Input -match "^cli\s+help$") {
            return Get-ChatBotCliHelpMessage
        }
        
        # Detect intent
        $intentResult = Detect-ChatBotIntent -Input $Input
        
        if ($intentResult) {
            $intent = $intentResult.Intent
            $matchData = $intentResult.Match
            
            # Extract entities
            $entities = Extract-ChatBotEntities -Input $Input -Intent $intent
            
            # Generate response
            $response = Generate-ChatBotResponse -Intent $intent -Entities $entities -Input $Input
            
            # Update state
            $script:chatBotState.CurrentIntent = $intent.Name
            $script:chatBotState.Entities = $entities
            $script:chatBotState.LastActivity = Get-Date
            
            # Add to conversation history
            $script:chatBotConversationHistory += @{
                Role = "assistant"
                Content = $response
                Timestamp = Get-Date
            }
            
            return $response
        }
        else {
            # Unknown intent - use AI if available
            if ($UseAI) {
                if (Get-Command -Name "Send-OllamaRequest" -ErrorAction SilentlyContinue) {
                    $response = Send-OllamaRequest -Prompt $Input -Model $AIModel
                    $script:chatBotConversationHistory += @{
                        Role = "assistant"
                        Content = $response
                        Timestamp = Get-Date
                    }
                    return $response
                }
            }
            
            # Fallback to default unknown response
            $response = $script:supportedLanguages[$script:chatBotLanguage].Unknown
            $script:chatBotConversationHistory += @{
                Role = "assistant"
                Content = $response
                Timestamp = Get-Date
            }
            return $response
        }
    }
    catch {
        Write-DevConsole "❌ Error processing message: $($_.Exception.Message)" "ERROR"
        return "❌ Error: $($_.Exception.Message)"
    }
}

# ============================================
# HTTP SERVER FUNCTIONALITY
# ============================================

function Start-ChatBotHttpServer {
    <#
    .SYNOPSIS
        Starts the HTTP server for chatbot API
    .PARAMETER Port
        Port to listen on (default: 8080)
    #>
    param(
        [Parameter(Mandatory = $false)]
        [int]$Port = 8080
    )
    
    try {
        if ($script:chatBotHttpServerRunning) {
            return "HTTP server is already running on port $script:chatBotHttpPort"
        }
        
        $script:chatBotHttpPort = $Port
        $script:chatBotHttpListener = New-Object System.Net.HttpListener
        $script:chatBotHttpListener.Prefixes.Add("http://localhost:$Port/")
        $script:chatBotHttpListener.Start()
        $script:chatBotHttpServerRunning = $true
        
        Write-DevConsole "🌐 HTTP server started on port $Port" "SUCCESS"
        
        # Start async listener
        $job = Start-Job -ScriptBlock {
            param($listener, $port)
            
            while ($listener.IsListening) {
                try {
                    $context = $listener.GetContext()
                    $request = $context.Request
                    $response = $context.Response
                    
                    # Handle CORS
                    $response.AddHeader("Access-Control-Allow-Origin", "*")
                    $response.AddHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
                    $response.AddHeader("Access-Control-Allow-Headers", "Content-Type")
                    
                    if ($request.HttpMethod -eq "OPTIONS") {
                        $response.StatusCode = 200
                        $response.Close()
                        continue
                    }
                    
                    # Handle POST requests (chat messages)
                    if ($request.HttpMethod -eq "POST" -and $request.Url.AbsolutePath -eq "/chat") {
                        $reader = New-Object System.IO.StreamReader($request.InputStream)
                        $body = $reader.ReadToEnd()
                        $reader.Close()
                        
                        # Parse JSON (simple parsing)
                        $data = $body | ConvertFrom-Json
                        $message = $data.message
                        
                        # Process message (would need to import functions)
                        $chatResponse = "Chat response for: $message"
                        
                        $response.ContentType = "application/json"
                        $response.StatusCode = 200
                        $jsonResponse = @{
                            response = $chatResponse
                            timestamp = (Get-Date).ToString("o")
                        } | ConvertTo-Json
                        
                        $buffer = [System.Text.Encoding]::UTF8.GetBytes($jsonResponse)
                        $response.ContentLength64 = $buffer.Length
                        $response.OutputStream.Write($buffer, 0, $buffer.Length)
                        $response.Close()
                    }
                    # Handle GET requests (status)
                    elseif ($request.HttpMethod -eq "GET" -and $request.Url.AbsolutePath -eq "/status") {
                        $response.ContentType = "application/json"
                        $response.StatusCode = 200
                        $jsonResponse = @{
                            status = "running"
                            port = $port
                            language = $using:script:chatBotLanguage
                        } | ConvertTo-Json
                        
                        $buffer = [System.Text.Encoding]::UTF8.GetBytes($jsonResponse)
                        $response.ContentLength64 = $buffer.Length
                        $response.OutputStream.Write($buffer, 0, $buffer.Length)
                        $response.Close()
                    }
                    else {
                        $response.StatusCode = 404
                        $response.Close()
                    }
                }
                catch {
                    # Continue listening
                }
            }
        } -ArgumentList $script:chatBotHttpListener, $Port
        
        return "✅ HTTP server started on http://localhost:$Port"
    }
    catch {
        Write-DevConsole "❌ Failed to start HTTP server: $($_.Exception.Message)" "ERROR"
        return "❌ Failed to start HTTP server: $($_.Exception.Message)"
    }
}

function Stop-ChatBotHttpServer {
    <#
    .SYNOPSIS
        Stops the HTTP server
    #>
    
    try {
        if (-not $script:chatBotHttpServerRunning) {
            return "HTTP server is not running"
        }
        
        if ($script:chatBotHttpListener) {
            $script:chatBotHttpListener.Stop()
            $script:chatBotHttpListener.Close()
            $script:chatBotHttpListener = $null
        }
        
        $script:chatBotHttpServerRunning = $false
        Write-DevConsole "🛑 HTTP server stopped" "INFO"
        return "✅ HTTP server stopped"
    }
    catch {
        Write-DevConsole "❌ Error stopping HTTP server: $($_.Exception.Message)" "ERROR"
        return "❌ Error: $($_.Exception.Message)"
    }
}

# ============================================
# STATUS AND UTILITY FUNCTIONS
# ============================================

function Get-ChatBotStatus {
    <#
    .SYNOPSIS
        Returns current chatbot status
    #>
    
    $status = @{
        Language = $script:chatBotLanguage
        LanguageName = $script:supportedLanguages[$script:chatBotLanguage].Name
        IntentsRegistered = $script:chatBotIntents.Count
        EntitiesRegistered = $script:chatBotEntities.Count
        ConversationHistoryCount = $script:chatBotConversationHistory.Count
        HttpServerRunning = $script:chatBotHttpServerRunning
        HttpServerPort = if ($script:chatBotHttpServerRunning) { $script:chatBotHttpPort } else { $null }
        CurrentIntent = $script:chatBotState.CurrentIntent
        LastActivity = $script:chatBotState.LastActivity
    }
    
    $statusText = @"
🤖 ChatBot Status

Language: $($status.LanguageName) ($($status.Language))
Intents: $($status.IntentsRegistered)
Entities: $($status.EntitiesRegistered)
Conversation History: $($status.ConversationHistoryCount) messages
HTTP Server: $(if ($status.HttpServerRunning) { "Running on port $($status.HttpServerPort)" } else { "Stopped" })
Current Intent: $($status.CurrentIntent)
Last Activity: $($status.LastActivity)
"@
    
    return $statusText
}

function Set-ChatBotLanguage {
    <#
    .SYNOPSIS
        Sets the chatbot language
    .PARAMETER LanguageCode
        Language code (en, es, fr, de)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$LanguageCode
    )
    
    $langCode = $LanguageCode.ToLower()
    if ($script:supportedLanguages.ContainsKey($langCode)) {
        $script:chatBotLanguage = $langCode
        $script:chatBotState.Language = $langCode
        Write-DevConsole "🌐 Language changed to $($script:supportedLanguages[$langCode].Name)" "SUCCESS"
        return $true
    }
    else {
        Write-DevConsole "❌ Unsupported language code: $langCode" "ERROR"
        return $false
    }
}

function Get-ChatBotConversationHistory {
    <#
    .SYNOPSIS
        Returns conversation history
    .PARAMETER Limit
        Maximum number of messages to return
    #>
    param(
        [Parameter(Mandatory = $false)]
        [int]$Limit = 50
    )
    
    if ($script:chatBotConversationHistory.Count -eq 0) {
        return @()
    }
    
    $startIndex = [Math]::Max(0, $script:chatBotConversationHistory.Count - $Limit)
    return $script:chatBotConversationHistory[$startIndex..($script:chatBotConversationHistory.Count - 1)]
}

function Clear-ChatBotConversationHistory {
    <#
    .SYNOPSIS
        Clears conversation history
    #>
    
    $script:chatBotConversationHistory = @()
    Write-DevConsole "🗑️ Conversation history cleared" "INFO"
    return $true
}

# ============================================
# INTEGRATION WITH RAWRXD CHAT INTERFACE
# ============================================

function Invoke-ChatBotFromRawrXD {
    <#
    .SYNOPSIS
        Main entry point for processing messages from RawrXD chat interface
    .PARAMETER Message
        User message from chat input
    .PARAMETER ChatBox
        RichTextBox control to append responses
    .PARAMETER UseAI
        Whether to use AI for unknown intents
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,
        
        [Parameter(Mandatory = $false)]
        [System.Windows.Forms.RichTextBox]$ChatBox = $null,
        
        [Parameter(Mandatory = $false)]
        [bool]$UseAI = $true
    )
    
    try {
        # Process the message
        $response = Process-ChatBotMessage -Input $Message -UseAI $UseAI
        
        # Append to chat box if provided
        if ($ChatBox -and -not $ChatBox.IsDisposed) {
            $ChatBox.AppendText("You: $Message`r`n")
            $ChatBox.AppendText("ChatBot: $response`r`n`r`n")
            $ChatBox.ScrollToCaret()
        }
        
        return $response
    }
    catch {
        Write-DevConsole "❌ Error in ChatBot integration: $($_.Exception.Message)" "ERROR"
        if ($ChatBox -and -not $ChatBox.IsDisposed) {
            $ChatBox.AppendText("ChatBot Error: $($_.Exception.Message)`r`n`r`n")
        }
        return "Error: $($_.Exception.Message)"
    }
}

# ============================================
# CLEANUP
# ============================================

function Remove-RawrXDChatBotExtension {
    <#
    .SYNOPSIS
        Cleanup and remove the ChatBot extension
    #>
    
    try {
        # Stop HTTP server if running
        if ($script:chatBotHttpServerRunning) {
            Stop-ChatBotHttpServer | Out-Null
        }
        
        # Clear state
        $script:chatBotState = $null
        $script:chatBotIntents = @{}
        $script:chatBotEntities = @{}
        $script:chatBotConversationHistory = @()
        
        Write-DevConsole "✅ ChatBot Extension removed" "SUCCESS"
        return $true
    }
    catch {
        Write-DevConsole "❌ Error removing ChatBot Extension: $($_.Exception.Message)" "ERROR"
        return $false
    }
}

# ============================================
# EXPORT FUNCTIONS
# ============================================

Export-ModuleMember -Function @(
    "Initialize-RawrXDChatBotExtension",
    "Process-ChatBotMessage",
    "Invoke-ChatBotFromRawrXD",
    "Register-ChatBotIntent",
    "Register-ChatBotEntity",
    "Detect-ChatBotIntent",
    "Extract-ChatBotEntities",
    "Generate-ChatBotResponse",
    "Invoke-ChatBotTerminalCommand",
    "Invoke-ChatBotCliCommand",
    "Get-ChatBotCliHelpMessage",
    "Start-ChatBotHttpServer",
    "Stop-ChatBotHttpServer",
    "Get-ChatBotStatus",
    "Set-ChatBotLanguage",
    "Get-ChatBotConversationHistory",
    "Clear-ChatBotConversationHistory",
    "Get-ChatBotHelpMessage",
    "Remove-RawrXDChatBotExtension"
) -Variable @("RawrXDChatBotExtension")

# Auto-initialize on module load
Initialize-RawrXDChatBotExtension

