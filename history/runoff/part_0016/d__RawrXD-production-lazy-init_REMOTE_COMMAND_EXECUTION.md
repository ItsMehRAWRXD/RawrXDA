# RawrXD CLI - Remote Command Execution Guide

## Overview

The RawrXD CLI now supports **full remote command execution** through a REST API, allowing external tools like GitHub Copilot, Amazon Q, and VS Code extensions to run commands and get results without user interaction.

**Key Features:**
- ✅ Execute CLI commands remotely (help, analyze, search, status, etc.)
- ✅ Real-time command output with status tracking
- ✅ Persistent API connection (non-blocking execution)
- ✅ OpenAI-compatible chat and completion endpoints
- ✅ Project analysis and code search
- ✅ Full integration with GitHub Copilot and Amazon Q

## Remote Command Execution

### Quick Start

1. **Start RawrXD CLI:**
```powershell
.\build\bin-msvc\Release\RawrXD-CLI.exe
# Output: RawrXD CLI Instance [PID: 1234] [Port: 11434]
```

2. **Execute a command remotely:**
```bash
curl -X POST http://localhost:11434/api/v1/execute \
  -H "Content-Type: application/json" \
  -d '{"command": "help"}'
```

3. **Get command status:**
```bash
curl http://localhost:11434/api/v1/status?id=cmd_12345
```

## API Endpoints

### POST /api/v1/execute
Execute any CLI command remotely

**Request:**
```json
{
  "command": "help"
}
```

**Response:**
```json
{
  "command": "help",
  "status": "success",
  "result": "Available commands:\n  load <model>       - Load a model\n  analyze            - Analyze current project\n  ...",
  "timestamp": "2025-01-15T12:00:00Z"
}
```

### GET /api/v1/status
Track command execution status

**Query Parameters:**
- `id` - Command ID (optional)

**Response:**
```json
{
  "command_id": "cmd_12345",
  "status": "completed",
  "progress": 100,
  "output": "Command execution complete",
  "timestamps": {
    "created": "2025-01-15T12:00:00Z",
    "completed": "2025-01-15T12:00:05Z"
  }
}
```

## Available Commands

### System Commands

```bash
# Get help on available commands
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "help"}'

# Check server status
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "status"}'

# List available models
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "models"}'

# Get version information
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "version"}'
```

### Model Commands

```bash
# Load a model
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "load bigdaddyg"}'

# OR by number
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "load 1"}'
```

### Analysis Commands

```bash
# Analyze project
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "analyze"}'

# Search for code
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "search class Database"}'
```

### Chat Commands

```bash
# Send chat message to loaded model
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "chat Write a hello world function in Python"}'

# Generate text completion
curl -X POST http://localhost:11434/api/v1/execute \
  -d '{"command": "generate def factorial"}'
```

## Integration Examples

### Python - GitHub Copilot Integration

```python
import requests
import json
from typing import Dict, Any

class RawrXDCLI:
    def __init__(self, base_url: str = "http://localhost:11434"):
        self.base_url = base_url
    
    def execute_command(self, command: str) -> Dict[str, Any]:
        """Execute a CLI command and return results"""
        response = requests.post(
            f"{self.base_url}/api/v1/execute",
            json={"command": command},
            timeout=30
        )
        return response.json()
    
    def get_status(self, cmd_id: str = None) -> Dict[str, Any]:
        """Get status of a command"""
        params = {"id": cmd_id} if cmd_id else {}
        response = requests.get(
            f"{self.base_url}/api/v1/status",
            params=params,
            timeout=10
        )
        return response.json()
    
    def help(self) -> str:
        """Get available commands"""
        result = self.execute_command("help")
        return result.get("result", "No help available")
    
    def load_model(self, model: str) -> str:
        """Load a model"""
        result = self.execute_command(f"load {model}")
        return result.get("result", "Load failed")
    
    def analyze_project(self) -> str:
        """Analyze the current project"""
        result = self.execute_command("analyze")
        return result.get("result", "Analysis failed")
    
    def search_code(self, query: str) -> str:
        """Search for code patterns"""
        result = self.execute_command(f"search {query}")
        return result.get("result", "Search failed")
    
    def chat(self, message: str) -> str:
        """Send chat message to AI"""
        result = self.execute_command(f"chat {message}")
        return result.get("result", "Chat failed")
    
    def get_status_info(self) -> str:
        """Get server status"""
        result = self.execute_command("status")
        return result.get("result", "Status unavailable")

# Usage with GitHub Copilot
client = RawrXDCLI()

# Get available models
print(client.help())

# Load BigDaddyG
print(client.load_model("1"))  # Load first model

# Analyze project
analysis = client.analyze_project()
print(f"Project Analysis:\n{analysis}")

# Chat with model
response = client.chat("Write a sorting algorithm in Python")
print(f"AI Response:\n{response}")

# Search code
results = client.search_code("function main")
print(f"Search Results:\n{results}")
```

### JavaScript/Node.js - VS Code Extension

```javascript
const axios = require('axios');

class RawrXDCLI {
  constructor(baseUrl = 'http://localhost:11434') {
    this.baseUrl = baseUrl;
    this.client = axios.create({ timeout: 30000 });
  }

  async executeCommand(command) {
    try {
      const response = await this.client.post(
        `${this.baseUrl}/api/v1/execute`,
        { command }
      );
      return response.data;
    } catch (error) {
      console.error(`Command execution failed: ${error.message}`);
      throw error;
    }
  }

  async getStatus(cmdId = null) {
    try {
      const params = cmdId ? { id: cmdId } : {};
      const response = await this.client.get(
        `${this.baseUrl}/api/v1/status`,
        { params }
      );
      return response.data;
    } catch (error) {
      console.error(`Status check failed: ${error.message}`);
      throw error;
    }
  }

  async help() {
    const result = await this.executeCommand('help');
    return result.result;
  }

  async loadModel(model) {
    const result = await this.executeCommand(`load ${model}`);
    return result.result;
  }

  async analyzeProject() {
    const result = await this.executeCommand('analyze');
    return result.result;
  }

  async searchCode(query) {
    const result = await this.executeCommand(`search ${query}`);
    return result.result;
  }

  async chat(message) {
    const result = await this.executeCommand(`chat ${message}`);
    return result.result;
  }

  async status() {
    const result = await this.executeCommand('status');
    return result.result;
  }
}

// Usage in VS Code extension
const cli = new RawrXDCLI();

// Execute from within VS Code
vscode.commands.registerCommand('rawrxd.loadModel', async () => {
  const result = await cli.loadModel('1');
  vscode.window.showInformationMessage(result);
});

vscode.commands.registerCommand('rawrxd.analyzeProject', async () => {
  const analysis = await cli.analyzeProject();
  vscode.window.showInformationMessage(`Analysis:\n${analysis}`);
});

vscode.commands.registerCommand('rawrxd.chatWithAI', async () => {
  const input = await vscode.window.showInputBox({ prompt: 'Enter message' });
  if (input) {
    const response = await cli.chat(input);
    vscode.window.showInformationMessage(`Response:\n${response}`);
  }
});
```

### PowerShell - Amazon Q Integration

```powershell
# RawrXD CLI Command Execution Wrapper

function Invoke-RawrXDCommand {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Command,
        
        [string]$BaseUrl = "http://localhost:11434"
    )
    
    $payload = @{
        command = $Command
    } | ConvertTo-Json
    
    try {
        $response = Invoke-RestMethod `
            -Uri "$BaseUrl/api/v1/execute" `
            -Method Post `
            -ContentType "application/json" `
            -Body $payload `
            -ErrorAction Stop
        
        return @{
            success = $response.status -eq "success"
            result = $response.result
            command = $response.command
            timestamp = $response.timestamp
        }
    }
    catch {
        return @{
            success = $false
            error = $_.Exception.Message
        }
    }
}

function Get-RawrXDCommandStatus {
    param(
        [string]$CommandId = $null,
        [string]$BaseUrl = "http://localhost:11434"
    )
    
    $params = if ($CommandId) { "?id=$CommandId" } else { "" }
    
    try {
        $response = Invoke-RestMethod `
            -Uri "$BaseUrl/api/v1/status$params" `
            -Method Get `
            -ErrorAction Stop
        
        return $response
    }
    catch {
        Write-Error "Status check failed: $_"
    }
}

# Usage
$result = Invoke-RawrXDCommand -Command "help"
Write-Host "Help Output:`n$($result.result)"

$result = Invoke-RawrXDCommand -Command "load 1"
Write-Host "Model Loaded: $($result.result)"

$result = Invoke-RawrXDCommand -Command "analyze"
Write-Host "Project Analysis:`n$($result.result)"

$result = Invoke-RawrXDCommand -Command "chat Write a REST API in Python"
Write-Host "AI Response:`n$($result.result)"
```

## GitHub Copilot Integration

### Setup

1. **Configure VS Code:**
```json
{
  "github.copilot.advanced": {
    "debug.testOverrideProxyUrl": "http://localhost:11434",
    "debug.overrideEngine": "gpt-4",
    "debug.overrideExtensionPath": "..."
  }
}
```

2. **Use Copilot with RawrXD:**
- Open Copilot Chat (`Ctrl+Shift+I`)
- Commands are automatically routed to RawrXD CLI
- Use natural language to execute tasks:
  - "Analyze my project"
  - "Load the BigDaddyG model"
  - "Search for database functions"
  - "Generate a sorting algorithm"

## Amazon Q Integration

### Setup

1. **Configure AWS IDE:**
```bash
aws configure set aws_profile_for_codewhisperer default
```

2. **Set Custom Backend:**
- Go to AWS Toolkit Settings
- Custom Model Server: `http://localhost:11434`
- API Type: OpenAI Compatible

3. **Use Amazon Q:**
- Use Q Chat for code generation
- Commands routed through `/api/v1/execute`
- Full project context available

## Command Response Format

All commands return a consistent JSON structure:

```json
{
  "command": "the command that was executed",
  "status": "success or error",
  "result": "command output as string",
  "timestamp": "ISO 8601 timestamp",
  "metadata": {
    "execution_time_ms": 150,
    "lines_of_output": 25
  }
}
```

## Error Handling

**Connection Error:**
```python
try:
    result = client.execute_command("help")
except requests.ConnectionError:
    print("RawrXD CLI not running on port 11434")
```

**Command Error:**
```python
result = client.execute_command("unknown_command")
if result['status'] == 'success':
    print(result['result'])
else:
    print(f"Error: {result.get('error', 'Unknown error')}")
```

**Timeout Handling:**
```python
try:
    result = client.execute_command("analyze", timeout=60)
except requests.Timeout:
    # Check status later
    status = client.get_status(cmd_id)
    print(f"Command progress: {status['progress']}%")
```

## Performance Tips

1. **Batch Commands:**
```python
# Instead of multiple requests
for cmd in ["help", "status", "models"]:
    result = client.execute_command(cmd)

# Load model once, then use it
client.execute_command("load bigdaddyg")
client.execute_command("chat explain this code")
client.execute_command("chat generate a function")
```

2. **Async Execution:**
```python
# Fire-and-forget for long operations
response = requests.post(
    'http://localhost:11434/api/v1/execute',
    json={"command": "analyze"},
    timeout=5  # Don't wait for full response
)
cmd_id = response.json()['command_id']
# Check status later
```

3. **Connection Pooling:**
```python
# Reuse session for multiple requests
session = requests.Session()
result1 = session.post('.../execute', json={"command": "help"})
result2 = session.post('.../execute', json={"command": "models"})
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "Connection refused" | Start CLI: `RawrXD-CLI.exe` |
| "Unknown command" | Use `help` command to see available options |
| "No model loaded" | Execute `load 1` first |
| "Timeout" | Check server status with `/health` |
| "Rate limited" | Wait 1 minute (60 req/min limit) |

## Advanced Features

### Custom Command Handlers

Extend the CLI with custom commands:

```powershell
# Add custom analysis
Invoke-RawrXDCommand "analyze --format=csv --output=report.csv"

# Scheduled tasks
$job = Start-Job -ScriptBlock {
    for ($i = 0; $i -lt 10; $i++) {
        Invoke-RawrXDCommand "status"
        Start-Sleep -Seconds 10
    }
}
```

### Monitoring

```bash
# Monitor server health
while true; do
  curl http://localhost:11434/health | jq '.uptime_seconds'
  sleep 10
done
```

## Support

- **Documentation**: See `API_EXTERNAL_TOOLS_INTEGRATION.md`
- **Test Script**: Run `test-external-api.ps1`
- **Repository**: https://github.com/ItsMehRAWRXD/RawrXD
- **Issues**: https://github.com/ItsMehRAWRXD/RawrXD/issues
