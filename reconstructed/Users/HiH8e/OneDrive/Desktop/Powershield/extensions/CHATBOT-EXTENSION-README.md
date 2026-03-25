# RawrXD ChatBot Extension Module

## Overview

The **RawrXD-ChatBot** extension is a production-ready, enterprise-level chatbot module that provides:

- ✅ **Multi-language support** (English, Spanish, French, German)
- ✅ **Terminal command integration** (executes commands via RawrXD's terminal)
- ✅ **AI integration** (falls back to Ollama for unknown intents)
- ✅ **HTTP server capabilities** (REST API for chatbot)
- ✅ **State management** (intent-based conversation flow)
- ✅ **Entity extraction** (extracts structured data from user input)
- ✅ **Conversation history** (tracks all interactions)

## What Was Missing from the Examples

The original examples provided were incomplete or had issues. This module addresses:

### 1. **Missing Integration Points**
- ❌ Examples didn't integrate with RawrXD's existing terminal system
- ❌ No connection to RawrXD's AI/Ollama integration
- ❌ Missing proper extension metadata and initialization
- ✅ **Fixed**: Full integration with `Invoke-TerminalCommand` and `Send-OllamaRequest`

### 2. **Incomplete State Management**
- ❌ Examples had basic state machines without proper context tracking
- ❌ No conversation history management
- ❌ Missing entity persistence
- ✅ **Fixed**: Complete state management with `ChatBotState` class and conversation history

### 3. **Limited Language Support**
- ❌ Examples only showed English responses
- ❌ No language switching mechanism
- ✅ **Fixed**: Multi-language support with 4 languages and easy extensibility

### 4. **HTTP Server Issues**
- ❌ Example HTTP server code had syntax errors and incomplete request handling
- ❌ No CORS support
- ❌ Missing proper JSON handling
- ✅ **Fixed**: Production-ready HTTP server with CORS, proper JSON handling, and async processing

### 5. **Missing Terminal Command Execution**
- ❌ Examples didn't show how to execute actual terminal commands
- ❌ No integration with PowerShell execution context
- ✅ **Fixed**: Full terminal command execution via RawrXD's terminal integration

### 6. **No Extension Pattern Compliance**
- ❌ Examples weren't structured as RawrXD extensions
- ❌ Missing extension metadata
- ❌ No proper initialization/cleanup
- ✅ **Fixed**: Follows RawrXD extension pattern with metadata, initialization, and cleanup

## Installation

The extension is automatically loaded when placed in the `extensions/` directory. RawrXD will detect and load it on startup.

## Usage

### Basic Usage in RawrXD

The chatbot integrates automatically with RawrXD's chat interface. Simply type messages in the chat input box:

```
You: Hello
ChatBot: Hello! How can I assist you today?

You: /term ls
ChatBot: ✅ Command executed: ls
Result: [file listing]

You: /cli test-ollama
ChatBot: ✅ CLI command executed: test-ollama
[Ollama test results...]

You: /cli video-search -Prompt python tutorial
ChatBot: ✅ CLI command executed: video-search
[Video search results...]

You: /help
ChatBot: [shows help message]
```

### Programmatic Usage

```powershell
# Initialize the extension (auto-initialized on load)
Initialize-RawrXDChatBotExtension

# Process a message
$response = Process-ChatBotMessage -Input "Hello, how are you?"
Write-Host $response

# Execute terminal command
$response = Process-ChatBotMessage -Input "/term Get-ChildItem"
Write-Host $response

# Change language
Set-ChatBotLanguage -LanguageCode "es"
$response = Process-ChatBotMessage -Input "Hola"
Write-Host $response

# Start HTTP server
Start-ChatBotHttpServer -Port 8080

# Get status
$status = Get-ChatBotStatus
Write-Host $status
```

### HTTP API Usage

Once the HTTP server is started, you can interact via REST API:

```powershell
# POST /chat
$body = @{
    message = "Hello, how are you?"
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:8080/chat" `
    -Method POST `
    -Body $body `
    -ContentType "application/json"

Write-Host $response.response

# GET /status
$status = Invoke-RestMethod -Uri "http://localhost:8080/status" -Method GET
Write-Host $status
```

## Features

### 1. Intent Detection

The chatbot uses pattern matching to detect user intents:

```powershell
# Register a custom intent
Register-ChatBotIntent -Name "weather" `
    -Patterns @("^weather.*", "^temperature.*", "^forecast.*") `
    -Response "I can help you with weather information!" `
    -RequiresEntities $false
```

### 2. Entity Extraction

Extract structured data from user input:

```powershell
# Register entity extractor
Register-ChatBotEntity -Name "Location" `
    -Type "Text" `
    -Pattern "in (\w+)"

# Usage
$entities = Extract-ChatBotEntities -Input "What's the weather in New York?"
# Returns: Location entity with value "New York"
```

### 3. Terminal Command Execution

Execute terminal commands safely:

```
User: /term Get-Process | Select-Object -First 5
ChatBot: ✅ Command executed: Get-Process | Select-Object -First 5
Result: [process list]
```

### 3a. RawrXD CLI Command Execution

Execute any RawrXD CLI command via the chatbot:

```
User: /cli test-ollama
ChatBot: ✅ CLI command executed: test-ollama
[Ollama connection test results...]

User: /cli video-search -Prompt machine learning
ChatBot: ✅ CLI command executed: video-search
[YouTube search results...]

User: /cli diagnose
ChatBot: ✅ CLI command executed: diagnose
[Diagnostic results...]

User: /cli help
ChatBot: [Shows all available CLI commands]
```

Available CLI commands include:
- **AI & Ollama**: test-ollama, list-models, chat, analyze-file
- **Agents**: create-agent, list-agents, git-status
- **Marketplace**: marketplace-sync, marketplace-search, marketplace-install
- **VSCode**: vscode-popular, vscode-search, vscode-install, copilot-status
- **Video**: video-search, video-download, video-play
- **Browser**: browser-navigate, browser-screenshot, browser-click
- **Testing**: diagnose, test-editor-settings, test-all-features
- **Settings**: get-settings, set-setting

### 4. Multi-Language Support

Switch languages on the fly:

```
User: /language es
ChatBot: Language changed to Spanish

User: Hola
ChatBot: ¡Hola! ¿Cómo puedo ayudarte hoy?
```

### 5. AI Fallback

When intent is unknown, the chatbot falls back to Ollama AI:

```powershell
# Process with AI fallback
$response = Process-ChatBotMessage -Input "What is quantum computing?" -UseAI $true
```

## Configuration

### Supported Languages

Currently supported languages:
- `en` - English
- `es` - Spanish
- `fr` - French
- `de` - German

To add more languages, modify the `$script:supportedLanguages` hashtable in the module.

### Default Intents

The module comes with these default intents:
- `greeting` - Detects greetings
- `goodbye` - Detects farewells
- `terminal_command` - Executes terminal commands
- `help` - Shows help information

### HTTP Server Port

Default port is `8080`. Change it when starting the server:

```powershell
Start-ChatBotHttpServer -Port 9000
```

## Integration with RawrXD

The extension integrates seamlessly with RawrXD:

1. **Terminal Integration**: Uses `Invoke-TerminalCommand` from `RawrXD.Terminal` module
2. **AI Integration**: Uses `Send-OllamaRequest` from `RawrXD.AI` module
3. **Logging**: Uses `Write-DevConsole` for consistent logging
4. **Extension Pattern**: Follows RawrXD extension metadata and initialization pattern

## API Reference

### Core Functions

#### `Process-ChatBotMessage`
Processes a user message and returns a response.

**Parameters:**
- `Input` (string, required) - User input message
- `UseAI` (bool, optional) - Use AI for unknown intents (default: true)
- `AIModel` (string, optional) - AI model to use (default: "llama2")

**Returns:** Response string

#### `Register-ChatBotIntent`
Registers a new intent pattern.

**Parameters:**
- `Name` (string, required) - Intent name
- `Patterns` (string[], required) - Regex patterns to match
- `Response` (string, optional) - Default response
- `Handler` (scriptblock, optional) - Custom handler
- `NextIntent` (string, optional) - Next intent to transition to
- `RequiresEntities` (bool, optional) - Whether entities are required

#### `Set-ChatBotLanguage`
Changes the chatbot language.

**Parameters:**
- `LanguageCode` (string, required) - Language code (en, es, fr, de)

#### `Start-ChatBotHttpServer`
Starts the HTTP server for REST API access.

**Parameters:**
- `Port` (int, optional) - Port number (default: 8080)

#### `Get-ChatBotStatus`
Returns current chatbot status and statistics.

## Examples

### Example 1: Custom Intent Handler

```powershell
Register-ChatBotIntent -Name "file_operation" `
    -Patterns @("^create file.*", "^make file.*") `
    -Handler {
        param($input, $entities, $state)
        # Extract filename from input
        if ($input -match "file\s+(.+)") {
            $filename = $Matches[1]
            # Create file logic here
            return "File created: $filename"
        }
        return "Please specify a filename"
    } `
    -RequiresEntities $true
```

### Example 2: Integration with RawrXD Chat

```powershell
# In RawrXD's chat handler
function Handle-ChatInput {
    param([string]$Message)
    
    # Use chatbot extension
    $response = Invoke-ChatBotFromRawrXD -Message $Message -ChatBox $chatBox -UseAI $true
    return $response
}
```

### Example 3: HTTP API Client

```powershell
# PowerShell client
function Send-ChatBotMessage {
    param([string]$Message)
    
    $body = @{ message = $Message } | ConvertTo-Json
    $response = Invoke-RestMethod -Uri "http://localhost:8080/chat" `
        -Method POST `
        -Body $body `
        -ContentType "application/json"
    
    return $response.response
}

# Usage
Send-ChatBotMessage "Hello, chatbot!"
```

## Troubleshooting

### HTTP Server Won't Start

- Check if port is already in use: `Test-NetConnection -ComputerName localhost -Port 8080`
- Ensure you have administrator privileges if using ports < 1024
- Check Windows Firewall settings

### Terminal Commands Not Executing

- Ensure `RawrXD.Terminal` module is loaded
- Check that `Invoke-TerminalCommand` function exists
- Verify command syntax is correct

### AI Fallback Not Working

- Ensure `RawrXD.AI` module is loaded
- Check that `Send-OllamaRequest` function exists
- Verify Ollama is running and accessible

## Future Enhancements

Potential improvements:
- [ ] WebSocket support for real-time chat
- [ ] Sentiment analysis integration
- [ ] Voice input/output support
- [ ] Plugin system for custom handlers
- [ ] Conversation context window management
- [ ] Multi-user support for HTTP server
- [ ] Authentication/authorization for HTTP API
- [ ] Rate limiting and abuse prevention

## License

Part of RawrXD project. See main project license.

## Support

For issues or questions:
1. Check RawrXD main documentation
2. Review extension logs via `Write-DevConsole`
3. Check HTTP server status: `Get-ChatBotStatus`

